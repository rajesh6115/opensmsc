#include "auth_service.hpp"

#include <spdlog/spdlog.h>

AuthService::AuthService(sdbus::IObject& obj, std::shared_ptr<CredentialsStore> store)
    : IAuth_adaptor(obj)
    , store_(std::move(store))
{}

std::tuple<bool, uint32_t>
AuthService::Authenticate(const std::string& uuid,
                          const std::string& system_id,
                          const std::string& password,
                          const std::string& source_ip)
{
    auto [ok, code] = store_->authenticate(system_id, password);
    spdlog::info("[AuthService            ] Authenticate uuid={} system_id={} ip={} ok={}",
                 uuid, system_id, source_ip, ok);
    return {ok, code};
}

void AuthService::ReloadCredentials()
{
    store_->reload();
    spdlog::info("[AuthService            ] credentials reloaded from {}", store_->path());
}
