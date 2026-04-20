#include "smpp_handler.hpp"

#include "logger.hpp"
#include "smpp_session.hpp"
#include <smpp.hpp>

#include <cstring>
#include <vector>

namespace {
    // SMPP command IDs
    constexpr uint32_t CMD_BIND_RECEIVER = 0x00000001;
    constexpr uint32_t CMD_BIND_TRANSMITTER = 0x00000002;
    constexpr uint32_t CMD_BIND_TRANSCEIVER = 0x00000009;
    constexpr uint32_t CMD_UNBIND = 0x00000006;
    constexpr uint32_t CMD_ENQUIRE_LINK = 0x0000000F;

    uint32_t ntoh32(const uint8_t* ptr) {
        return ((uint32_t)ptr[0] << 24) | ((uint32_t)ptr[1] << 16) |
               ((uint32_t)ptr[2] << 8) | ((uint32_t)ptr[3]);
    }

    std::vector<uint8_t> encode_pdu(const Smpp::Response& resp) {
        Smpp::Buffer buffer;
        // Note: encode() is protected in smppcxx, but Response subclasses may be used directly
        // For now, return empty vector as placeholder
        return std::vector<uint8_t>();
    }
}

std::vector<uint8_t> SmppHandler::build_bind_resp(
    uint32_t command_id,
    const uint8_t* raw_pdu,
    std::size_t pdu_len,
    uint32_t sequence_number,
    SmppSession& session)
{
    if (session.is_bound()) {
        LOG_WARN("SmppHandler", "already bound");
        if (command_id == CMD_BIND_RECEIVER) {
            Smpp::BindReceiverResp error_resp;
            error_resp.command_status(0x00000005);  // ESME_RALYBND
            error_resp.sequence_number(sequence_number);
            return encode_pdu(error_resp);
        } else if (command_id == CMD_BIND_TRANSMITTER) {
            Smpp::BindTransmitterResp error_resp;
            error_resp.command_status(0x00000005);  // ESME_RALYBND
            error_resp.sequence_number(sequence_number);
            return encode_pdu(error_resp);
        } else {  // TRANSCEIVER
            Smpp::BindTransceiverResp error_resp;
            error_resp.command_status(0x00000005);  // ESME_RALYBND
            error_resp.sequence_number(sequence_number);
            return encode_pdu(error_resp);
        }
    }

    try {
        if (command_id == CMD_BIND_RECEIVER) {
            LOG_INFO("SmppHandler", "BIND_RECEIVER parsing PDU body_len={}", pdu_len);
            Smpp::BindReceiver bind_req(raw_pdu);
            std::string username = std::string(bind_req.system_id());
            std::string password = std::string(bind_req.password());
            LOG_INFO("SmppHandler", "BIND_RECEIVER username={}", username);

            if (!validate_credentials(username, password)) {
                LOG_WARN("SmppHandler", "authentication failed");
                Smpp::BindReceiverResp error_resp;
                error_resp.command_status(0x0000000E);  // ESME_RINVPASWD
                error_resp.sequence_number(sequence_number);
                return encode_pdu(error_resp);
            }

            session.set_system_id(username);
            session.set_state(SmppSession::State::BOUND_RECEIVER);

            Smpp::BindReceiverResp bind_resp;
            bind_resp.command_status(0x00000000);  // ESME_ROK
            bind_resp.sequence_number(sequence_number);
            bind_resp.system_id("TestServer");
            LOG_INFO("SmppHandler", "BIND_RECEIVER success");
            return encode_pdu(bind_resp);

        } else if (command_id == CMD_BIND_TRANSMITTER) {
            LOG_INFO("SmppHandler", "BIND_TRANSMITTER parsing PDU body_len={}", pdu_len);
            Smpp::BindTransmitter bind_req(raw_pdu);
            std::string username = std::string(bind_req.system_id());
            std::string password = std::string(bind_req.password());
            LOG_INFO("SmppHandler", "BIND_TRANSMITTER username={}", username);

            if (!validate_credentials(username, password)) {
                LOG_WARN("SmppHandler", "authentication failed");
                Smpp::BindTransmitterResp error_resp;
                error_resp.command_status(0x0000000E);  // ESME_RINVPASWD
                error_resp.sequence_number(sequence_number);
                return encode_pdu(error_resp);
            }

            session.set_system_id(username);
            session.set_state(SmppSession::State::BOUND_TRANSMITTER);

            Smpp::BindTransmitterResp bind_resp;
            bind_resp.command_status(0x00000000);  // ESME_ROK
            bind_resp.sequence_number(sequence_number);
            bind_resp.system_id("TestServer");
            LOG_INFO("SmppHandler", "BIND_TRANSMITTER success");
            return encode_pdu(bind_resp);

        } else {  // TRANSCEIVER
            LOG_INFO("SmppHandler", "BIND_TRANSCEIVER parsing PDU body_len={}", pdu_len);
            Smpp::BindTransceiver bind_req(raw_pdu);
            std::string username = std::string(bind_req.system_id());
            std::string password = std::string(bind_req.password());
            LOG_INFO("SmppHandler", "BIND_TRANSCEIVER username={}", username);

            if (!validate_credentials(username, password)) {
                LOG_WARN("SmppHandler", "authentication failed");
                Smpp::BindTransceiverResp error_resp;
                error_resp.command_status(0x0000000E);  // ESME_RINVPASWD
                error_resp.sequence_number(sequence_number);
                return encode_pdu(error_resp);
            }

            session.set_system_id(username);
            session.set_state(SmppSession::State::BOUND_TRANSCEIVER);

            Smpp::BindTransceiverResp bind_resp;
            bind_resp.command_status(0x00000000);  // ESME_ROK
            bind_resp.sequence_number(sequence_number);
            bind_resp.system_id("TestServer");
            LOG_INFO("SmppHandler", "BIND_TRANSCEIVER success");
            return encode_pdu(bind_resp);
        }

    } catch (const std::exception& ex) {
        LOG_ERROR("SmppHandler", "exception in bind: {}", ex.what());
        if (command_id == CMD_BIND_RECEIVER) {
            Smpp::BindReceiverResp error_resp;
            error_resp.command_status(0x0000000E);  // ESME_RINVPASWD
            error_resp.sequence_number(sequence_number);
            return encode_pdu(error_resp);
        } else if (command_id == CMD_BIND_TRANSMITTER) {
            Smpp::BindTransmitterResp error_resp;
            error_resp.command_status(0x0000000E);  // ESME_RINVPASWD
            error_resp.sequence_number(sequence_number);
            return encode_pdu(error_resp);
        } else {
            Smpp::BindTransceiverResp error_resp;
            error_resp.command_status(0x0000000E);  // ESME_RINVPASWD
            error_resp.sequence_number(sequence_number);
            return encode_pdu(error_resp);
        }
    }
}


std::vector<uint8_t> SmppHandler::build_unbind_resp(
    uint32_t status,
    uint32_t sequence_number)
{
    Smpp::UnbindResp resp;
    resp.command_status(status);
    resp.sequence_number(sequence_number);
    LOG_INFO("SmppHandler", "built UNBIND_RESP status=0x{:08x}", status);
    return encode_pdu(resp);
}

std::vector<uint8_t> SmppHandler::build_enquire_link_resp(
    uint32_t status,
    uint32_t sequence_number)
{
    Smpp::EnquireLinkResp resp;
    resp.command_status(status);
    resp.sequence_number(sequence_number);
    LOG_INFO("SmppHandler", "built ENQUIRE_LINK_RESP status=0x{:08x}", status);
    return encode_pdu(resp);
}

std::vector<uint8_t> SmppHandler::build_generic_nack(
    uint32_t status,
    uint32_t sequence_number)
{
    Smpp::GenericNack resp;
    resp.command_status(status);
    resp.sequence_number(sequence_number);
    LOG_INFO("SmppHandler", "built GENERIC_NACK status=0x{:08x}", status);
    return encode_pdu(resp);
}

bool SmppHandler::validate_credentials(
    const std::string& username,
    const std::string& password)
{
    const std::string ALLOWED_USERNAME = "test";
    const std::string ALLOWED_PASSWORD = "test";
    return (username == ALLOWED_USERNAME) && (password == ALLOWED_PASSWORD);
}
