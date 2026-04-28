#pragma once

#include <shared_mutex>
#include <string>
#include <unordered_map>

class CredentialsStore {
public:
    static constexpr uint32_t OK              = 0x00000000;
    static constexpr uint32_t ESME_RINVSYSID  = 0x0000000F;
    static constexpr uint32_t ESME_RINVPASWD  = 0x0000000E;

    explicit CredentialsStore(const std::string& toml_path);

    // Returns {true, OK} on success, {false, error_code} on failure.
    std::pair<bool, uint32_t> authenticate(const std::string& system_id,
                                           const std::string& password) const;

    void reload();
    const std::string& path() const { return path_; }

private:
    void load();

    std::string                              path_;
    mutable std::shared_mutex                mutex_;
    std::unordered_map<std::string,std::string> credentials_;
};
