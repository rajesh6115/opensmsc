#pragma once

#include <asio.hpp>
#include <memory>
#include <string>

class IpValidator;
class SmppServiceManager;

// Async TCP server built on standalone ASIO.
//
// Lifecycle per accepted connection:
//   1. Extract remote IP from the accepted socket.
//   2. Validate IP against the whitelist (IpValidator).
//      - Rejected: send "ERROR IP_NOT_ALLOWED\n" and close.
//   3. Generate a unique session ID (session_id.hpp).
//   4. Start SMPPAuthenticator@<session_id>.service via SmppServiceManager.
//      - Service start failed: send "ERROR SERVICE_START_FAILED\n" and close.
//   5. Send "OK <session_id>\n" to the client and close the control socket.
class TcpServer
{
public:
    TcpServer(asio::io_context&                    io_ctx,
              uint16_t                              port,
              std::shared_ptr<IpValidator>          validator,
              std::shared_ptr<SmppServiceManager>   service_mgr);

    // Listening port currently bound.
    uint16_t port() const;

private:
    void do_accept();
    void handle_connection(asio::ip::tcp::socket socket);

    asio::ip::tcp::acceptor              acceptor_;
    std::shared_ptr<IpValidator>         validator_;
    std::shared_ptr<SmppServiceManager>  service_mgr_;
};
