#include "tcp_server.hpp"

#include "ip_validator.hpp"
#include "logger.hpp"
#include "session_id.hpp"
#include "smpp_client_handler.hpp"
#include "smpp_service_manager.hpp"

#include <asio.hpp>
#include <memory>

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
    try {
        asio::error_code ec;
        acceptor_.set_option(asio::ip::v6_only(false), ec);
        if (ec) {
            LOG_WARN("TcpServer", "could not set IPv6-only option: {}", ec.message());
        }
    } catch (const std::exception& e) {
        LOG_WARN("TcpServer", "exception setting IPv6-only option: {}", e.what());
    }
    LOG_INFO("TcpServer", "listening on [::]:{}",  port);
    do_accept();
}

uint16_t TcpServer::port() const
{
    return acceptor_.local_endpoint().port();
}

// ── Internal ──────────────────────────────────────────────────────────────────

void TcpServer::do_accept()
{
    auto socket = std::make_shared<asio::ip::tcp::socket>(acceptor_.get_executor());
    acceptor_.async_accept(*socket, [this, socket](const asio::error_code& ec) {
        if (ec) {
            if (ec != asio::error::operation_aborted) {
                LOG_ERROR("TcpServer", "accept error: {}", ec.message());
            }
        } else {
            handle_connection(socket);
        }
        do_accept();
    });
}

void TcpServer::handle_connection(std::shared_ptr<asio::ip::tcp::socket> socket)
{
    asio::error_code ec;
    auto remote_ep = socket->remote_endpoint(ec);
    if (ec) {
        LOG_WARN("TcpServer", "could not read remote endpoint: {}", ec.message());
        return;
    }

    const std::string client_ip = remote_ep.address().to_string();
    LOG_INFO("TcpServer", "connection from {}:{}", client_ip, remote_ep.port());

    if (!validator_->is_allowed(client_ip)) {
        LOG_WARN("TcpServer", "IP {} is NOT in the whitelist — rejecting", client_ip);
        return;
    }

    LOG_INFO("TcpServer", "IP {} is allowed", client_ip);

    const std::string session_id = generate_session_id();
    LOG_INFO("TcpServer", "session_id={}", session_id);

    if (!service_mgr_->start_authenticator(session_id, client_ip)) {
        LOG_WARN("TcpServer", "service start failed");
        return;
    }

    auto session = std::make_shared<SmppSession>();
    read_pdu_header(socket, session);
}

void TcpServer::read_pdu_header(std::shared_ptr<asio::ip::tcp::socket> socket,
                                std::shared_ptr<SmppSession> session)
{
    auto header_buffer = std::make_shared<std::vector<uint8_t>>(16);

    asio::async_read(*socket, asio::buffer(*header_buffer, 16),
        [this, socket, session, header_buffer](const asio::error_code& ec,
                                               std::size_t bytes) {
            on_header_read(socket, session, ec, bytes, header_buffer);
        });
}

void TcpServer::on_header_read(std::shared_ptr<asio::ip::tcp::socket> socket,
                               std::shared_ptr<SmppSession> session,
                               const asio::error_code& ec,
                               std::size_t bytes_transferred,
                               std::shared_ptr<std::vector<uint8_t>> header_buffer)
{
    if (ec) {
        if (ec != asio::error::eof) {
            LOG_WARN("TcpServer", "read header error: {}", ec.message());
        }
        socket->close();
        return;
    }

    if (bytes_transferred != 16) {
        LOG_WARN("TcpServer", "incomplete header read");
        socket->close();
        return;
    }

    uint32_t command_length = ntoh32(header_buffer->data());
    if (command_length < 16) {
        LOG_WARN("TcpServer", "invalid command_length");
        socket->close();
        return;
    }

    uint32_t body_length = command_length - 16;
    read_pdu_body(socket, session, header_buffer, body_length);
}

void TcpServer::read_pdu_body(std::shared_ptr<asio::ip::tcp::socket> socket,
                              std::shared_ptr<SmppSession> session,
                              std::shared_ptr<std::vector<uint8_t>> header_buffer,
                              uint32_t body_length)
{
    auto body_buffer = std::make_shared<std::vector<uint8_t>>(body_length);

    asio::async_read(*socket, asio::buffer(*body_buffer, body_length),
        [this, socket, session, header_buffer, body_buffer](
            const asio::error_code& ec, std::size_t bytes) {
            on_body_read(socket, session, header_buffer, body_buffer, ec, bytes);
        });
}

void TcpServer::on_body_read(std::shared_ptr<asio::ip::tcp::socket> socket,
                             std::shared_ptr<SmppSession> session,
                             std::shared_ptr<std::vector<uint8_t>> header_buffer,
                             std::shared_ptr<std::vector<uint8_t>> body_buffer,
                             const asio::error_code& ec,
                             std::size_t bytes_transferred)
{
    if (ec) {
        if (ec != asio::error::eof) {
            LOG_WARN("TcpServer", "read body error: {}", ec.message());
        }
        socket->close();
        return;
    }

    std::vector<uint8_t> full_pdu(*header_buffer);
    full_pdu.insert(full_pdu.end(), body_buffer->begin(), body_buffer->end());

    // TODO: Implement PDU dispatch logic
    // SmppHandler::dispatch_pdu(full_pdu, *session, *socket);

    if (socket->is_open()) {
        read_pdu_header(socket, session);
    }
}
