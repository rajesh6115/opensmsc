#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace smpp {

// ── Command IDs ───────────────────────────────────────────────────────────────
constexpr uint32_t BIND_TRANSMITTER      = 0x00000002;
constexpr uint32_t BIND_TRANSMITTER_RESP = 0x80000002;
constexpr uint32_t BIND_RECEIVER         = 0x00000003;
constexpr uint32_t BIND_RECEIVER_RESP    = 0x80000003;
constexpr uint32_t BIND_TRANSCEIVER      = 0x00000009;
constexpr uint32_t BIND_TRANSCEIVER_RESP = 0x80000009;
constexpr uint32_t UNBIND                = 0x00000006;
constexpr uint32_t UNBIND_RESP           = 0x80000006;
constexpr uint32_t ENQUIRE_LINK          = 0x00000015;
constexpr uint32_t ENQUIRE_LINK_RESP     = 0x80000015;
constexpr uint32_t SUBMIT_SM             = 0x00000004;
constexpr uint32_t SUBMIT_SM_RESP        = 0x80000004;
constexpr uint32_t DELIVER_SM            = 0x00000005;
constexpr uint32_t DELIVER_SM_RESP       = 0x80000005;
constexpr uint32_t GENERIC_NACK          = 0x80000000;

// ── Status codes ─────────────────────────────────────────────────────────────
constexpr uint32_t ESME_ROK         = 0x00000000;
constexpr uint32_t ESME_RINVBNDSTS  = 0x00000004; // invalid bind status
constexpr uint32_t ESME_RALYBND     = 0x00000005; // already bound
constexpr uint32_t ESME_RINVPASWD   = 0x0000000E;
constexpr uint32_t ESME_RINVSYSID   = 0x0000000F;
constexpr uint32_t ESME_RINVCMDID   = 0x00000003; // invalid command id

// ── PDU header ────────────────────────────────────────────────────────────────
struct Header {
    uint32_t command_length;
    uint32_t command_id;
    uint32_t command_status;
    uint32_t sequence_number;
};

inline uint32_t read_u32(const uint8_t* p)
{
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16)
         | (uint32_t(p[2]) << 8)  | uint32_t(p[3]);
}

inline void write_u32(uint8_t* p, uint32_t v)
{
    p[0] = (v >> 24) & 0xff;
    p[1] = (v >> 16) & 0xff;
    p[2] = (v >>  8) & 0xff;
    p[3] = v & 0xff;
}

inline Header decode_header(const uint8_t* buf)
{
    return {read_u32(buf), read_u32(buf+4), read_u32(buf+8), read_u32(buf+12)};
}

// Parse C-octet string from buffer at offset; advances offset past the null terminator.
inline std::string read_cstr(const std::vector<uint8_t>& buf, size_t& off, size_t max_len = 65)
{
    std::string s;
    while (off < buf.size() && s.size() < max_len) {
        char c = static_cast<char>(buf[off++]);
        if (c == '\0') break;
        s += c;
    }
    return s;
}

// Build a minimal header-only response (no body or just system_id).
inline std::vector<uint8_t> make_response(uint32_t command_id,
                                           uint32_t command_status,
                                           uint32_t sequence_number,
                                           const std::string& system_id = "")
{
    const size_t body_len = system_id.empty() ? 0 : system_id.size() + 1;
    const uint32_t total  = 16 + static_cast<uint32_t>(body_len);
    std::vector<uint8_t> pdu(total);
    write_u32(pdu.data(),      total);
    write_u32(pdu.data() + 4,  command_id);
    write_u32(pdu.data() + 8,  command_status);
    write_u32(pdu.data() + 12, sequence_number);
    if (!system_id.empty()) {
        std::memcpy(pdu.data() + 16, system_id.c_str(), system_id.size() + 1);
    }
    return pdu;
}

// Build a deliver_sm PDU (SMSC→ESME MO delivery).
// All address fields are C-octet strings; short_message is raw bytes.
inline std::vector<uint8_t> make_deliver_sm(
    uint32_t           sequence_number,
    const std::string& src_addr,
    const std::string& dst_addr,
    const std::string& short_message,
    uint8_t            src_ton = 0, uint8_t src_npi = 0,
    uint8_t            dst_ton = 0, uint8_t dst_npi = 0)
{
    std::vector<uint8_t> body;
    // service_type (empty)
    body.push_back(0);
    body.push_back(src_ton); body.push_back(src_npi);
    for (char c : src_addr) body.push_back(static_cast<uint8_t>(c));
    body.push_back(0);
    body.push_back(dst_ton); body.push_back(dst_npi);
    for (char c : dst_addr) body.push_back(static_cast<uint8_t>(c));
    body.push_back(0);
    // esm_class=0, protocol_id=0, priority_flag=0
    body.push_back(0); body.push_back(0); body.push_back(0);
    // schedule_delivery_time="", validity_period=""
    body.push_back(0); body.push_back(0);
    // registered_delivery=0, replace_if_present=0, data_coding=0, sm_default_msg_id=0
    body.push_back(0); body.push_back(0); body.push_back(0); body.push_back(0);
    // sm_length + short_message
    body.push_back(static_cast<uint8_t>(short_message.size()));
    for (char c : short_message) body.push_back(static_cast<uint8_t>(c));

    uint32_t total = 16 + static_cast<uint32_t>(body.size());
    std::vector<uint8_t> pdu(total);
    write_u32(pdu.data(),      total);
    write_u32(pdu.data() + 4,  DELIVER_SM);
    write_u32(pdu.data() + 8,  ESME_ROK);
    write_u32(pdu.data() + 12, sequence_number);
    std::memcpy(pdu.data() + 16, body.data(), body.size());
    return pdu;
}

inline uint32_t bind_resp_id(uint32_t req_id)
{
    return req_id | 0x80000000u;
}

} // namespace smpp
