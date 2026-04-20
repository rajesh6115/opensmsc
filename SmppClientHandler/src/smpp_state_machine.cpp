#include "smpp_state_machine.hpp"
#include <spdlog/sinks/stdout_color_sinks.h>

namespace smpp_client {

// Helper to check if command_id is a BIND response
bool is_bind_response(uint32_t cmd_id) {
    return cmd_id == CommandIdValues::BindTransmitterResp ||
           cmd_id == CommandIdValues::BindReceiverResp ||
           cmd_id == CommandIdValues::BindTransceiverResp;
}

// Helper to get bind state from response command_id
SmppState get_bind_state_from_response(uint32_t cmd_id) {
    if (cmd_id == CommandIdValues::BindTransmitterResp) return SmppState::BOUND_TX;
    if (cmd_id == CommandIdValues::BindReceiverResp) return SmppState::BOUND_RX;
    if (cmd_id == CommandIdValues::BindTransceiverResp) return SmppState::BOUND_TRX;
    return SmppState::ERROR_STATE;
}

SmppStateMachine::SmppStateMachine(std::shared_ptr<spdlog::logger> logger)
    : current_state_(SmppState::IDLE), logger_(logger) {
    if (!logger_) {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        std::vector<spdlog::sink_ptr> sinks{console_sink};
        logger_ = std::make_shared<spdlog::logger>("SmppStateMachine", sinks.begin(), sinks.end());
        logger_->set_level(spdlog::level::info);
    }
}

std::string SmppStateMachine::state_to_string(SmppState state) const {
    switch (state) {
        case SmppState::IDLE:           return "IDLE";
        case SmppState::CONNECTED:      return "CONNECTED";
        case SmppState::BINDING:        return "BINDING";
        case SmppState::BOUND_TX:       return "BOUND_TX";
        case SmppState::BOUND_RX:       return "BOUND_RX";
        case SmppState::BOUND_TRX:      return "BOUND_TRX";
        case SmppState::UNBINDING:      return "UNBINDING";
        case SmppState::DISCONNECTED:   return "DISCONNECTED";
        case SmppState::ERROR_STATE:    return "ERROR_STATE";
        default:                        return "UNKNOWN";
    }
}

void SmppStateMachine::transition_to(SmppState new_state) {
    if (current_state_ == new_state) {
        return;
    }

    SmppState old_state = current_state_;
    current_state_ = new_state;

    logger_->info("State transition: {} -> {}", state_to_string(old_state), state_to_string(new_state));

    if (state_transition_callback_) {
        state_transition_callback_(old_state, new_state);
    }
}

bool SmppStateMachine::request_bind(uint32_t bind_command_id) {
    if (current_state_ != SmppState::CONNECTED) {
        logger_->warn("Cannot request BIND from state: {}", state_to_string(current_state_));
        return false;
    }

    if (bind_command_id != CommandIdValues::BindTransmitter &&
        bind_command_id != CommandIdValues::BindReceiver &&
        bind_command_id != CommandIdValues::BindTransceiver) {
        logger_->error("Invalid BIND type: 0x{:08x}", bind_command_id);
        return false;
    }

    transition_to(SmppState::BINDING);
    logger_->info("BIND request initiated: 0x{:08x}", bind_command_id);
    return true;
}

bool SmppStateMachine::request_unbind() {
    if (!is_bound()) {
        logger_->warn("Cannot request UNBIND from state: {}", state_to_string(current_state_));
        return false;
    }

    transition_to(SmppState::UNBINDING);
    logger_->info("UNBIND request initiated");
    return true;
}

bool SmppStateMachine::request_enquire_link() {
    if (!is_bound()) {
        logger_->warn("Cannot send ENQUIRE_LINK from state: {}", state_to_string(current_state_));
        return false;
    }

    logger_->debug("ENQUIRE_LINK sent, state remains: {}", state_to_string(current_state_));
    return true;
}

bool SmppStateMachine::is_bound() const {
    return current_state_ == SmppState::BOUND_TX ||
           current_state_ == SmppState::BOUND_RX ||
           current_state_ == SmppState::BOUND_TRX;
}

bool SmppStateMachine::can_send() const {
    return current_state_ == SmppState::BOUND_TX ||
           current_state_ == SmppState::BOUND_TRX;
}

bool SmppStateMachine::can_receive() const {
    return current_state_ == SmppState::BOUND_RX ||
           current_state_ == SmppState::BOUND_TRX;
}

void SmppStateMachine::process_event(const SmppEvent& event) {
    logger_->debug("Processing event: PDU type = {}, Status = {}", (int)event.command_id, event.status_code);

    switch (current_state_) {
        case SmppState::IDLE:
            logger_->warn("Received event in IDLE state, ignoring");
            break;

        case SmppState::CONNECTED:
            handle_event_in_connected(event);
            break;

        case SmppState::BINDING:
            handle_event_in_binding(event);
            break;

        case SmppState::BOUND_TX:
            handle_event_in_bound_tx(event);
            break;

        case SmppState::BOUND_RX:
            handle_event_in_bound_rx(event);
            break;

        case SmppState::BOUND_TRX:
            handle_event_in_bound_trx(event);
            break;

        case SmppState::UNBINDING:
            handle_event_in_unbinding(event);
            break;

        case SmppState::DISCONNECTED:
        case SmppState::ERROR_STATE:
            logger_->debug("Ignoring event in terminal state: {}", state_to_string(current_state_));
            break;
    }
}

void SmppStateMachine::handle_event_in_connected(const SmppEvent& event) {
    // In CONNECTED state, only BIND requests are expected
    // This handler is typically not called here; BIND requests come from client
    logger_->warn("Unexpected event in CONNECTED state");
}

void SmppStateMachine::handle_event_in_binding(const SmppEvent& event) {
    // Expecting BIND_*_RESP
    if (is_bind_response(event.command_id)) {
        handle_bind_response(event);
    } else {
        handle_error("Unexpected PDU in BINDING state: 0x" + std::to_string(event.command_id));
    }
}

void SmppStateMachine::handle_event_in_bound_tx(const SmppEvent& event) {
    if (event.command_id == CommandIdValues::EnquireLink) {
        logger_->debug("ENQUIRE_LINK received in BOUND_TX, sending response");
    } else if (event.command_id == CommandIdValues::Unbind) {
        logger_->info("UNBIND received in BOUND_TX");
        transition_to(SmppState::UNBINDING);
    } else {
        logger_->warn("Unexpected PDU in BOUND_TX: 0x{:08x}", event.command_id);
    }
}

void SmppStateMachine::handle_event_in_bound_rx(const SmppEvent& event) {
    if (event.command_id == CommandIdValues::EnquireLink) {
        logger_->debug("ENQUIRE_LINK received in BOUND_RX, sending response");
    } else if (event.command_id == CommandIdValues::Unbind) {
        logger_->info("UNBIND received in BOUND_RX");
        transition_to(SmppState::UNBINDING);
    } else {
        logger_->warn("Unexpected PDU in BOUND_RX: 0x{:08x}", event.command_id);
    }
}

void SmppStateMachine::handle_event_in_bound_trx(const SmppEvent& event) {
    if (event.command_id == CommandIdValues::EnquireLink) {
        logger_->debug("ENQUIRE_LINK received in BOUND_TRX, sending response");
    } else if (event.command_id == CommandIdValues::Unbind) {
        logger_->info("UNBIND received in BOUND_TRX");
        transition_to(SmppState::UNBINDING);
    } else {
        logger_->warn("Unexpected PDU in BOUND_TRX: 0x{:08x}", event.command_id);
    }
}

void SmppStateMachine::handle_event_in_unbinding(const SmppEvent& event) {
    if (event.command_id == CommandIdValues::UnbindResp) {
        handle_unbind_response(event);
    } else {
        handle_error("Unexpected PDU in UNBINDING state: 0x" + std::to_string(event.command_id));
    }
}

void SmppStateMachine::handle_bind_response(const SmppEvent& event) {
    if (event.status_code == CommandStatusValues::ESME_ROK) {  // Success
        // Determine bound state based on response type
        SmppState new_state = get_bind_state_from_response(event.command_id);
        transition_to(new_state);

        logger_->info("BIND successful, state: {}", state_to_string(current_state_));

        if (bind_success_callback_) {
            bind_success_callback_(event);
        }
    } else {
        // BIND failed
        handle_error("BIND failed with status code: 0x" + std::to_string(event.status_code));
        transition_to(SmppState::DISCONNECTED);
    }
}

void SmppStateMachine::handle_unbind_response(const SmppEvent& event) {
    if (event.status_code == CommandStatusValues::ESME_ROK) {  // Success
        logger_->info("UNBIND successful");
        transition_to(SmppState::DISCONNECTED);

        if (unbind_success_callback_) {
            unbind_success_callback_(event);
        }
    } else {
        handle_error("UNBIND failed with status code: 0x" + std::to_string(event.status_code));
        transition_to(SmppState::ERROR_STATE);
    }
}

void SmppStateMachine::handle_error(const std::string& error_msg) {
    logger_->error("State machine error: {}", error_msg);
    transition_to(SmppState::ERROR_STATE);

    if (error_callback_) {
        error_callback_(error_msg);
    }
}

} // namespace smpp_client
