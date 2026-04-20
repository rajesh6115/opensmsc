#include "smpp_client_handler.hpp"

#include "logger.hpp"
#include "smpp_handler.hpp"
#include "smpp_service_manager.hpp"
#include <smpp.hpp>

namespace {
    constexpr uint32_t CMD_BIND_RECEIVER = 0x00000001;
    constexpr uint32_t CMD_BIND_TRANSMITTER = 0x00000002;
    constexpr uint32_t CMD_BIND_TRANSCEIVER = 0x00000009;
    constexpr uint32_t CMD_UNBIND = 0x00000006;
    constexpr uint32_t CMD_ENQUIRE_LINK = 0x0000000F;

    uint32_t ntoh32(const uint8_t* ptr) {
        return ((uint32_t)ptr[0] << 24) | ((uint32_t)ptr[1] << 16) |
               ((uint32_t)ptr[2] << 8) | ((uint32_t)ptr[3]);
    }
}

SmppClientHandler::SmppClientHandler(
    asio::ip::tcp::socket socket,
    std::string session_id,
    std::shared_ptr<SmppServiceManager> service_mgr)
    : strand_(asio::make_strand(socket.get_executor())),
      socket_(std::move(socket)),
      session_id_(session_id),
      service_mgr_(service_mgr)
{
    LOG_INFO("SmppClientHandler", "created session_id={}", session_id_);
}

void SmppClientHandler::start()
{
    asio::dispatch(strand_, [self = shared_from_this()]() {
        self->do_read();
    });
}

void SmppClientHandler::do_read()
{
    socket_.async_read_some(
        asio::buffer(read_buf_),
        asio::bind_executor(strand_, [self = shared_from_this()](
            const asio::error_code& ec, std::size_t bytes_transferred) {
            self->on_read(ec, bytes_transferred);
        }));
}

void SmppClientHandler::on_read(const asio::error_code& ec, std::size_t bytes_transferred)
{
    if (ec) {
        close("read error: " + ec.message());
        return;
    }

    framer_.feed_bytes(read_buf_.data(), bytes_transferred);

    while (framer_.get_state() == SmppPduFramer::State::PDU_COMPLETE) {
        dispatch_pdu(framer_.get_pdu());
        framer_.reset();
    }

    do_read();
}

void SmppClientHandler::dispatch_pdu(const std::vector<uint8_t>& full_pdu)
{
    if (full_pdu.size() < 16) {
        LOG_ERROR("SmppClientHandler", "invalid PDU length={}", full_pdu.size());
        return;
    }

    uint32_t command_id = ntoh32(full_pdu.data() + 4);
    uint32_t sequence_number = ntoh32(full_pdu.data() + 12);

    LOG_INFO("SmppClientHandler", "received PDU command_id=0x{:08x} seq={} size={}",
             command_id, sequence_number, full_pdu.size());

    switch (command_id) {
    case CMD_BIND_RECEIVER:
    case CMD_BIND_TRANSMITTER:
    case CMD_BIND_TRANSCEIVER:
        handle_bind(command_id, full_pdu.data(), full_pdu.size(), sequence_number);
        break;

    case CMD_UNBIND:
        handle_unbind(sequence_number);
        break;

    case CMD_ENQUIRE_LINK:
        handle_enquire_link(sequence_number);
        break;

    default:
        handle_unknown(command_id, sequence_number);
        break;
    }
}

void SmppClientHandler::handle_bind(
    uint32_t command_id,
    const uint8_t* raw_pdu,
    std::size_t len,
    uint32_t sequence_number)
{
    try {
        auto resp = SmppHandler::build_bind_resp(command_id, raw_pdu, len, sequence_number, session_);
        send_pdu(std::move(resp));
    } catch (const std::exception& ex) {
        LOG_ERROR("SmppClientHandler", "exception in handle_bind: {}", ex.what());
        Smpp::GenericNack error_resp;
        error_resp.command_status(0x00000003);  // ESME_RINVCMDID
        error_resp.sequence_number(sequence_number);
        send_pdu(SmppHandler::build_generic_nack(0x00000003, sequence_number));
    }
}

void SmppClientHandler::handle_unbind(uint32_t sequence_number)
{
    if (!session_.is_bound()) {
        LOG_WARN("SmppClientHandler", "UNBIND but not bound");
        send_pdu(SmppHandler::build_unbind_resp(0x0000000D, sequence_number));
        return;
    }

    LOG_INFO("SmppClientHandler", "UNBIND");
    close_after_write_ = true;
    send_pdu(SmppHandler::build_unbind_resp(0x00000000, sequence_number));
    session_.set_state(SmppSession::State::UNBOUND);
}

void SmppClientHandler::handle_enquire_link(uint32_t sequence_number)
{
    if (!session_.is_bound()) {
        LOG_WARN("SmppClientHandler", "ENQUIRE_LINK but not bound");
        send_pdu(SmppHandler::build_enquire_link_resp(0x0000000D, sequence_number));
        return;
    }

    LOG_INFO("SmppClientHandler", "ENQUIRE_LINK keep-alive");
    send_pdu(SmppHandler::build_enquire_link_resp(0x00000000, sequence_number));
}

void SmppClientHandler::handle_unknown(uint32_t command_id, uint32_t sequence_number)
{
    LOG_WARN("SmppClientHandler", "unknown command 0x{:08x}", command_id);
    send_pdu(SmppHandler::build_generic_nack(0x00000003, sequence_number));
}

void SmppClientHandler::send_pdu(std::vector<uint8_t> pdu)
{
    asio::dispatch(strand_,
        [self = shared_from_this(), pdu = std::move(pdu)]() mutable {
            self->enqueue_pdu(std::move(pdu));
        });
}

void SmppClientHandler::enqueue_pdu(std::vector<uint8_t> pdu)
{
    write_queue_.push_back(std::move(pdu));
    if (!write_in_progress_) {
        write_in_progress_ = true;
        do_write();
    }
}

void SmppClientHandler::do_write()
{
    if (write_queue_.empty()) {
        write_in_progress_ = false;
        return;
    }

    const auto& pdu = write_queue_.front();
    asio::async_write(socket_, asio::buffer(pdu),
        asio::bind_executor(strand_,
            [self = shared_from_this()](const asio::error_code& ec, std::size_t bytes) {
                self->on_write(ec, bytes);
            }));
}

void SmppClientHandler::on_write(const asio::error_code& ec, std::size_t bytes_transferred)
{
    if (ec) {
        close("write error: " + ec.message());
        return;
    }

    write_queue_.pop_front();

    if (close_after_write_) {
        close("UNBIND complete");
        return;
    }

    if (!write_queue_.empty()) {
        do_write();
    } else {
        write_in_progress_ = false;
    }
}

void SmppClientHandler::close(const std::string& reason)
{
    LOG_INFO("SmppClientHandler", "closing session_id={} reason={}", session_id_, reason);
    service_mgr_->stop_authenticator(session_id_);

    asio::error_code ec;
    socket_.close(ec);
}
