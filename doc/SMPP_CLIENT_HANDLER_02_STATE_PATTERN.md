# SMPP Client Handler - State Pattern Design

## Overview

The SmppClientHandler uses the **State Pattern** to encapsulate SMPP protocol states. Each state is a separate class that implements only the PDU handlers valid for that state.

### Benefits
- **Type Safety**: Compiler-enforced valid PDU operations per state
- **Encapsulation**: State logic self-contained in state classes
- **Extensibility**: New states/PDUs added without modifying existing code
- **Error Handling**: Invalid PDUs throw `SmppStateException`

## State Hierarchy

```
SmppStateBase (Abstract)
├── IdleState
├── ConnectedState
├── BindingState
├── BoundTxState
├── BoundRxState
├── BoundTrxState
├── UnbindingState
├── DisconnectedState
└── ErrorState
```

## State Descriptions

### IdleState
- **Initial state**, no socket connection
- **Rejects**: All PDUs

### ConnectedState
- Socket connected, waiting for BIND
- **Accepts**: `on_bind(type, system_id, password)`
- **Transitions to**: BINDING

### BindingState
- BIND request sent, waiting for response
- **Accepts**: `on_bind_resp(type, status_code)`
- **Timeout**: 10 seconds
- **Transitions to**: BOUND_TX/RX/TRX (success) or DISCONNECTED (failure)

### BoundTxState (Transmitter)
- Authenticated, send-only mode
- **Accepts**: UNBIND, ENQUIRE_LINK, (Ph2: SUBMIT_SM)
- **Rejects**: DELIVER_SM
- **Transitions to**: UNBINDING

### BoundRxState (Receiver)
- Authenticated, receive-only mode
- **Accepts**: UNBIND, ENQUIRE_LINK, (Ph2: DELIVER_SM)
- **Rejects**: SUBMIT_SM
- **Transitions to**: UNBINDING

### BoundTrxState (Transceiver)
- Authenticated, bidirectional mode
- **Accepts**: UNBIND, ENQUIRE_LINK, (Ph2: SUBMIT_SM, DELIVER_SM)
- **Transitions to**: UNBINDING

### UnbindingState
- UNBIND request sent, waiting for response
- **Accepts**: `on_unbind_resp(status_code)`
- **Timeout**: 10 seconds
- **Transitions to**: DISCONNECTED (success) or ERROR_STATE (failure)

### DisconnectedState / ErrorState (Terminal)
- Connection closed (gracefully or with error)
- **Rejects**: All PDUs
- No outgoing transitions

## Exception Handling

When an invalid PDU is received in a state:

```cpp
throw SmppStateException("PDU_NAME not accepted in state: STATE_NAME");
```

Examples:
- `on_deliver_sm()` in BOUND_TX throws exception (RX-only)
- `on_submit_sm()` in BOUND_RX throws exception (TX-only)
- Any PDU in DISCONNECTED throws exception (terminal state)

The state machine catches exceptions and transitions to ERROR_STATE.

## PDU Handler Interface

All states implement (or inherit exception-throwing defaults for) these methods:

**Phase 1 (Implemented)**
- `on_bind(type, system_id, password)`
- `on_bind_resp(type, status_code)`
- `on_unbind()`
- `on_unbind_resp(status_code)`
- `on_enquire_link()`
- `on_enquire_link_resp(status_code)`

**Phase 2 (Stubs)**
- `on_submit_sm(source_addr, dest_addr, message)`
- `on_submit_sm_resp(status_code, message_id)`
- `on_deliver_sm(source_addr, dest_addr, message)`
- `on_deliver_sm_resp(status_code)`

## Adding Phase 2 Features

To add SUBMIT_SM support for TX mode:

```cpp
// smpp_states.hpp
class BoundTxState : public SmppStateBase {
    void on_submit_sm(SmppStateMachine& context,
                      const std::string& source_addr,
                      const std::string& dest_addr,
                      const std::string& message) override;
};

// smpp_states.cpp
void BoundTxState::on_submit_sm(...) {
    logger->info("Queueing SMS to {}", dest_addr);
    // TODO: Queue for sending
}
```

No changes to other states needed - they inherit exception-throwing default.

## Usage Pattern

```cpp
SmppStateMachine sm(logger);

// PDU handlers automatically route to current state
sm.handle_bind(0x00000002, "user1", "pass");      // CONNECTED → BINDING
sm.handle_bind_resp(0x80000002, 0);                // BINDING → BOUND_TX

sm.handle_enquire_link();                          // BOUND_TX → BOUND_TX
sm.handle_deliver_sm(...);                         // THROWS: not allowed in TX

// All PDU handlers are try-catch protected
// Invalid PDUs transition to ERROR_STATE
```
