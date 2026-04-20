#pragma once

#include <memory>
#include <functional>
#include <string>
#include <spdlog/spdlog.h>
#include "smpp_state_base.hpp"
#include "smpp_states.hpp"

namespace smpp_client {

// Forward declarations (now included in smpp_states.hpp)
// class SmppStateBase;
// class IdleState;
// class ConnectedState;
// class BindingState;
// class BoundTxState;
// class BoundRxState;
// class BoundTrxState;
// class UnbindingState;
// class DisconnectedState;
// class ErrorState;

// Callbacks for state transitions and events
using StateTransitionCallback = std::function<void(const std::string& from, const std::string& to)>;
using BindSuccessCallback = std::function<void(uint32_t bind_type)>;
using UnbindSuccessCallback = std::function<void()>;
using ErrorCallback = std::function<void(const std::string& error_msg)>;

// ──────────────────────────────────────────────────────────────────────────────
// SmppStateMachine - Orchestrates state transitions and PDU handling
// ──────────────────────────────────────────────────────────────────────────────
class SmppStateMachine {
public:
    explicit SmppStateMachine(std::shared_ptr<spdlog::logger> logger = nullptr);
    ~SmppStateMachine();

    SmppStateMachine(const SmppStateMachine&) = delete;
    SmppStateMachine& operator=(const SmppStateMachine&) = delete;
    SmppStateMachine(SmppStateMachine&&) = default;
    SmppStateMachine& operator=(SmppStateMachine&&) = default;

    // ──── State Management ────
    std::string get_current_state() const;
    SmppStateBase& get_state_impl() { return *current_state_; }

    // ──── Connection State Helpers ────
    bool is_bound() const;
    bool can_send() const;      // BOUND_TX or BOUND_TRX
    bool can_receive() const;   // BOUND_RX or BOUND_TRX

    // ──── PDU Handlers (public interface for SmppClientHandler) ────

    // BIND PDU handling
    void handle_bind(uint32_t bind_type, const std::string& system_id, const std::string& password);

    // BIND Response handling
    void handle_bind_resp(uint32_t bind_type, uint32_t status_code);

    // UNBIND PDU handling
    void handle_unbind();

    // UNBIND Response handling
    void handle_unbind_resp(uint32_t status_code);

    // ENQUIRE_LINK PDU handling
    void handle_enquire_link();

    // ENQUIRE_LINK Response handling
    void handle_enquire_link_resp(uint32_t status_code);

    // Future Phase 2 - SMS Operations
    void handle_submit_sm(const std::string& source_addr,
                         const std::string& dest_addr,
                         const std::string& message);

    void handle_submit_sm_resp(uint32_t status_code, const std::string& message_id);

    void handle_deliver_sm(const std::string& source_addr,
                          const std::string& dest_addr,
                          const std::string& message);

    void handle_deliver_sm_resp(uint32_t status_code);

    // ──── Callback Registration ────
    void on_state_transition(StateTransitionCallback callback) {
        state_transition_callback_ = callback;
    }

    void on_bind_success(BindSuccessCallback callback) {
        bind_success_callback_ = callback;
    }

    void on_unbind_success(UnbindSuccessCallback callback) {
        unbind_success_callback_ = callback;
    }

    void on_error(ErrorCallback callback) {
        error_callback_ = callback;
    }

    // ──── State Transition Methods (called by state implementations) ────
    void transition_to_connected();
    void transition_to_binding();
    void transition_to_bound_tx();
    void transition_to_bound_rx();
    void transition_to_bound_trx();
    void transition_to_unbinding();
    void transition_to_disconnected();
    void transition_to_error(const std::string& error_msg);

private:
    std::shared_ptr<spdlog::logger> logger_;
    std::unique_ptr<SmppStateBase> current_state_;

    // Callback functions
    StateTransitionCallback state_transition_callback_;
    BindSuccessCallback bind_success_callback_;
    UnbindSuccessCallback unbind_success_callback_;
    ErrorCallback error_callback_;

    // State instances (reusable)
    std::unique_ptr<IdleState> idle_state_;
    std::unique_ptr<ConnectedState> connected_state_;
    std::unique_ptr<BindingState> binding_state_;
    std::unique_ptr<BoundTxState> bound_tx_state_;
    std::unique_ptr<BoundRxState> bound_rx_state_;
    std::unique_ptr<BoundTrxState> bound_trx_state_;
    std::unique_ptr<UnbindingState> unbinding_state_;
    std::unique_ptr<DisconnectedState> disconnected_state_;
    std::unique_ptr<ErrorState> error_state_;

    // Helper methods
    void init_states();
    void transition_to(SmppStateBase* new_state, const std::string& state_name);
    void invoke_callbacks_for_bind_success(uint32_t bind_type);
    void invoke_callbacks_for_unbind_success();
};

} // namespace smpp_client
