#include "ip_validator.hpp"
#include "smpp_service_manager.hpp"
#include "tcp_server.hpp"

#include <asio.hpp>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

// ── Defaults (override via env or argv) ──────────────────────────────────────

static constexpr uint16_t    DEFAULT_PORT       = 2775;
static constexpr const char* DEFAULT_IP_CONFIG  = "/etc/simple_smpp_server/allowed_ips.conf";

// ── Signal handling via ASIO (no globals) ────────────────────────────────────

// ── Helpers ───────────────────────────────────────────────────────────────────

static void print_usage(const char* prog)
{
    std::cerr
        << "Usage: " << prog << " [port] [ip_config_file]\n"
        << "  port           : TCP listen port (default " << DEFAULT_PORT << ")\n"
        << "  ip_config_file : path to allowed-IPs whitelist\n"
        << "                   (default " << DEFAULT_IP_CONFIG << ")\n";
}

// ── main ──────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[])
{
    // ── Parse arguments ───────────────────────────────────────────────────────
    uint16_t    port      = DEFAULT_PORT;
    std::string ip_config = DEFAULT_IP_CONFIG;

    if (argc > 3) { print_usage(argv[0]); return EXIT_FAILURE; }
    if (argc > 1) {
        try { port = static_cast<uint16_t>(std::stoul(argv[1])); }
        catch (...) {
            std::cerr << "[ERROR] Invalid port: " << argv[1] << "\n";
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }
    if (argc > 2) ip_config = argv[2];

    std::cout << "[INFO] simple_smpp_server starting\n"
              << "[INFO]   port      : " << port      << "\n"
              << "[INFO]   ip_config : " << ip_config << "\n";

    // ── Bootstrap components ──────────────────────────────────────────────────
    try {
        auto validator    = std::make_shared<IpValidator>(ip_config);
        auto service_mgr  = std::make_shared<SmppServiceManager>();

        asio::io_context io_ctx;

        // Graceful shutdown on Ctrl-C / SIGTERM via ASIO signal_set
        asio::signal_set signals(io_ctx, SIGINT, SIGTERM);
        signals.async_wait([&io_ctx](const asio::error_code& ec, int sig) {
            if (!ec) {
                std::cout << "\n[INFO] main: received signal " << sig << " — stopping\n";
                io_ctx.stop();
            }
        });

        TcpServer server(io_ctx, port, validator, service_mgr);

        // Run the io_context on N threads (at least 2 so accept and write
        // can overlap, but leave headroom for systemd D-Bus calls).
        const unsigned int n_threads =
            std::max(2u, std::thread::hardware_concurrency());

        std::cout << "[INFO] io_context running on " << n_threads << " thread(s)\n";

        std::vector<std::thread> workers;
        workers.reserve(n_threads - 1);
        for (unsigned i = 0; i < n_threads - 1; ++i) {
            workers.emplace_back([&io_ctx]{ io_ctx.run(); });
        }
        io_ctx.run();   // main thread also drives the loop

        for (auto& t : workers) t.join();

    } catch (const std::exception& ex) {
        std::cerr << "[FATAL] " << ex.what() << "\n";
        return EXIT_FAILURE;
    }

    std::cout << "[INFO] simple_smpp_server stopped\n";
    return EXIT_SUCCESS;
}
