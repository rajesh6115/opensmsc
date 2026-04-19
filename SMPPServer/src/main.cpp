#include "ip_validator.hpp"
#include "logger.hpp"
#include "smpp_service_manager.hpp"
#include "tcp_server.hpp"

#include <asio.hpp>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

// ── Defaults (override via env or argv) ──────────────────────────────────────

static constexpr uint16_t    DEFAULT_PORT       = 2775;
static constexpr const char* DEFAULT_IP_CONFIG  = "/etc/simple_smpp_server/allowed_ips.conf";
static constexpr const char* DEFAULT_LOG_FILE   = "/var/log/simple_smpp_server/server.log";
static constexpr const char* DEFAULT_LOG_LEVEL  = "info";

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
    uint16_t    port         = DEFAULT_PORT;
    std::string ip_config    = DEFAULT_IP_CONFIG;
    std::string log_level_str = DEFAULT_LOG_LEVEL;

    // Check env variable first (lowest precedence)
    if (const char* env = std::getenv("SMPP_LOG_LEVEL"); env != nullptr) {
        log_level_str = env;
    }

    // Parse command-line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg.find("--log-level=") == 0) {
            log_level_str = arg.substr(12);
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        } else if (i == 1) {
            // First positional arg is port
            try { port = static_cast<uint16_t>(std::stoul(argv[i])); }
            catch (...) {
                fprintf(stderr, "[ERROR] Invalid port: %s\n", argv[i]);
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }
        } else if (i == 2) {
            // Second positional arg is ip_config
            ip_config = argv[i];
        }
    }

    // ── Initialize logging FIRST ────────────────────────────────────────────
    auto log_level = logger::level_from_string(log_level_str);
    logger::init(DEFAULT_LOG_FILE, log_level);

    LOG_INFO("main", "simple_smpp_server starting");
    LOG_INFO("main", "port      : {}", port);
    LOG_INFO("main", "ip_config : {}", ip_config);
    LOG_INFO("main", "log_level : {}", log_level_str);
    LOG_INFO("main", "log_file  : {}", DEFAULT_LOG_FILE);

    // ── Bootstrap components ──────────────────────────────────────────────────
    try {
        auto validator    = std::make_shared<IpValidator>(ip_config);
        auto service_mgr  = std::make_shared<SmppServiceManager>();

        asio::io_context io_ctx;

        // Graceful shutdown on Ctrl-C / SIGTERM via ASIO signal_set
        asio::signal_set signals(io_ctx, SIGINT, SIGTERM);
        signals.async_wait([&io_ctx](const asio::error_code& ec, int sig) {
            if (!ec) {
                LOG_INFO("main", "received signal {} — stopping", sig);
                io_ctx.stop();
            }
        });

        TcpServer server(io_ctx, port, validator, service_mgr);

        // Run the io_context on N threads (at least 2 so accept and write
        // can overlap, but leave headroom for systemd D-Bus calls).
        const unsigned int n_threads =
            std::max(2u, std::thread::hardware_concurrency());

        LOG_INFO("main", "io_context running on {} thread(s)", n_threads);

        std::vector<std::thread> workers;
        workers.reserve(n_threads - 1);
        for (unsigned i = 0; i < n_threads - 1; ++i) {
            workers.emplace_back([&io_ctx]{ io_ctx.run(); });
        }
        io_ctx.run();   // main thread also drives the loop

        for (auto& t : workers) t.join();

    } catch (const std::exception& ex) {
        LOG_FATAL("main", "{}", ex.what());
        logger::shutdown();
        return EXIT_FAILURE;
    }

    LOG_INFO("main", "simple_smpp_server stopped");
    logger::shutdown();
    return EXIT_SUCCESS;
}
