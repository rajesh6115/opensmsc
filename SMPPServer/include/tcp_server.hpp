#pragma once

#include <asio.hpp>
#include <memory>
#include <cstdint>
#include <cstring>
#include "smpp_handler.hpp"

class IpValidator;
class SmppServiceManager;
class SmppSession;

// Utility: convert network byte order (big-endian) uint32_t to host byte order
inline uint32_t ntoh32(const uint8_t* ptr) {
    return (static_cast<uint32_t>(ptr[0]) << 24) |
           (static_cast<uint32_t>(ptr[1]) << 16) |
           (static_cast<uint32_t>(ptr[2]) << 8) |
           (static_cast<uint32_t>(ptr[3]));
}

// Pure SMPP TCP acceptor. Delegates per-client handling to SmppClientHandler.
// - Accepts SMPP client connections on a port
// - Validates client IP against whitelist
// - Creates SmppClientHandler to own the socket and drive the async read loop
class TcpServer
{
public:
    TcpServer(asio::io_context&                    io_ctx,
              uint16_t                              port,
              std::shared_ptr<IpValidator>          validator,
              std::shared_ptr<SmppServiceManager>   service_mgr);

    uint16_t port() const;

private:
    void do_accept();
    void handle_connection(std::shared_ptr<asio::ip::tcp::socket> socket);
    void read_pdu_header(std::shared_ptr<asio::ip::tcp::socket> socket,
                        std::shared_ptr<SmppSession> session);
    void on_header_read(std::shared_ptr<asio::ip::tcp::socket> socket,
                       std::shared_ptr<SmppSession> session,
                       const asio::error_code& ec,
                       std::size_t bytes_transferred,
                       std::shared_ptr<std::vector<uint8_t>> header_buffer);
    void read_pdu_body(std::shared_ptr<asio::ip::tcp::socket> socket,
                      std::shared_ptr<SmppSession> session,
                      std::shared_ptr<std::vector<uint8_t>> header_buffer,
                      uint32_t body_length);
    void on_body_read(std::shared_ptr<asio::ip::tcp::socket> socket,
                     std::shared_ptr<SmppSession> session,
                     std::shared_ptr<std::vector<uint8_t>> header_buffer,
                     std::shared_ptr<std::vector<uint8_t>> body_buffer,
                     const asio::error_code& ec,
                     std::size_t bytes_transferred);

    asio::ip::tcp::acceptor              acceptor_;
    std::shared_ptr<IpValidator>         validator_;
    std::shared_ptr<SmppServiceManager>  service_mgr_;
};
