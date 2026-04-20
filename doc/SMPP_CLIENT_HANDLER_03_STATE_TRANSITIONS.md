# SMPP Client Handler - State Transitions

## Complete State Transition Matrix

| From | Event | To | Condition | Handler |
|------|-------|-----|-----------|---------|
| IDLE | socket_connected | CONNECTED | - | *(external)* |
| CONNECTED | on_bind | BINDING | type ∈ {TX,RX,TRX} | `ConnectedState::on_bind()` |
| BINDING | on_bind_resp | BOUND_TX | cmd=TX_RESP, status=0 | `BindingState::on_bind_resp()` |
| BINDING | on_bind_resp | BOUND_RX | cmd=RX_RESP, status=0 | `BindingState::on_bind_resp()` |
| BINDING | on_bind_resp | BOUND_TRX | cmd=TRX_RESP, status=0 | `BindingState::on_bind_resp()` |
| BINDING | on_bind_resp | DISCONNECTED | status ≠ 0 | `BindingState::on_bind_resp()` |
| BINDING | timeout | DISCONNECTED | 10s | *(external timer)* |
| BOUND_TX | on_enquire_link | BOUND_TX | (self) | `BoundTxState::on_enquire_link()` |
| BOUND_TX | on_enquire_link_resp | BOUND_TX | (self) | `BoundTxState::on_enquire_link_resp()` |
| BOUND_TX | on_unbind | UNBINDING | - | `BoundTxState::on_unbind()` |
| BOUND_RX | on_enquire_link | BOUND_RX | (self) | `BoundRxState::on_enquire_link()` |
| BOUND_RX | on_unbind | UNBINDING | - | `BoundRxState::on_unbind()` |
| BOUND_TRX | on_enquire_link | BOUND_TRX | (self) | `BoundTrxState::on_enquire_link()` |
| BOUND_TRX | on_unbind | UNBINDING | - | `BoundTrxState::on_unbind()` |
| UNBINDING | on_unbind_resp | DISCONNECTED | status = 0 | `UnbindingState::on_unbind_resp()` |
| UNBINDING | on_unbind_resp | ERROR_STATE | status ≠ 0 | `UnbindingState::on_unbind_resp()` |
| UNBINDING | timeout | ERROR_STATE | 10s | *(external timer)* |
| *(Any)* | SmppStateException | ERROR_STATE | Invalid PDU | State machine |
| DISCONNECTED | - | - | Terminal | - |
| ERROR_STATE | - | - | Terminal | - |

## State Capabilities Matrix

```
┌──────────────────┬─────────┬───────────┬─────────┬─────────┬─────────┬────────────┐
│ PDU / State      │ IDLE    │ CONNECTED │ BINDING │ BOUND_TX│ BOUND_RX│ BOUND_TRX  │
├──────────────────┼─────────┼───────────┼─────────┼─────────┼─────────┼────────────┤
│ BIND             │ ✗       │ ✓ (Ph1)   │ ✗       │ ✗       │ ✗       │ ✗          │
│ BIND_RESP        │ ✗       │ ✗         │ ✓ (Ph1) │ ✗       │ ✗       │ ✗          │
│ UNBIND           │ ✗       │ ✗         │ ✗       │ ✓ (Ph1) │ ✓ (Ph1) │ ✓ (Ph1)    │
│ UNBIND_RESP      │ ✗       │ ✗         │ ✗       │ ✗       │ ✗       │ ✗ (wait)   │
│ ENQUIRE_LINK     │ ✗       │ ✗         │ ✗       │ ✓ (Ph1) │ ✓ (Ph1) │ ✓ (Ph1)    │
│ ENQUIRE_LINK_RESP│ ✗       │ ✗         │ ✗       │ ✓ (Ph1) │ ✓ (Ph1) │ ✓ (Ph1)    │
│ SUBMIT_SM        │ ✗       │ ✗         │ ✗       │ ✓ (Ph2)⚠│ ✗       │ ✓ (Ph2)⚠   │
│ DELIVER_SM       │ ✗       │ ✗         │ ✗       │ ✗       │ ✓ (Ph2)⚠│ ✓ (Ph2)⚠   │
└──────────────────┴─────────┴───────────┴─────────┴─────────┴─────────┴────────────┘

Legend: ✓ = Accepted, ✗ = Rejected (exception), ⚠ = Phase 2
```

## ASCII State Diagram

```
                IDLE
                 │
        socket_connected()
                 │
                 ▼
            CONNECTED
                 │
           on_bind(type)
                 │
                 ▼
            BINDING
          ┌──┬──┬──┐
    fail  │  │  │  │  fail
          │  │  │  │
     ┌────┘  │  │  └────┐
     │       │  │       │
 (fail)  (TX)(RX)(TRX) (fail)
     │   │   │   │      │
     ▼   ▼   ▼   ▼      ▼
   ┌──────────────────────────┐
   │ BOUND_TX BOUND_RX BOUND_TRX
   └┬──────────┬──────────┬───┬┘
    │          │          │   │
    │  on_enquire_link()  │   │
    │  [self-transitions] │   │
    │          │          │   │
    └────────┬─┴──────────┴─┬─┘
             │              │
             │ on_unbind()  │
             │              │
             ▼              ▼
         UNBINDING
          ┌──┬──┐
       ok │  │  │ fail
          │  │  │
          ▼  ▼  ▼
    DISCONNECTED ERROR_STATE
        (Term)    (Term)
```

## Timeout Strategy

- **BINDING**: 10 seconds - No BIND response → DISCONNECTED
- **UNBINDING**: 10 seconds - No UNBIND response → ERROR_STATE
- **ENQUIRE_LINK**: 30 seconds interval, 2 missed → disconnect

## State Transition Sequence - Happy Path

```
IDLE
  ↓ (socket connects)
CONNECTED
  ↓ (recv BIND)
BINDING
  ↓ (recv BIND_RESP, success)
BOUND_TX/RX/TRX
  ↓ (periodic ENQUIRE_LINK keep-alive)
BOUND_TX/RX/TRX (self-transitions)
  ↓ (client unbinds)
UNBINDING
  ↓ (recv UNBIND_RESP, success)
DISCONNECTED (graceful close)
```

## Error Scenarios

### BIND Failure
```
CONNECTED
  ↓ (recv BIND)
BINDING
  ↓ (recv BIND_RESP, error status)
DISCONNECTED
```

### Invalid PDU in BOUND_TX
```
BOUND_TX
  ↓ (recv DELIVER_SM - RX only!)
  → SmppStateException
ERROR_STATE
```

### UNBIND Timeout
```
UNBINDING (waiting 10s)
  ↓ (timeout)
ERROR_STATE
```

## Recovery From Terminal States

| Terminal State | Recovery |
|---|---|
| DISCONNECTED | Create new SmppClientHandler with new socket FD |
| ERROR_STATE | Create new SmppClientHandler with new socket FD |
