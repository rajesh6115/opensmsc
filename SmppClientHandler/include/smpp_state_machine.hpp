#pragma once

#include <memory>
#include <functional>
#include <string>
#include <spdlog/spdlog.h>

namespace smpp_client {

// SMPP Protocol States
enum class SmppState {
    IDLE,           // Initial state, no connection
    CONNECTED,      // Socket connected, waiting for BIND
    BINDING,        // BIND request sent, waiting for response
    BOUND_TX,       // Bound as TRANSMITTER (send-only)
    BOUND_RX,       // Bound as RECEIVER (receive-only)
    BOUND_TRX,      // Bound as TRANSCEIVER (send & receive)
    UNBINDING,      // UNBIND request sent, waiting for response
    DISCONNECTED,   // Connection closed or error
    ERROR_STATE     // Unrecoverable error state
};

// Event triggered by PDU reception or timeout
// Command IDs and status codes are from smppcxx::CommandId and smppcxx::CommandStatus enums
struct SmppEvent {
    uint32_t command_id;       // PDU command ID (from Smpp::CommandId enum)
    uint32_t status_code;      // Response status code (from Smpp::CommandStatus enum)
    std::string error_msg;
    uint32_t sequence_num;

    SmppEvent() : command_id(0), status_code(0), sequence_num(0) {}
    SmppEvent(uint32_t cmd_id, uint32_t status = 0, const std::string& msg = "")
        : command_id(cmd_id), status_code(status), error_msg(msg), sequence_num(0) {}
};

// CommandId constants from smppcxx (copied for reference)
namespace CommandIdValues {
    constexpr uint32_t BindReceiver          = 0x00000001;
    constexpr uint32_t BindTransmitter       = 0x00000002;
    constexpr uint32_t BindTransceiver       = 0x00000009;
    constexpr uint32_t Unbind                = 0x00000006;
    constexpr uint32_t EnquireLink           = 0x00000015;
    constexpr uint32_t BindReceiverResp      = 0x80000001;
    constexpr uint32_t BindTransmitterResp   = 0x80000002;
    constexpr uint32_t BindTransceiverResp   = 0x80000009;
    constexpr uint32_t UnbindResp            = 0x80000006;
    constexpr uint32_t EnquireLinkResp       = 0x80000015;
}

// CommandStatus constants from smppcxx (copied for reference)
namespace CommandStatusValues {
    constexpr uint32_t ESME_ROK              = 0x00000000;  // No Error
    constexpr uint32_t ESME_RINVMSGLEN       = 0x00000001;  // Invalid message length
    constexpr uint32_t ESME_RINVCMDLEN       = 0x00000002;  // Invalid command length
    constexpr uint32_t ESME_RINVBNDSTS       = 0x00000004;  // Incorrect BIND Status
    constexpr uint32_t ESME_RSYSERR          = 0x00000008;  // System error
}

// State Machine Callbacks
using StateTransitionCallback = std::function<void(SmppState from, SmppState to)>;
using PduHandlerCallback = std::function<void(const SmppEvent&)>;
using ErrorCallback = std::function<void(const std::string&)>;

class SmppStateMachine {
public:
    explicit SmppStateMachine(std::shared_ptr<spdlog::logger> logger = nullptr);
    ~SmppStateMachine() = default;

    SmppStateMachine(const SmppStateMachine&) = delete;
    SmppStateMachine& operator=(const SmppStateMachine&) = delete;
    SmppStateMachine(SmppStateMachine&&) = default;
    SmppStateMachine& operator=(SmppStateMachine&&) = default;

    // Get current state
    SmppState get_state() const { return current_state_; }

    // Get state as string
    std::string state_to_string(SmppState state) const;

    // Process incoming events
    void process_event(const SmppEvent& event);

    // Request state transitions
    bool request_bind(uint32_t bind_command_id);  // Use CommandIdValues::BindTransmitter, etc.
    bool request_unbind();
    bool request_enquire_link();

    // Register callbacks
    void on_state_transition(StateTransitionCallback callback) {
        state_transition_callback_ = callback;
    }

    void on_bind_success(PduHandlerCallback callback) {
        bind_success_callback_ = callback;
    }

    void on_unbind_success(PduHandlerCallback callback) {
        unbind_success_callback_ = callback;
    }

    void on_error(ErrorCallback callback) {
        error_callback_ = callback;
    }

    // Check if client is bound (TX, RX, or TRX)
    bool is_bound() const;

    // Check if client can send (TX or TRX)
    bool can_send() const;

    // Check if client can receive (RX or TRX)
    bool can_receive() const;

private:
    SmppState current_state_;
    std::shared_ptr<spdlog::logger> logger_;

    // Callbacks
    StateTransitionCallback state_transition_callback_;
    PduHandlerCallback bind_success_callback_;
    PduHandlerCallback unbind_success_callback_;
    ErrorCallback error_callback_;

    // State transition handlers
    void handle_event_in_connected(const SmppEvent& event);
    void handle_event_in_binding(const SmppEvent& event);
    void handle_event_in_bound_tx(const SmppEvent& event);
    void handle_event_in_bound_rx(const SmppEvent& event);
    void handle_event_in_bound_trx(const SmppEvent& event);
    void handle_event_in_unbinding(const SmppEvent& event);

    // Helper methods
    void transition_to(SmppState new_state);
    void handle_bind_response(const SmppEvent& event);
    void handle_unbind_response(const SmppEvent& event);
    void handle_error(const std::string& error_msg);
};

} // namespace smpp_client
