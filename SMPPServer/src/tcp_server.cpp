#include "tcp_server.hpp"
#include "connection_registry.hpp"
#include "ip_validator.hpp"
#include "logger.hpp"
#include "session_id.hpp"

#include <asio.hpp>

TcpServer::TcpServer(asio::io_context&                    io_ctx,
                     uint16_t                              port,
                     std::shared_ptr<IpValidator>          validator,
                     std::shared_ptr<ConnectionRegistry>   registry)
    : acceptor_(io_ctx,
                asio::ip::tcp::endpoint(asio::ip::tcp::v6(), port),
                /* reuse_addr */ true)
    , validator_(std::move(validator))
    , registry_(std::move(registry))
{
    asio::error_code ec;
    acceptor_.set_option(asio::ip::v6_only(false), ec);
    if (ec) {
        LOG_WARN("TcpServer", "could not set IPV6_V6ONLY=0: {}", ec.message());
    }
    LOG_INFO("TcpServer", "listening on [::]:{}",  port);
    do_accept();
}

uint16_t TcpServer::port() const
{
    return acceptor_.local_endpoint().port();
}

void TcpServer::do_accept()
{
    auto socket = std::make_shared<asio::ip::tcp::socket>(acceptor_.get_executor());
    acceptor_.async_accept(*socket, [this, socket](const asio::error_code& ec) {
        if (!ec) {
            handle_connection(socket);
        } else if (ec != asio::error::operation_aborted) {
            LOG_ERROR("TcpServer", "accept error: {}", ec.message());
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

    if (!validator_->is_allowed(client_ip)) {
        LOG_WARN("TcpServer", "IP {} not in whitelist — rejecting", client_ip);
        return;
    }

    const std::string uuid = generate_session_id();
    int raw_fd = socket->native_handle();

    if (!registry_->add(uuid, raw_fd, client_ip)) {
        LOG_WARN("TcpServer", "IP {} exceeded max connections — rejecting (uuid={})",
                 client_ip, uuid);
        return;
    }

    // Transfer socket ownership: fd stays open; SmppClientHandler claims via GetSocket.
    socket->release();

    LOG_INFO("TcpServer", "accepted uuid={} ip={}", uuid, client_ip);
    // T9: systemd-run launch of smpp_client_handler goes here
}
