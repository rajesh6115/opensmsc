#include "smpp_state_machine_v2.hpp"
#include "smpp_states.hpp"
#include <spdlog/sinks/stdout_color_sinks.h>

namespace smpp_client {

SmppStateMachine::SmppStateMachine(std::shared_ptr<spdlog::logger> logger)
    : logger_(logger) {
    if (!logger_) {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        std::vector<spdlog::sink_ptr> sinks{console_sink};
        logger_ = std::make_shared<spdlog::logger>("SmppStateMachine", sinks.begin(), sinks.end());
        logger_->set_level(spdlog::level::info);
    }

    init_states();
}

void SmppStateMachine::init_states() {
    // Create all state instances
    idle_state_ = std::make_unique<IdleState>();
    connected_state_ = std::make_unique<ConnectedState>();
    binding_state_ = std::make_unique<BindingState>();
    bound_tx_state_ = std::make_unique<BoundTxState>();
    bound_rx_state_ = std::make_unique<BoundRxState>();
    bound_trx_state_ = std::make_unique<BoundTrxState>();
    unbinding_state_ = std::make_unique<UnbindingState>();
    disconnected_state_ = std::make_unique<DisconnectedState>();
    error_state_ = std::make_unique<ErrorState>();

    // Start in IDLE state
    current_state_ = std::make_unique<IdleState>();
    logger_->info("SmppStateMachine initialized, starting in IDLE state");
}

std::string SmppStateMachine::get_current_state() const {
    return current_state_->state_name();
}

bool SmppStateMachine::is_bound() const {
    auto state = current_state_->state_name();
    return state == "BOUND_TX" || state == "BOUND_RX" || state == "BOUND_TRX";
}

bool SmppStateMachine::can_send() const {
    auto state = current_state_->state_name();
    return state == "BOUND_TX" || state == "BOUND_TRX";
}

bool SmppStateMachine::can_receive() const {
    auto state = current_state_->state_name();
    return state == "BOUND_RX" || state == "BOUND_TRX";
}

// ──────────────────────────────────────────────────────────────────────────────
// PDU Handler Methods
// ──────────────────────────────────────────────────────────────────────────────

void SmppStateMachine::handle_bind(uint32_t bind_type,
                                   const std::string& system_id,
                                   const std::string& password) {
    try {
        current_state_->on_bind(*this, bind_type, system_id, password);
    } catch (const SmppStateException& e) {
        logger_->error("Error handling BIND: {}", e.what());
        transition_to_error(e.what());
    }
}

void SmppStateMachine::handle_bind_resp(uint32_t bind_type, uint32_t status_code) {
    try {
        current_state_->on_bind_resp(*this, bind_type, status_code);
        if (status_code == 0x00000000) {  // ESME_ROK
            invoke_callbacks_for_bind_success(bind_type);
        }
    } catch (const SmppStateException& e) {
        logger_->error("Error handling BIND_RESP: {}", e.what());
        transition_to_error(e.what());
    }
}

void SmppStateMachine::handle_unbind() {
    try {
        current_state_->on_unbind(*this);
    } catch (const SmppStateException& e) {
        logger_->error("Error handling UNBIND: {}", e.what());
        transition_to_error(e.what());
    }
}

void SmppStateMachine::handle_unbind_resp(uint32_t status_code) {
    try {
        current_state_->on_unbind_resp(*this, status_code);
        if (status_code == 0x00000000) {  // ESME_ROK
            invoke_callbacks_for_unbind_success();
        }
    } catch (const SmppStateException& e) {
        logger_->error("Error handling UNBIND_RESP: {}", e.what());
        transition_to_error(e.what());
    }
}

void SmppStateMachine::handle_enquire_link() {
    try {
        current_state_->on_enquire_link(*this);
    } catch (const SmppStateException& e) {
        logger_->error("Error handling ENQUIRE_LINK: {}", e.what());
        transition_to_error(e.what());
    }
}

void SmppStateMachine::handle_enquire_link_resp(uint32_t status_code) {
    try {
        current_state_->on_enquire_link_resp(*this, status_code);
    } catch (const SmppStateException& e) {
        logger_->error("Error handling ENQUIRE_LINK_RESP: {}", e.what());
        transition_to_error(e.what());
    }
}

void SmppStateMachine::handle_submit_sm(const std::string& source_addr,
                                       const std::string& dest_addr,
                                       const std::string& message) {
    try {
        current_state_->on_submit_sm(*this, source_addr, dest_addr, message);
    } catch (const SmppStateException& e) {
        logger_->error("Error handling SUBMIT_SM: {}", e.what());
        transition_to_error(e.what());
    }
}

void SmppStateMachine::handle_submit_sm_resp(uint32_t status_code,
                                            const std::string& message_id) {
    try {
        current_state_->on_submit_sm_resp(*this, status_code, message_id);
    } catch (const SmppStateException& e) {
        logger_->error("Error handling SUBMIT_SM_RESP: {}", e.what());
        transition_to_error(e.what());
    }
}

void SmppStateMachine::handle_deliver_sm(const std::string& source_addr,
                                        const std::string& dest_addr,
                                        const std::string& message) {
    try {
        current_state_->on_deliver_sm(*this, source_addr, dest_addr, message);
    } catch (const SmppStateException& e) {
        logger_->error("Error handling DELIVER_SM: {}", e.what());
        transition_to_error(e.what());
    }
}

void SmppStateMachine::handle_deliver_sm_resp(uint32_t status_code) {
    try {
        current_state_->on_deliver_sm_resp(*this, status_code);
    } catch (const SmppStateException& e) {
        logger_->error("Error handling DELIVER_SM_RESP: {}", e.what());
        transition_to_error(e.what());
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// State Transition Methods
// ──────────────────────────────────────────────────────────────────────────────

void SmppStateMachine::transition_to_connected() {
    transition_to(connected_state_.get(), "CONNECTED");
}

void SmppStateMachine::transition_to_binding() {
    transition_to(binding_state_.get(), "BINDING");
}

void SmppStateMachine::transition_to_bound_tx() {
    transition_to(bound_tx_state_.get(), "BOUND_TX");
}

void SmppStateMachine::transition_to_bound_rx() {
    transition_to(bound_rx_state_.get(), "BOUND_RX");
}

void SmppStateMachine::transition_to_bound_trx() {
    transition_to(bound_trx_state_.get(), "BOUND_TRX");
}

void SmppStateMachine::transition_to_unbinding() {
    transition_to(unbinding_state_.get(), "UNBINDING");
}

void SmppStateMachine::transition_to_disconnected() {
    transition_to(disconnected_state_.get(), "DISCONNECTED");
}

void SmppStateMachine::transition_to_error(const std::string& error_msg) {
    transition_to(error_state_.get(), "ERROR_STATE");
    if (error_callback_) {
        error_callback_(error_msg);
    }
}

void SmppStateMachine::transition_to(SmppStateBase* new_state, const std::string& state_name) {
    std::string old_state = current_state_->state_name();

    if (old_state == state_name) {
        return;  // No change
    }

    current_state_.reset(new_state);

    logger_->info("State transition: {} -> {}", old_state, state_name);

    if (state_transition_callback_) {
        state_transition_callback_(old_state, state_name);
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Private Helper Methods
// ──────────────────────────────────────────────────────────────────────────────

void SmppStateMachine::invoke_callbacks_for_bind_success(uint32_t bind_type) {
    if (bind_success_callback_) {
        bind_success_callback_(bind_type);
    }
}

void SmppStateMachine::invoke_callbacks_for_unbind_success() {
    if (unbind_success_callback_) {
        unbind_success_callback_();
    }
}

// Destructor defined here after all state class definitions are visible
SmppStateMachine::~SmppStateMachine() = default;

} // namespace smpp_client
