#pragma once

#include <cstdint>
#include <memory>
#include <vector>

class SmppSession;

class SmppHandler
{
public:
    static std::vector<uint8_t> build_bind_resp(
        uint32_t command_id,
        const uint8_t* raw_pdu,
        std::size_t pdu_len,
        uint32_t sequence_number,
        SmppSession& session);

    static std::vector<uint8_t> build_unbind_resp(
        uint32_t status,
        uint32_t sequence_number);

    static std::vector<uint8_t> build_enquire_link_resp(
        uint32_t status,
        uint32_t sequence_number);

    static std::vector<uint8_t> build_generic_nack(
        uint32_t status,
        uint32_t sequence_number);

private:
    static bool validate_credentials(
        const std::string& username,
        const std::string& password);
};
