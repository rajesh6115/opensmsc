#pragma once

#include <memory>
#include <functional>
#include <asio.hpp>
#include <spdlog/spdlog.h>
#include "smpp_state_machine_v2.hpp"

namespace smpp_client {

class SmppClientHandler {
public:
    using ErrorCallback = std::function<void(const std::string&)>;

    explicit SmppClientHandler(int socket_fd);
    ~SmppClientHandler();

    SmppClientHandler(const SmppClientHandler&) = delete;
    SmppClientHandler& operator=(const SmppClientHandler&) = delete;
    SmppClientHandler(SmppClientHandler&&) = delete;
    SmppClientHandler& operator=(SmppClientHandler&&) = delete;

    // Initialize the handler with the file descriptor
    void init();

    // Start processing PDUs
    void start();

    // Stop processing PDUs
    void stop();

    // Set error callback
    void set_error_callback(ErrorCallback callback);

    // Get reference to state machine
    SmppStateMachine& get_state_machine() { return state_machine_; }

    // Check if client is bound
    bool is_bound() const { return state_machine_.is_bound(); }

    // Check capabilities
    bool can_send() const { return state_machine_.can_send(); }
    bool can_receive() const { return state_machine_.can_receive(); }

private:
    int socket_fd_;
    std::shared_ptr<spdlog::logger> logger_;
    SmppStateMachine state_machine_;
    ErrorCallback error_callback_;
    bool running_;

    // PDU processing methods
    void process_pdu();
    void handle_bind_request(uint32_t bind_command_id);
    void handle_unbind_request();
    void handle_enquire_link_request();
};

} // namespace smpp_client
