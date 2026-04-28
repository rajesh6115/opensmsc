#pragma once

#include <asio.hpp>
#include <cstdint>
#include <memory>

class IpValidator;
class SmppServerService;

class TcpServer {
public:
    TcpServer(asio::io_context&               io_ctx,
              uint16_t                         port,
              std::shared_ptr<IpValidator>     validator,
              SmppServerService&               svc);

    uint16_t port() const;

private:
    void do_accept();
    void handle_connection(std::shared_ptr<asio::ip::tcp::socket> socket);

    asio::ip::tcp::acceptor   acceptor_;
    std::shared_ptr<IpValidator> validator_;
    SmppServerService&        svc_;
};
