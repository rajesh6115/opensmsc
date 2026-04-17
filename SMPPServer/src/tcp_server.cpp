#include "tcp_server.hpp"

#include "ip_validator.hpp"
#include "session_id.hpp"
#include "smpp_service_manager.hpp"

#include <asio.hpp>

#include <iostream>
#include <string>

// ── TcpServer ─────────────────────────────────────────────────────────────────

TcpServer::TcpServer(asio::io_context&                    io_ctx,
                     uint16_t                              port,
                     std::shared_ptr<IpValidator>          validator,
                     std::shared_ptr<SmppServiceManager>   service_mgr)
    : acceptor_(io_ctx,
                asio::ip::tcp::endpoint(asio::ip::tcp::v6(), port),
                /* reuse_addr */ true)
    , validator_(std::move(validator))
    , service_mgr_(std::move(service_mgr))
{
    // IPV6_V6ONLY = 0 → dual-stack: accept both IPv4 and IPv6 on one socket.
    acceptor_.set_option(asio::ip::v6_only(false));

    std::cout << "[INFO] TcpServer: listening on [::]:" << port << "\n";
    do_accept();
}

uint16_t TcpServer::port() const
{
    return acceptor_.local_endpoint().port();
}

// ── Internal ──────────────────────────────────────────────────────────────────

void TcpServer::do_accept()
{
    acceptor_.async_accept(
        [this](asio::error_code ec, asio::ip::tcp::socket socket)
        {
            if (ec) {
                if (ec != asio::error::operation_aborted) {
                    std::cerr << "[ERROR] TcpServer: accept error: " << ec.message() << "\n";
                }
            } else {
                handle_connection(std::move(socket));
            }
            do_accept();   // queue next accept immediately
        });
}

void TcpServer::handle_connection(asio::ip::tcp::socket socket)
{
    asio::error_code ec;
    auto remote_ep  = socket.remote_endpoint(ec);
    if (ec) {
        std::cerr << "[WARN] TcpServer: could not read remote endpoint: "
                  << ec.message() << "\n";
        return;
    }

    const std::string client_ip = remote_ep.address().to_string();

    std::cout << "[INFO] TcpServer: connection from " << client_ip
              << ":" << remote_ep.port() << "\n";

    // ── 1. Source-IP validation ───────────────────────────────────────────────
    if (!validator_->is_allowed(client_ip)) {
        std::cout << "[WARN] TcpServer: IP " << client_ip
                  << " is NOT in the whitelist — rejecting\n";
        const std::string resp = "ERROR IP_NOT_ALLOWED\n";
        asio::write(socket, asio::buffer(resp), ec);
        return;   // socket destructs → connection closed
    }

    std::cout << "[INFO] TcpServer: IP " << client_ip << " is allowed\n";

    // ── 2. Generate session ID ───────────────────────────────────────────────
    const std::string session_id = generate_session_id();
    std::cout << "[INFO] TcpServer: session_id=" << session_id << "\n";

    // ── 3. Start SMPPAuthenticator@<session_id>.service ──────────────────────
    if (!service_mgr_->start_authenticator(session_id, client_ip)) {
        const std::string resp = "ERROR SERVICE_START_FAILED\n";
        asio::write(socket, asio::buffer(resp), ec);
        return;
    }

    // ── 4. Acknowledge to the client ─────────────────────────────────────────
    const std::string resp = "OK " + session_id + "\n";
    asio::write(socket, asio::buffer(resp), ec);
    if (ec) {
        std::cerr << "[WARN] TcpServer: write to " << client_ip
                  << " failed: " << ec.message() << "\n";
    }
    // Socket goes out of scope here → connection closes cleanly.
}
