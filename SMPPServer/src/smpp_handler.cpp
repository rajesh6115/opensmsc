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
}

void SmppHandler::dispatch_pdu(
    const std::vector<uint8_t>& pdu_header,
    const std::vector<uint8_t>& pdu_body,
    SmppSession& session,
    asio::ip::tcp::socket& socket)
{
    // Parse header
    if (pdu_header.size() < 16) {
        LOG_ERROR("SmppHandler", "invalid header length");
        return;
    }

    uint32_t command_id = ntoh32(pdu_header.data() + 4);
    uint32_t sequence_number = ntoh32(pdu_header.data() + 12);

    LOG_INFO("SmppHandler", "received PDU command_id=0x{:08x} seq={}", command_id, sequence_number);

    switch (command_id) {
    case CMD_BIND_RECEIVER:
        handle_bind_receiver(pdu_body, sequence_number, session, socket);
        break;

    case CMD_BIND_TRANSMITTER:
        handle_bind_transmitter(pdu_body, sequence_number, session, socket);
        break;

    case CMD_BIND_TRANSCEIVER:
        handle_bind_transceiver(pdu_body, sequence_number, session, socket);
        break;

    case CMD_UNBIND:
        handle_unbind(sequence_number, session, socket);
        break;

    case CMD_ENQUIRE_LINK:
        handle_enquire_link(sequence_number, session, socket);
        break;

    default:
        LOG_WARN("SmppHandler", "unknown command 0x{:08x}", command_id);
        break;
    }
}

void SmppHandler::handle_bind_receiver(
    const std::vector<uint8_t>& pdu_body,
    uint32_t sequence_number,
    SmppSession& session,
    asio::ip::tcp::socket& socket)
{
    if (session.is_bound()) {
        LOG_WARN("SmppHandler", "already bound");
        Smpp::BindReceiverResp error_resp;
        error_resp.command_status(0x00000005);  // ESME_RALYBND
        error_resp.sequence_number(sequence_number);
        std::vector<uint8_t> resp_buf(error_resp.encode(),
                                       error_resp.encode() + error_resp.command_length());
        asio::error_code ec;
        asio::write(socket, asio::buffer(resp_buf), ec);
        return;
    }

    try {
        Smpp::BindReceiver bind_req(pdu_body.data());
        std::string username = std::string(bind_req.system_id());
        std::string password = std::string(bind_req.password());

        LOG_INFO("SmppHandler", "BIND_RECEIVER username={}", username);

        if (!validate_credentials(username, password)) {
            LOG_WARN("SmppHandler", "authentication failed");
            Smpp::BindReceiverResp error_resp;
            error_resp.command_status(0x0000000E);  // ESME_RINVPASWD
            error_resp.sequence_number(sequence_number);
            std::vector<uint8_t> resp_buf(error_resp.encode(),
                                           error_resp.encode() + error_resp.command_length());
            asio::error_code ec;
            asio::write(socket, asio::buffer(resp_buf), ec);
            return;
        }

        session.set_system_id(username);
        session.set_state(SmppSession::State::BOUND_RECEIVER);

        Smpp::BindReceiverResp bind_resp;
        bind_resp.command_status(0x00000000);  // ESME_ROK
        bind_resp.sequence_number(sequence_number);
        bind_resp.system_id("TestServer");

        std::vector<uint8_t> resp_buf(bind_resp.encode(),
                                       bind_resp.encode() + bind_resp.command_length());

        LOG_INFO("SmppHandler", "sending BIND_RECEIVER_RESP");
        asio::error_code ec;
        asio::write(socket, asio::buffer(resp_buf), ec);
        if (ec) {
            LOG_ERROR("SmppHandler", "write failed: {}", ec.message());
        }

    } catch (const std::exception& ex) {
        LOG_ERROR("SmppHandler", "exception in BIND_RECEIVER: {}", ex.what());
        Smpp::BindReceiverResp error_resp;
        error_resp.command_status(0x0000000E);  // ESME_RINVPASWD
        error_resp.sequence_number(sequence_number);
        std::vector<uint8_t> resp_buf(error_resp.encode(),
                                       error_resp.encode() + error_resp.command_length());
        asio::error_code ec;
        asio::write(socket, asio::buffer(resp_buf), ec);
    }
}

void SmppHandler::handle_bind_transmitter(
    const std::vector<uint8_t>& pdu_body,
    uint32_t sequence_number,
    SmppSession& session,
    asio::ip::tcp::socket& socket)
{
    if (session.is_bound()) {
        LOG_WARN("SmppHandler", "already bound");
        Smpp::BindTransmitterResp error_resp;
        error_resp.command_status(0x00000005);  // ESME_RALYBND
        error_resp.sequence_number(sequence_number);
        std::vector<uint8_t> resp_buf(error_resp.encode(),
                                       error_resp.encode() + error_resp.command_length());
        asio::error_code ec;
        asio::write(socket, asio::buffer(resp_buf), ec);
        return;
    }

    try {
        Smpp::BindTransmitter bind_req(pdu_body.data());
        std::string username = std::string(bind_req.system_id());
        std::string password = std::string(bind_req.password());

        LOG_INFO("SmppHandler", "BIND_TRANSMITTER username={}", username);

        if (!validate_credentials(username, password)) {
            LOG_WARN("SmppHandler", "authentication failed");
            Smpp::BindTransmitterResp error_resp;
            error_resp.command_status(0x0000000E);  // ESME_RINVPASWD
            error_resp.sequence_number(sequence_number);
            std::vector<uint8_t> resp_buf(error_resp.encode(),
                                           error_resp.encode() + error_resp.command_length());
            asio::error_code ec;
            asio::write(socket, asio::buffer(resp_buf), ec);
            return;
        }

        session.set_system_id(username);
        session.set_state(SmppSession::State::BOUND_TRANSMITTER);

        Smpp::BindTransmitterResp bind_resp;
        bind_resp.command_status(0x00000000);  // ESME_ROK
        bind_resp.sequence_number(sequence_number);
        bind_resp.system_id("TestServer");

        std::vector<uint8_t> resp_buf(bind_resp.encode(),
                                       bind_resp.encode() + bind_resp.command_length());

        LOG_INFO("SmppHandler", "sending BIND_TRANSMITTER_RESP");
        asio::error_code ec;
        asio::write(socket, asio::buffer(resp_buf), ec);
        if (ec) {
            LOG_ERROR("SmppHandler", "write failed: {}", ec.message());
        }

    } catch (const std::exception& ex) {
        LOG_ERROR("SmppHandler", "exception in BIND_TRANSMITTER: {}", ex.what());
        Smpp::BindTransmitterResp error_resp;
        error_resp.command_status(0x0000000E);  // ESME_RINVPASWD
        error_resp.sequence_number(sequence_number);
        std::vector<uint8_t> resp_buf(error_resp.encode(),
                                       error_resp.encode() + error_resp.command_length());
        asio::error_code ec;
        asio::write(socket, asio::buffer(resp_buf), ec);
    }
}

void SmppHandler::handle_bind_transceiver(
    const std::vector<uint8_t>& pdu_body,
    uint32_t sequence_number,
    SmppSession& session,
    asio::ip::tcp::socket& socket)
{
    if (session.is_bound()) {
        LOG_WARN("SmppHandler", "already bound");
        Smpp::BindTransceiverResp error_resp;
        error_resp.command_status(0x00000005);  // ESME_RALYBND
        error_resp.sequence_number(sequence_number);
        std::vector<uint8_t> resp_buf(error_resp.encode(),
                                       error_resp.encode() + error_resp.command_length());
        asio::error_code ec;
        asio::write(socket, asio::buffer(resp_buf), ec);
        return;
    }

    try {
        Smpp::BindTransceiver bind_req(pdu_body.data());
        std::string username = std::string(bind_req.system_id());
        std::string password = std::string(bind_req.password());

        LOG_INFO("SmppHandler", "BIND_TRANSCEIVER username={}", username);

        if (!validate_credentials(username, password)) {
            LOG_WARN("SmppHandler", "authentication failed");
            Smpp::BindTransceiverResp error_resp;
            error_resp.command_status(0x0000000E);  // ESME_RINVPASWD
            error_resp.sequence_number(sequence_number);
            std::vector<uint8_t> resp_buf(error_resp.encode(),
                                           error_resp.encode() + error_resp.command_length());
            asio::error_code ec;
            asio::write(socket, asio::buffer(resp_buf), ec);
            return;
        }

        session.set_system_id(username);
        session.set_state(SmppSession::State::BOUND_TRANSCEIVER);

        Smpp::BindTransceiverResp bind_resp;
        bind_resp.command_status(0x00000000);  // ESME_ROK
        bind_resp.sequence_number(sequence_number);
        bind_resp.system_id("TestServer");

        std::vector<uint8_t> resp_buf(bind_resp.encode(),
                                       bind_resp.encode() + bind_resp.command_length());

        LOG_INFO("SmppHandler", "sending BIND_TRANSCEIVER_RESP");
        asio::error_code ec;
        asio::write(socket, asio::buffer(resp_buf), ec);
        if (ec) {
            LOG_ERROR("SmppHandler", "write failed: {}", ec.message());
        }

    } catch (const std::exception& ex) {
        LOG_ERROR("SmppHandler", "exception in BIND_TRANSCEIVER: {}", ex.what());
        Smpp::BindTransceiverResp error_resp;
        error_resp.command_status(0x0000000E);  // ESME_RINVPASWD
        error_resp.sequence_number(sequence_number);
        std::vector<uint8_t> resp_buf(error_resp.encode(),
                                       error_resp.encode() + error_resp.command_length());
        asio::error_code ec;
        asio::write(socket, asio::buffer(resp_buf), ec);
    }
}

void SmppHandler::handle_unbind(
    uint32_t sequence_number,
    SmppSession& session,
    asio::ip::tcp::socket& socket)
{
    if (!session.is_bound()) {
        LOG_WARN("SmppHandler", "UNBIND but not bound");
        Smpp::UnbindResp error_resp;
        error_resp.command_status(0x0000000D);  // ESME_RBINDFAIL (not bound);
        error_resp.sequence_number(sequence_number);
        std::vector<uint8_t> resp_buf(error_resp.encode(),
                                       error_resp.encode() + error_resp.command_length());
        asio::error_code ec;
        asio::write(socket, asio::buffer(resp_buf), ec);
        return;
    }

    LOG_INFO("SmppHandler", "UNBIND");

    Smpp::UnbindResp unbind_resp;
    unbind_resp.command_status(0x00000000);  // ESME_ROK
    unbind_resp.sequence_number(sequence_number);

    std::vector<uint8_t> resp_buf(unbind_resp.encode(),
                                   unbind_resp.encode() + unbind_resp.command_length());

    LOG_INFO("SmppHandler", "sending UNBIND_RESP and closing connection");
    asio::error_code ec;
    asio::write(socket, asio::buffer(resp_buf), ec);

    session.set_state(SmppSession::State::UNBOUND);
    socket.close();
}

void SmppHandler::handle_enquire_link(
    uint32_t sequence_number,
    SmppSession& session,
    asio::ip::tcp::socket& socket)
{
    if (!session.is_bound()) {
        LOG_WARN("SmppHandler", "ENQUIRE_LINK but not bound");
        Smpp::EnquireLinkResp error_resp;
        error_resp.command_status(0x0000000D);  // ESME_RBINDFAIL (not bound);
        error_resp.sequence_number(sequence_number);
        std::vector<uint8_t> resp_buf(error_resp.encode(),
                                       error_resp.encode() + error_resp.command_length());
        asio::error_code ec;
        asio::write(socket, asio::buffer(resp_buf), ec);
        return;
    }

    LOG_INFO("SmppHandler", "ENQUIRE_LINK keep-alive");

    Smpp::EnquireLinkResp enquire_resp;
    enquire_resp.command_status(0x00000000);  // ESME_ROK
    enquire_resp.sequence_number(sequence_number);

    std::vector<uint8_t> resp_buf(enquire_resp.encode(),
                                   enquire_resp.encode() + enquire_resp.command_length());

    LOG_INFO("SmppHandler", "sending ENQUIRE_LINK_RESP");
    asio::error_code ec;
    asio::write(socket, asio::buffer(resp_buf), ec);
    if (ec) {
        LOG_ERROR("SmppHandler", "write failed: {}", ec.message());
    }
}

bool SmppHandler::validate_credentials(
    const std::string& username,
    const std::string& password)
{
    const std::string ALLOWED_USERNAME = "test";
    const std::string ALLOWED_PASSWORD = "test";
    return (username == ALLOWED_USERNAME) && (password == ALLOWED_PASSWORD);
}

std::vector<uint8_t> SmppHandler::create_error_response(
    uint32_t command_id,
    uint32_t status,
    uint32_t sequence_number)
{
    std::vector<uint8_t> response(16);
    uint32_t response_id = command_id | 0x80000000;
    uint32_t command_length = 16;

    std::memcpy(response.data() + 0, &command_length, 4);
    std::memcpy(response.data() + 4, &response_id, 4);
    std::memcpy(response.data() + 8, &status, 4);
    std::memcpy(response.data() + 12, &sequence_number, 4);

    for (int i = 0; i < 4; ++i) {
        std::swap(response[i], response[3 - i]);
        std::swap(response[4 + i], response[7 - i]);
        std::swap(response[8 + i], response[11 - i]);
        std::swap(response[12 + i], response[15 - i]);
    }

    return response;
}
