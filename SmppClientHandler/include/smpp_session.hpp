#pragma once

#include "generated/IClientHandler_adaptor.hpp"
#include "smpp_pdu.hpp"

#include <asio.hpp>
#include <asio/posix/stream_descriptor.hpp>
#include <sdbus-c++/sdbus-c++.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

enum class SessionState { OPEN, BOUND_TX, BOUND_RX, BOUND_TRX, UNBOUND };

inline const char* state_name(SessionState s) {
    switch (s) {
        case SessionState::OPEN:      return "OPEN";
        case SessionState::BOUND_TX:  return "BOUND_TX";
        case SessionState::BOUND_RX:  return "BOUND_RX";
        case SessionState::BOUND_TRX: return "BOUND_TRX";
        case SessionState::UNBOUND:   return "UNBOUND";
    }
    return "UNKNOWN";
}

class SmppSession : public com::telecom::smpp::IClientHandler_adaptor {
public:
    // enquire_link_interval_sec=0 disables the keepalive (useful in tests)
    // max_submit_per_sec = 0 disables throttling
    SmppSession(asio::io_context&   io_ctx,
                int                 socket_fd,
                const std::string&  uuid,
                const std::string&  client_ip,
                sdbus::IObject&     dbus_obj,
                sdbus::IConnection& dbus_conn,
                const std::string&  server_svc,
                const std::string&  auth_svc = "com.telecom.smpp.Auth",
                unsigned            enquire_link_interval_sec = 60,
                unsigned            enquire_link_timeout_sec  = 10,
                unsigned            max_submit_per_sec        = 0);

    void start();

private:
    std::tuple<std::string,std::string,std::string,std::string,uint64_t>
        GetSessionInfo() override;
    void Disconnect(const std::string& reason) override;
    uint32_t DeliverSm(const std::string& src_addr,
                       const std::string& dst_addr,
                       const std::string& short_message) override;

    void read_header();
    void on_header(const asio::error_code& ec, std::size_t);
    void read_body(smpp::Header hdr);
    void on_body(const asio::error_code& ec, smpp::Header hdr,
                 std::shared_ptr<std::vector<uint8_t>> body);

    void dispatch(const smpp::Header& hdr, const std::vector<uint8_t>& body);
    void handle_bind(const smpp::Header& hdr, const std::vector<uint8_t>& body);
    void handle_unbind(const smpp::Header& hdr);
    void handle_enquire_link(const smpp::Header& hdr);
    void handle_submit_sm(const smpp::Header& hdr, const std::vector<uint8_t>& body);
    void handle_deliver_sm_resp(const smpp::Header& hdr);
    std::string next_message_id();
    uint32_t    next_deliver_seq();
    bool        throttle_check();  // returns true if request should be allowed

    void send_pdu(std::vector<uint8_t> pdu);
    void do_disconnect(const std::string& reason);
    void update_session_details();

    // keepalive
    void schedule_enquire_link();
    void on_enquire_link_timer(const asio::error_code& ec);
    void handle_enquire_link_resp(const smpp::Header& hdr);
    void arm_enquire_link_timeout();
    void on_enquire_link_timeout(const asio::error_code& ec);

    asio::posix::stream_descriptor      socket_;
    std::string                          uuid_;
    std::string                          client_ip_;
    std::string                          system_id_;
    SessionState                         state_{SessionState::OPEN};
    uint64_t                             bind_time_{0};
    mutable std::mutex                   state_mutex_;

    sdbus::IConnection&                  dbus_conn_;
    std::unique_ptr<sdbus::IProxy>       server_proxy_;
    std::string                          server_svc_;
    std::string                          auth_svc_;

    // keepalive timers
    unsigned                             el_interval_sec_;
    unsigned                             el_timeout_sec_;
    uint32_t                             el_seq_{0};
    bool                                 el_pending_{false};
    asio::steady_timer                   el_timer_;
    asio::steady_timer                   el_timeout_timer_;

    std::atomic<uint32_t>                msg_seq_{0};
    std::atomic<uint32_t>                deliver_seq_{0x20000000u};

    // Token-bucket rate limiter for submit_sm
    unsigned                             max_submit_per_sec_{0};
    double                               tb_tokens_{0.0};
    std::chrono::steady_clock::time_point tb_last_refill_{};
    std::string                          msg_id_prefix_;  // first 8 chars of uuid
    std::vector<uint8_t>                 header_buf_;
};
