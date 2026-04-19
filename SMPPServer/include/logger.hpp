#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <memory>
#include <string>

// ── Log-level macros ──────────────────────────────────────────────────────────
//
// Usage:
//   LOG_INFO("TcpServer",  "listening on [::]:{}",  port);
//   LOG_WARN("SmppHandler","already bound");
//   LOG_ERROR("TcpServer", "accept error: {}", ec.message());
//   LOG_FATAL("main",      "{}", ex.what());
//
// Module name is left-padded to 20 chars for columnar alignment.
// Format string and args follow {fmt} conventions (spdlog's underlying engine).

#define LOG_INFO(module, fmt, ...)  \
    spdlog::info("[{:<20}] " fmt, module, ##__VA_ARGS__)

#define LOG_WARN(module, fmt, ...)  \
    spdlog::warn("[{:<20}] " fmt, module, ##__VA_ARGS__)

#define LOG_ERROR(module, fmt, ...) \
    spdlog::error("[{:<20}] " fmt, module, ##__VA_ARGS__)

#define LOG_FATAL(module, fmt, ...) \
    spdlog::critical("[{:<20}] " fmt, module, ##__VA_ARGS__)

// ── Initialiser and utilities ─────────────────────────────────────────────────

namespace logger {

inline void init(const std::string& log_file_path,
                 spdlog::level::level_enum level = spdlog::level::info)
{
    // Sink 1: coloured stdout
    auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    stdout_sink->set_level(level);

    // Sink 2: rotating file (10 MB per file, keep 5 rotations)
    std::shared_ptr<spdlog::logger> combined;
    try {
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            log_file_path, 10 * 1024 * 1024, 5);
        file_sink->set_level(level);
        combined = std::make_shared<spdlog::logger>("smpp",
            spdlog::sinks_init_list{stdout_sink, file_sink});
    } catch (const spdlog::spdlog_ex& ex) {
        // Log directory missing or unwritable — continue with stdout only
        fprintf(stderr, "[logger::init] file sink failed (%s) — stdout only\n", ex.what());
        combined = std::make_shared<spdlog::logger>("smpp", stdout_sink);
    }

    combined->set_level(level);

    // Format: "2026-04-19 05:01:23.456 [INFO ] message"
    // Module name is embedded in the message via LOG_* macros.
    combined->set_pattern("%Y-%m-%d %H:%M:%S.%e [%^%-5l%$] %v");

    spdlog::set_default_logger(combined);
    spdlog::flush_on(spdlog::level::warn);           // flush immediately on warn+
    spdlog::flush_every(std::chrono::seconds(2));    // periodic flush for info
}

inline void shutdown()
{
    spdlog::shutdown();
}

inline spdlog::level::level_enum level_from_string(const std::string& s)
{
    if (s == "trace")                return spdlog::level::trace;
    if (s == "debug")                return spdlog::level::debug;
    if (s == "info")                 return spdlog::level::info;
    if (s == "warn" || s == "warning") return spdlog::level::warn;
    if (s == "error" || s == "err")  return spdlog::level::err;
    if (s == "critical" || s == "fatal") return spdlog::level::critical;
    return spdlog::level::info;   // safe default
}

} // namespace logger
