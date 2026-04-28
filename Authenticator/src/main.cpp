#include "auth_service.hpp"
#include "credentials_store.hpp"

#include <sdbus-c++/sdbus-c++.h>
#include <spdlog/spdlog.h>

#include <csignal>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>

static constexpr const char* DEFAULT_CREDENTIALS =
    "/etc/smpp-server/credentials.toml";

static std::shared_ptr<CredentialsStore> g_store;

static void sighup_handler(int)
{
    if (g_store) g_store->reload();
}

int main(int argc, char* argv[])
{
    std::string creds_path = DEFAULT_CREDENTIALS;
    if (const char* env = std::getenv("SMPP_CREDENTIALS"); env) creds_path = env;
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg.rfind("--credentials=", 0) == 0) creds_path = arg.substr(14);
    }

    spdlog::set_level(spdlog::level::info);
    spdlog::info("[main                   ] authenticator starting, creds={}", creds_path);

    try {
        g_store = std::make_shared<CredentialsStore>(creds_path);

        std::signal(SIGHUP, sighup_handler);

        auto conn = sdbus::createSystemBusConnection(AuthService::SERVICE_NAME);
        auto obj  = sdbus::createObject(*conn, AuthService::OBJ_PATH);
        AuthService svc(*obj, g_store);
        obj->finishRegistration();

        spdlog::info("[main                   ] D-Bus service ready: {}",
                     AuthService::SERVICE_NAME);
        conn->enterEventLoop();

    } catch (const std::exception& ex) {
        spdlog::critical("[main                   ] {}", ex.what());
        return EXIT_FAILURE;
    }

    spdlog::info("[main                   ] authenticator stopped");
    return EXIT_SUCCESS;
}
