#include "common/logger.hpp"

#include <spdlog/sinks/systemd_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace smpp {

static std::shared_ptr<spdlog::logger> s_logger;

void Logger::init(const std::string& service_name) {
    std::vector<spdlog::sink_ptr> sinks;

    try {
        auto systemd_sink = std::make_shared<spdlog::sinks::systemd_sink_st>();
        systemd_sink->set_level(spdlog::level::trace);
        sinks.push_back(systemd_sink);
    } catch (...) {
        // fallback to stdout if systemd is not available (e.g. in unit tests)
        auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_st>();
        stdout_sink->set_level(spdlog::level::trace);
        sinks.push_back(stdout_sink);
    }

    spdlog::drop(service_name);
    s_logger = std::make_shared<spdlog::logger>(service_name, sinks.begin(), sinks.end());
    s_logger->set_level(spdlog::level::trace);
    spdlog::register_logger(s_logger);
    spdlog::set_default_logger(s_logger);
}

std::shared_ptr<spdlog::logger> Logger::get() {
    return s_logger;
}

std::string Logger::format(const LogContext& ctx, const std::string& msg) {
    return "[sid=" + ctx.session_id +
           " ip="  + ctx.client_ip  +
           " sys=" + ctx.system_id  +
           "] "    + msg;
}

void Logger::info(const std::string& msg)                        { if (s_logger) s_logger->info(msg); }
void Logger::warn(const std::string& msg)                        { if (s_logger) s_logger->warn(msg); }
void Logger::error(const std::string& msg)                       { if (s_logger) s_logger->error(msg); }
void Logger::debug(const std::string& msg)                       { if (s_logger) s_logger->debug(msg); }

void Logger::info(const LogContext& ctx, const std::string& msg)  { if (s_logger) s_logger->info(format(ctx, msg)); }
void Logger::warn(const LogContext& ctx, const std::string& msg)  { if (s_logger) s_logger->warn(format(ctx, msg)); }
void Logger::error(const LogContext& ctx, const std::string& msg) { if (s_logger) s_logger->error(format(ctx, msg)); }
void Logger::debug(const LogContext& ctx, const std::string& msg) { if (s_logger) s_logger->debug(format(ctx, msg)); }

} // namespace smpp
