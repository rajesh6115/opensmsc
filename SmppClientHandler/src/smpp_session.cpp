#include "smpp_session.hpp"

#include <chrono>
#include <spdlog/spdlog.h>
#include <sstream>
#include <iomanip>

static constexpr const char* ISERVER_IFACE = "com.telecom.smpp.IServer";
static constexpr const char* ISERVER_PATH  = "/com/telecom/smpp/server";
static constexpr const char* IAUTH_IFACE   = "com.telecom.smpp.IAuth";
static constexpr const char* IAUTH_PATH    = "/com/telecom/smpp/auth";

SmppSession::SmppSession(asio::io_context&   io_ctx,
                         int                 socket_fd,
                         const std::string&  uuid,
                         const std::string&  client_ip,
                         sdbus::IObject&     dbus_obj,
                         sdbus::IConnection& dbus_conn,
                         const std::string&  server_svc,
                         const std::string&  auth_svc,
                         unsigned            enquire_link_interval_sec,
                         unsigned            enquire_link_timeout_sec,
                         unsigned            max_submit_per_sec)
    : IClientHandler_adaptor(dbus_obj)
    , socket_(io_ctx, socket_fd)
    , uuid_(uuid)
    , client_ip_(client_ip)
    , dbus_conn_(dbus_conn)
    , server_svc_(server_svc)
    , auth_svc_(auth_svc)
    , el_interval_sec_(enquire_link_interval_sec)
    , el_timeout_sec_(enquire_link_timeout_sec)
    , el_timer_(io_ctx)
    , el_timeout_timer_(io_ctx)
    , max_submit_per_sec_(max_submit_per_sec)
    , tb_tokens_(static_cast<double>(max_submit_per_sec))
    , tb_last_refill_(std::chrono::steady_clock::now())
    , msg_id_prefix_(uuid.size() >= 8 ? uuid.substr(0, 8) : uuid)
    , header_buf_(16)
{
    server_proxy_ = sdbus::createProxy(dbus_conn_, server_svc_, ISERVER_PATH);
    spdlog::info("[SmppSession] created uuid={} ip={}", uuid_, client_ip_);
}

void SmppSession::start()
{
    read_header();
    if (el_interval_sec_ > 0) schedule_enquire_link();
}

std::tuple<std::string,std::string,std::string,std::string,uint64_t>
SmppSession::GetSessionInfo()
{
    std::lock_guard<std::mutex> lk(state_mutex_);
    return {uuid_, client_ip_, system_id_, std::string(state_name(state_)), bind_time_};
}

void SmppSession::Disconnect(const std::string& reason)
{
    asio::post(socket_.get_executor(), [this, reason]{ do_disconnect(reason); });
}

void SmppSession::read_header()
{
    asio::async_read(socket_, asio::buffer(header_buf_, 16),
        [this](const asio::error_code& ec, std::size_t n){ on_header(ec, n); });
}

void SmppSession::on_header(const asio::error_code& ec, std::size_t)
{
    if (ec) {
        if (ec != asio::error::eof && ec != asio::error::connection_reset
            && ec.value() != EBADF)
            spdlog::warn("[SmppSession] read header: {}", ec.message());
        do_disconnect("socket closed: " + ec.message());
        return;
    }
    auto hdr = smpp::decode_header(header_buf_.data());
    if (hdr.command_length < 16) { do_disconnect("invalid PDU length"); return; }
    read_body(hdr);
}

void SmppSession::read_body(smpp::Header hdr)
{
    const uint32_t body_len = hdr.command_length - 16;
    auto body = std::make_shared<std::vector<uint8_t>>(body_len);
    if (body_len == 0) {
        dispatch(hdr, *body);
        read_header();
        return;
    }
    asio::async_read(socket_, asio::buffer(*body, body_len),
        [this, hdr, body](const asio::error_code& ec, std::size_t) {
            on_body(ec, hdr, body);
        });
}

void SmppSession::on_body(const asio::error_code& ec, smpp::Header hdr,
                          std::shared_ptr<std::vector<uint8_t>> body)
{
    if (ec) { do_disconnect("body read: " + ec.message()); return; }
    dispatch(hdr, *body);
    read_header();
}

void SmppSession::dispatch(const smpp::Header& hdr, const std::vector<uint8_t>& body)
{
    switch (hdr.command_id) {
        case smpp::BIND_TRANSMITTER:
        case smpp::BIND_RECEIVER:
        case smpp::BIND_TRANSCEIVER:
            handle_bind(hdr, body); break;
        case smpp::UNBIND:
            handle_unbind(hdr); break;
        case smpp::ENQUIRE_LINK:
            handle_enquire_link(hdr); break;
        case smpp::SUBMIT_SM:
            handle_submit_sm(hdr, body); break;
        case smpp::ENQUIRE_LINK_RESP:
            handle_enquire_link_resp(hdr); break;
        case smpp::DELIVER_SM_RESP:
            handle_deliver_sm_resp(hdr); break;
        default:
            send_pdu(smpp::make_response(smpp::GENERIC_NACK,
                smpp::ESME_RINVCMDID, hdr.sequence_number));
    }
}

void SmppSession::handle_bind(const smpp::Header& hdr, const std::vector<uint8_t>& body)
{
    {
        std::lock_guard<std::mutex> lk(state_mutex_);
        if (state_ != SessionState::OPEN) {
            send_pdu(smpp::make_response(smpp::bind_resp_id(hdr.command_id),
                smpp::ESME_RALYBND, hdr.sequence_number));
            return;
        }
    }
    size_t off = 0;
    std::string sid  = smpp::read_cstr(body, off);
    std::string pass = smpp::read_cstr(body, off);

    bool auth_ok = false;
    uint32_t err_code = smpp::ESME_RINVSYSID;
    try {
        auto auth_proxy = sdbus::createProxy(dbus_conn_, auth_svc_, IAUTH_PATH);
        auth_proxy->callMethod("Authenticate").onInterface(IAUTH_IFACE)
            .withArguments(uuid_, sid, pass, client_ip_)
            .storeResultsTo(auth_ok, err_code);
    } catch (const std::exception& e) {
        spdlog::error("[SmppSession] IAuth call failed: {}", e.what());
    }

    if (!auth_ok) {
        send_pdu(smpp::make_response(smpp::bind_resp_id(hdr.command_id),
            err_code, hdr.sequence_number));
        do_disconnect("auth failed");
        return;
    }

    {
        std::lock_guard<std::mutex> lk(state_mutex_);
        system_id_ = sid;
        switch (hdr.command_id) {
            case smpp::BIND_TRANSMITTER: state_ = SessionState::BOUND_TX;  break;
            case smpp::BIND_RECEIVER:    state_ = SessionState::BOUND_RX;  break;
            case smpp::BIND_TRANSCEIVER: state_ = SessionState::BOUND_TRX; break;
        }
        bind_time_ = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
    }

    send_pdu(smpp::make_response(smpp::bind_resp_id(hdr.command_id),
        smpp::ESME_ROK, hdr.sequence_number, "smpp-server"));
    update_session_details();
    spdlog::info("[SmppSession] bound uuid={} system_id={}", uuid_, sid);
}

void SmppSession::handle_unbind(const smpp::Header& hdr)
{
    send_pdu(smpp::make_response(smpp::UNBIND_RESP, smpp::ESME_ROK, hdr.sequence_number));
    do_disconnect("unbind");
}

void SmppSession::handle_enquire_link(const smpp::Header& hdr)
{
    send_pdu(smpp::make_response(smpp::ENQUIRE_LINK_RESP, smpp::ESME_ROK, hdr.sequence_number));
}

void SmppSession::handle_submit_sm(const smpp::Header& hdr, const std::vector<uint8_t>&)
{
    uint32_t status;
    std::string msg_id;
    {
        std::lock_guard<std::mutex> lk(state_mutex_);
        status = (state_ == SessionState::BOUND_TX || state_ == SessionState::BOUND_TRX)
            ? smpp::ESME_ROK : smpp::ESME_RINVBNDSTS;
    }
    if (status == smpp::ESME_ROK && !throttle_check()) {
        spdlog::warn("[SmppSession] throttled uuid={}", uuid_);
        status = smpp::ESME_RTHROTTLED;
    }
    if (status == smpp::ESME_ROK) msg_id = next_message_id();
    send_pdu(smpp::make_response(smpp::SUBMIT_SM_RESP, status, hdr.sequence_number, msg_id));
    if (!msg_id.empty()) {
        spdlog::debug("[SmppSession] submit_sm accepted msg_id={} uuid={}", msg_id, uuid_);
        try {
            server_proxy_->callMethod("RouteMessage").onInterface("com.telecom.smpp.IServer")
                .withArguments(std::string{}, system_id_, std::string{}, msg_id);
        } catch (const std::exception& e) {
            spdlog::warn("[SmppSession] RouteMessage failed: {}", e.what());
        }
    }
}

std::string SmppSession::next_message_id()
{
    uint32_t seq = msg_seq_.fetch_add(1, std::memory_order_relaxed) + 1;
    std::ostringstream oss;
    oss << msg_id_prefix_ << std::hex << std::setw(8) << std::setfill('0') << seq;
    return oss.str();
}

void SmppSession::send_pdu(std::vector<uint8_t> pdu)
{
    auto buf = std::make_shared<std::vector<uint8_t>>(std::move(pdu));
    asio::async_write(socket_, asio::buffer(*buf),
        [buf](const asio::error_code& ec, std::size_t) {
            if (ec) spdlog::warn("[SmppSession] send_pdu: {}", ec.message());
        });
}

void SmppSession::do_disconnect(const std::string& reason)
{
    {
        std::lock_guard<std::mutex> lk(state_mutex_);
        if (state_ == SessionState::UNBOUND) return;
        state_ = SessionState::UNBOUND;
    }
    spdlog::info("[SmppSession] disconnect uuid={} reason={}", uuid_, reason);
    asio::error_code ec;
    socket_.close(ec);
    emitSessionExited(uuid_, reason);
}

void SmppSession::update_session_details()
{
    std::string sid, st;
    {
        std::lock_guard<std::mutex> lk(state_mutex_);
        sid = system_id_;
        st  = state_name(state_);
    }
    try {
        server_proxy_->callMethod("UpdateSessionDetails").onInterface(ISERVER_IFACE)
            .withArguments(uuid_, sid, st);
    } catch (const std::exception& e) {
        spdlog::warn("[SmppSession] UpdateSessionDetails: {}", e.what());
    }
}

uint32_t SmppSession::DeliverSm(const std::string& src_addr,
                                const std::string& dst_addr,
                                const std::string& short_message)
{
    {
        std::lock_guard<std::mutex> lk(state_mutex_);
        if (state_ != SessionState::BOUND_RX && state_ != SessionState::BOUND_TRX) {
            throw sdbus::Error("com.telecom.smpp.Error.InvalidBindState",
                               "session is not BOUND_RX or BOUND_TRX");
        }
    }
    uint32_t seq = next_deliver_seq();
    send_pdu(smpp::make_deliver_sm(seq, src_addr, dst_addr, short_message));
    spdlog::info("[SmppSession] deliver_sm seq={} src={} dst={} uuid={}",
                 seq, src_addr, dst_addr, uuid_);
    return seq;
}

void SmppSession::handle_deliver_sm_resp(const smpp::Header& hdr)
{
    spdlog::debug("[SmppSession] deliver_sm_resp seq={} status={:#010x} uuid={}",
                  hdr.sequence_number, hdr.command_status, uuid_);
    if (hdr.command_status != smpp::ESME_ROK)
        spdlog::warn("[SmppSession] deliver_sm_resp error status={:#010x} seq={} uuid={}",
                     hdr.command_status, hdr.sequence_number, uuid_);
}

uint32_t SmppSession::next_deliver_seq()
{
    return deliver_seq_.fetch_add(1, std::memory_order_relaxed);
}

bool SmppSession::throttle_check()
{
    if (max_submit_per_sec_ == 0) return true;  // unlimited

    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - tb_last_refill_).count();
    tb_last_refill_ = now;

    // Refill tokens proportional to elapsed time, capped at bucket size
    tb_tokens_ += elapsed * static_cast<double>(max_submit_per_sec_);
    if (tb_tokens_ > static_cast<double>(max_submit_per_sec_))
        tb_tokens_ = static_cast<double>(max_submit_per_sec_);

    if (tb_tokens_ >= 1.0) {
        tb_tokens_ -= 1.0;
        return true;
    }
    return false;
}

// ── Enquire-link keepalive ────────────────────────────────────────────────────

void SmppSession::schedule_enquire_link()
{
    el_timer_.expires_after(std::chrono::seconds(el_interval_sec_));
    el_timer_.async_wait([this](const asio::error_code& ec){
        on_enquire_link_timer(ec);
    });
}

void SmppSession::on_enquire_link_timer(const asio::error_code& ec)
{
    if (ec || el_pending_) return;
    {
        std::lock_guard<std::mutex> lk(state_mutex_);
        if (state_ == SessionState::UNBOUND) return;
    }
    el_seq_ = (el_seq_ == 0) ? 0x10000000u : el_seq_ + 1;
    el_pending_ = true;
    send_pdu(smpp::make_response(smpp::ENQUIRE_LINK, smpp::ESME_ROK, el_seq_));
    spdlog::debug("[SmppSession] sent enquire_link seq={}", el_seq_);
    arm_enquire_link_timeout();
}

void SmppSession::arm_enquire_link_timeout()
{
    el_timeout_timer_.expires_after(std::chrono::seconds(el_timeout_sec_));
    el_timeout_timer_.async_wait([this](const asio::error_code& ec){
        on_enquire_link_timeout(ec);
    });
}

void SmppSession::handle_enquire_link_resp(const smpp::Header& hdr)
{
    if (hdr.sequence_number == el_seq_ && el_pending_) {
        el_pending_ = false;
        el_timeout_timer_.cancel();
        spdlog::debug("[SmppSession] enquire_link_resp received seq={}", hdr.sequence_number);
        schedule_enquire_link();
    }
}

void SmppSession::on_enquire_link_timeout(const asio::error_code& ec)
{
    if (ec) return;  // cancelled
    if (!el_pending_) return;
    spdlog::warn("[SmppSession] enquire_link timeout uuid={}", uuid_);
    do_disconnect("enquire_link timeout");
}
