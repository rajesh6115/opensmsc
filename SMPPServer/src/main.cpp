#include "connection_registry.hpp"
#include "ip_validator.hpp"
#include "logger.hpp"
#include "tcp_server.hpp"

#include <asio.hpp>
#include <csignal>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

static constexpr uint16_t    DEFAULT_PORT      = 2775;
static constexpr const char* DEFAULT_IP_CONFIG = "/etc/simple_smpp_server/allowed_ips.conf";
static constexpr const char* DEFAULT_LOG_FILE  = "/var/log/simple_smpp_server/server.log";
static constexpr const char* DEFAULT_LOG_LEVEL = "info";
static constexpr uint32_t    DEFAULT_MAX_PER_IP = 5;

static void print_usage(const char* prog)
{
    fprintf(stderr,
            "Usage: %s [--port=N] [--ip-config=FILE] [--log-level=LEVEL]\n",
            prog);
}

int main(int argc, char* argv[])
{
    uint16_t    port          = DEFAULT_PORT;
    std::string ip_config     = DEFAULT_IP_CONFIG;
    std::string log_level_str = DEFAULT_LOG_LEVEL;

    if (const char* env = std::getenv("SMPP_LOG_LEVEL"); env) log_level_str = env;
    if (const char* env = std::getenv("SMPP_PORT");      env)
        port = static_cast<uint16_t>(std::stoul(env));

    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if      (arg.rfind("--port=",      0) == 0) port          = static_cast<uint16_t>(std::stoul(arg.substr(7)));
        else if (arg.rfind("--ip-config=", 0) == 0) ip_config     = arg.substr(12);
        else if (arg.rfind("--log-level=", 0) == 0) log_level_str = arg.substr(12);
        else if (arg == "--help" || arg == "-h") { print_usage(argv[0]); return EXIT_SUCCESS; }
    }

    auto log_level = logger::level_from_string(log_level_str);
    logger::init(DEFAULT_LOG_FILE, log_level);

    LOG_INFO("main", "simple_smpp_server starting port={} ip_config={}", port, ip_config);

    try {
        auto validator = std::make_shared<IpValidator>(ip_config);
        auto registry  = std::make_shared<ConnectionRegistry>(DEFAULT_MAX_PER_IP);

        asio::io_context io_ctx;

        asio::signal_set signals(io_ctx, SIGINT, SIGTERM);
        signals.async_wait([&io_ctx](const asio::error_code& ec, int sig) {
            if (!ec) {
                LOG_INFO("main", "signal {} — stopping", sig);
                io_ctx.stop();
            }
        });

        TcpServer server(io_ctx, port, validator, registry);

        const unsigned int n = std::max(2u, std::thread::hardware_concurrency());
        LOG_INFO("main", "io_context running on {} thread(s)", n);

        std::vector<std::thread> workers;
        workers.reserve(n - 1);
        for (unsigned i = 0; i < n - 1; ++i)
            workers.emplace_back([&io_ctx]{ io_ctx.run(); });
        io_ctx.run();
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
