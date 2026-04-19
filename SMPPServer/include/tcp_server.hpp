#pragma once

#include <asio.hpp>
#include <memory>
#include <string>
#include <vector>

class IpValidator;
class SmppServiceManager;
class SmppSession;

// Async TCP server built on standalone ASIO.
//
// Lifecycle per accepted SMPP connection:
//   1. Extract remote IP from the accepted socket.
//   2. Validate IP against the whitelist (IpValidator).
//      - Rejected: close connection.
//   3. Create SmppSession (tracks connection state).
//   4. Start async PDU reading loop (read header, then body).
//   5. Dispatch each PDU to SmppHandler for processing.
//   6. Keep connection open until UNBIND or error.
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
