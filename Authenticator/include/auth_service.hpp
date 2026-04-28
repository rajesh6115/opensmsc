#pragma once

#include "credentials_store.hpp"
#include "generated/IAuth_adaptor.hpp"

#include <memory>
#include <string>

class AuthService : public com::telecom::smpp::IAuth_adaptor {
public:
    static constexpr const char* SERVICE_NAME = "com.telecom.smpp.Auth";
    static constexpr const char* OBJ_PATH     = "/com/telecom/smpp/auth";

    AuthService(sdbus::IObject& obj, std::shared_ptr<CredentialsStore> store);

private:
    std::tuple<bool, uint32_t>
    Authenticate(const std::string& uuid,
                 const std::string& system_id,
                 const std::string& password,
                 const std::string& source_ip) override;

    void ReloadCredentials() override;

    std::shared_ptr<CredentialsStore> store_;
};
