#pragma once

#include "smpp_state_base.hpp"

namespace smpp_client {

// Forward declaration
class SmppStateMachine;

// ──────────────────────────────────────────────────────────────────────────────
// IDLE State: Initial state, no connection established
// ──────────────────────────────────────────────────────────────────────────────
class IdleState : public SmppStateBase {
public:
    std::string state_name() const override { return "IDLE"; }
    // All PDU operations throw exception - socket not connected yet
};

// ──────────────────────────────────────────────────────────────────────────────
// CONNECTED State: Socket connected, waiting for BIND
// ──────────────────────────────────────────────────────────────────────────────
class ConnectedState : public SmppStateBase {
public:
    std::string state_name() const override { return "CONNECTED"; }

    // Only BIND PDUs are accepted in this state
    void on_bind(SmppStateMachine& context,
                uint32_t bind_type,
                const std::string& system_id,
                const std::string& password) override;
};

// ──────────────────────────────────────────────────────────────────────────────
// BINDING State: BIND request sent, waiting for BIND_*_RESP
// ──────────────────────────────────────────────────────────────────────────────
class BindingState : public SmppStateBase {
public:
    std::string state_name() const override { return "BINDING"; }

    // Only BIND response PDUs are accepted in this state
    void on_bind_resp(SmppStateMachine& context,
                     uint32_t bind_type,
                     uint32_t status_code) override;
};

// ──────────────────────────────────────────────────────────────────────────────
// BOUND_TX State: Bound as TRANSMITTER (send-only)
// ──────────────────────────────────────────────────────────────────────────────
class BoundTxState : public SmppStateBase {
public:
    std::string state_name() const override { return "BOUND_TX"; }

    // Transmitter can:
    // - Send UNBIND request
    // - Send ENQUIRE_LINK (keep-alive)
    // - Send SUBMIT_SM (Phase 2)
    // - Receive ENQUIRE_LINK from server
    // - Receive SUBMIT_SM_RESP

    void on_unbind(SmppStateMachine& context) override;

    void on_enquire_link(SmppStateMachine& context) override;

    void on_enquire_link_resp(SmppStateMachine& context, uint32_t status_code) override;
};

// ──────────────────────────────────────────────────────────────────────────────
// BOUND_RX State: Bound as RECEIVER (receive-only)
// ──────────────────────────────────────────────────────────────────────────────
class BoundRxState : public SmppStateBase {
public:
    std::string state_name() const override { return "BOUND_RX"; }

    // Receiver can:
    // - Send UNBIND request
    // - Send ENQUIRE_LINK (keep-alive)
    // - Send DELIVER_SM_RESP
    // - Receive ENQUIRE_LINK from server
    // - Receive DELIVER_SM from server

    void on_unbind(SmppStateMachine& context) override;

    void on_enquire_link(SmppStateMachine& context) override;

    void on_enquire_link_resp(SmppStateMachine& context, uint32_t status_code) override;
};

// ──────────────────────────────────────────────────────────────────────────────
// BOUND_TRX State: Bound as TRANSCEIVER (bidirectional)
// ──────────────────────────────────────────────────────────────────────────────
class BoundTrxState : public SmppStateBase {
public:
    std::string state_name() const override { return "BOUND_TRX"; }

    // Transceiver can:
    // - Send UNBIND request
    // - Send ENQUIRE_LINK (keep-alive)
    // - Send SUBMIT_SM (Phase 2)
    // - Send DELIVER_SM_RESP
    // - Receive ENQUIRE_LINK from server
    // - Receive SUBMIT_SM_RESP
    // - Receive DELIVER_SM from server

    void on_unbind(SmppStateMachine& context) override;

    void on_enquire_link(SmppStateMachine& context) override;

    void on_enquire_link_resp(SmppStateMachine& context, uint32_t status_code) override;
};

// ──────────────────────────────────────────────────────────────────────────────
// UNBINDING State: UNBIND request sent, waiting for UNBIND_RESP
// ──────────────────────────────────────────────────────────────────────────────
class UnbindingState : public SmppStateBase {
public:
    std::string state_name() const override { return "UNBINDING"; }

    // Only UNBIND response is accepted in this state
    void on_unbind_resp(SmppStateMachine& context, uint32_t status_code) override;
};

// ──────────────────────────────────────────────────────────────────────────────
// DISCONNECTED State: Terminal state - connection closed gracefully
// ──────────────────────────────────────────────────────────────────────────────
class DisconnectedState : public SmppStateBase {
public:
    std::string state_name() const override { return "DISCONNECTED"; }
    // All PDU operations throw exception - connection is closed
};

// ──────────────────────────────────────────────────────────────────────────────
// ERROR_STATE: Terminal state - unrecoverable error occurred
// ──────────────────────────────────────────────────────────────────────────────
class ErrorState : public SmppStateBase {
public:
    std::string state_name() const override { return "ERROR_STATE"; }
    // All PDU operations throw exception - unrecoverable state
};

} // namespace smpp_client
