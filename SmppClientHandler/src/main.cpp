#include "smpp_session.hpp"

#include <asio.hpp>
#include <sdbus-c++/sdbus-c++.h>
#include <spdlog/spdlog.h>

#include <csignal>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <thread>

static constexpr const char* ISERVER_IFACE = "com.telecom.smpp.IServer";
static constexpr const char* ISERVER_PATH  = "/com/telecom/smpp/server";

int main(int argc, char* argv[])
{
    std::string uuid;
    std::string client_ip;
    std::string server_svc = "com.telecom.smpp.Server";

    for (int i = 1; i < argc; ++i) {
        std::string a(argv[i]);
        if      (a.rfind("--uuid=",       0) == 0) uuid       = a.substr(7);
        else if (a.rfind("--client-ip=",  0) == 0) client_ip  = a.substr(12);
        else if (a.rfind("--server-svc=", 0) == 0) server_svc = a.substr(13);
    }

    if (uuid.empty() || client_ip.empty()) {
        spdlog::critical("Usage: smpp_client_handler --uuid=UUID --client-ip=IP");
        return EXIT_FAILURE;
    }

    spdlog::set_level(spdlog::level::info);
    spdlog::info("[main] smpp_client_handler starting uuid={} ip={}", uuid, client_ip);

    try {
        const std::string svc_name = "com.telecom.smpp.Client." + uuid;
        auto dbus_conn = sdbus::createSystemBusConnection(svc_name);
        auto dbus_obj  = sdbus::createObject(*dbus_conn, "/com/telecom/smpp/client");

        auto server_proxy = sdbus::createProxy(*dbus_conn, server_svc, ISERVER_PATH);
        sdbus::UnixFd fd;
        server_proxy->callMethod("GetSocket").onInterface(ISERVER_IFACE)
                     .withArguments(uuid).storeResultsTo(fd);
        spdlog::info("[main] claimed fd={} uuid={}", fd.get(), uuid);

        asio::io_context io_ctx;
        asio::signal_set signals(io_ctx, SIGINT, SIGTERM);
        signals.async_wait([&](const asio::error_code& ec, int sig) {
            if (!ec) { spdlog::info("[main] signal {}", sig); io_ctx.stop(); }
        });

        auto session = std::make_shared<SmppSession>(
            io_ctx, fd.get(), uuid, client_ip, *dbus_obj, *dbus_conn, server_svc);
        dbus_obj->finishRegistration();

        std::thread dbus_thread([&dbus_conn]{ dbus_conn->enterEventLoop(); });

        session->start();
        io_ctx.run();

        dbus_conn->leaveEventLoop();
        if (dbus_thread.joinable()) dbus_thread.join();

    } catch (const std::exception& ex) {
        spdlog::critical("[main] {}", ex.what());
        return EXIT_FAILURE;
    }

    spdlog::info("[main] smpp_client_handler stopped uuid={}", uuid);
    return EXIT_SUCCESS;
}
