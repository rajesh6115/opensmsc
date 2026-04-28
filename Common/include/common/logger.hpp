#pragma once

#include <memory>
#include <string>
#include <spdlog/spdlog.h>

namespace smpp {

struct LogContext {
    std::string session_id;
    std::string client_ip;
    std::string system_id;
};

class Logger {
public:
    static void init(const std::string& service_name);
    static std::shared_ptr<spdlog::logger> get();

    static void info(const std::string& msg);
    static void warn(const std::string& msg);
    static void error(const std::string& msg);
    static void debug(const std::string& msg);

    static void info(const LogContext& ctx, const std::string& msg);
    static void warn(const LogContext& ctx, const std::string& msg);
    static void error(const LogContext& ctx, const std::string& msg);
    static void debug(const LogContext& ctx, const std::string& msg);

private:
    static std::string format(const LogContext& ctx, const std::string& msg);
};

} // namespace smpp
