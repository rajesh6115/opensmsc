#include "credentials_store.hpp"

#include <toml++/toml.hpp>
#include <mutex>
#include <stdexcept>

CredentialsStore::CredentialsStore(const std::string& toml_path)
    : path_(toml_path)
{
    load();
}

void CredentialsStore::load()
{
    std::unordered_map<std::string,std::string> tmp;
    auto tbl = toml::parse_file(path_);
    if (auto arr = tbl["credentials"].as_array()) {
        for (const auto& entry : *arr) {
            if (auto t = entry.as_table()) {
                auto sid = (*t)["system_id"].value<std::string>();
                auto pwd = (*t)["password"].value<std::string>();
                if (sid && pwd) tmp[*sid] = *pwd;
            }
        }
    }
    std::unique_lock lock(mutex_);
    credentials_ = std::move(tmp);
}

void CredentialsStore::reload()
{
    load();
}

std::pair<bool, uint32_t> CredentialsStore::authenticate(const std::string& system_id,
                                                          const std::string& password) const
{
    std::shared_lock lock(mutex_);
    auto it = credentials_.find(system_id);
    if (it == credentials_.end()) return {false, ESME_RINVSYSID};
    if (it->second != password)   return {false, ESME_RINVPASWD};
    return {true, OK};
}
