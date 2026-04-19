#pragma once

#include <asio.hpp>
#include <cstdint>
#include <memory>
#include <vector>

class SmppSession;

class SmppHandler
{
public:
    static void dispatch_pdu(
        const std::vector<uint8_t>& full_pdu,
        SmppSession& session,
        asio::ip::tcp::socket& socket);

private:
    static void handle_bind_transmitter(
        const uint8_t* body_data,
        size_t body_len,
        uint32_t sequence_number,
        SmppSession& session,
        asio::ip::tcp::socket& socket);

    static void handle_bind_receiver(
        const uint8_t* body_data,
        size_t body_len,
        uint32_t sequence_number,
        SmppSession& session,
        asio::ip::tcp::socket& socket);

    static void handle_bind_transceiver(
        const uint8_t* body_data,
        size_t body_len,
        uint32_t sequence_number,
        SmppSession& session,
        asio::ip::tcp::socket& socket);

    static void handle_unbind(
        uint32_t sequence_number,
        SmppSession& session,
        asio::ip::tcp::socket& socket);

    static void handle_enquire_link(
        uint32_t sequence_number,
        SmppSession& session,
        asio::ip::tcp::socket& socket);

    static bool validate_credentials(
        const std::string& username,
        const std::string& password);

    static std::vector<uint8_t> create_error_response(
        uint32_t command_id,
        uint32_t status,
        uint32_t sequence_number);
};
