#pragma once

#include "smpp_pdu_framer.hpp"
#include "smpp_session.hpp"

#include <asio.hpp>
#include <deque>
#include <memory>
#include <string>
#include <vector>
#include <array>

class SmppServiceManager;

class SmppClientHandler
    : public std::enable_shared_from_this<SmppClientHandler>
{
public:
    static constexpr std::size_t READ_BUFFER_SIZE = 4096;

    SmppClientHandler(
        asio::ip::tcp::socket socket,
        std::string session_id,
        std::shared_ptr<SmppServiceManager> service_mgr);

    SmppClientHandler(const SmppClientHandler&) = delete;
    SmppClientHandler& operator=(const SmppClientHandler&) = delete;

    void start();
    void send_pdu(std::vector<uint8_t> pdu);

private:
    void do_read();
    void on_read(const asio::error_code& ec, std::size_t bytes_transferred);

    void dispatch_pdu(const std::vector<uint8_t>& full_pdu);
    void handle_bind(uint32_t command_id,
                     const uint8_t* raw_pdu, std::size_t len,
                     uint32_t sequence_number);
    void handle_unbind(uint32_t sequence_number);
    void handle_enquire_link(uint32_t sequence_number);
    void handle_unknown(uint32_t command_id, uint32_t sequence_number);

    void enqueue_pdu(std::vector<uint8_t> pdu);
    void do_write();
    void on_write(const asio::error_code& ec, std::size_t bytes_transferred);

    void close(const std::string& reason);

    asio::strand<asio::io_context::executor_type> strand_;
    asio::ip::tcp::socket                          socket_;
    std::string                                    session_id_;
    std::shared_ptr<SmppServiceManager>            service_mgr_;

    SmppPduFramer                                  framer_;
    SmppSession                                    session_;

    std::array<uint8_t, READ_BUFFER_SIZE>          read_buf_;
    std::deque<std::vector<uint8_t>>               write_queue_;
    bool                                           write_in_progress_ = false;
    bool                                           close_after_write_  = false;
};
