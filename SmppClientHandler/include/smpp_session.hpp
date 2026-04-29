#pragma once

#include "generated/IClientHandler_adaptor.hpp"
#include "smpp_pdu.hpp"

#include <asio.hpp>
#include <asio/posix/stream_descriptor.hpp>
#include <sdbus-c++/sdbus-c++.h>
#include <spdlog/spdlog.h>

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
    SmppSession(asio::io_context&   io_ctx,
                int                 socket_fd,
                const std::string&  uuid,
                const std::string&  client_ip,
                sdbus::IObject&     dbus_obj,
                sdbus::IConnection& dbus_conn,
                const std::string&  server_svc,
                const std::string&  auth_svc = "com.telecom.smpp.Auth");

    void start();

private:
    std::tuple<std::string,std::string,std::string,std::string,uint64_t>
        GetSessionInfo() override;
    void Disconnect(const std::string& reason) override;

    void read_header();
    void on_header(const asio::error_code& ec, std::size_t);
    void read_body(smpp::Header hdr);
    void on_body(const asio::error_code& ec, smpp::Header hdr,
                 std::shared_ptr<std::vector<uint8_t>> body);

    void dispatch(const smpp::Header& hdr, const std::vector<uint8_t>& body);
    void handle_bind(const smpp::Header& hdr, const std::vector<uint8_t>& body);
    void handle_unbind(const smpp::Header& hdr);
    void handle_enquire_link(const smpp::Header& hdr);
    void handle_submit_sm(const smpp::Header& hdr);

    void send_pdu(std::vector<uint8_t> pdu);
    void do_disconnect(const std::string& reason);
    void update_session_details();

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

    std::vector<uint8_t>                 header_buf_;
};
