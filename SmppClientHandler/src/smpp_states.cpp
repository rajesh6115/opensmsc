#include "smpp_states.hpp"
#include "smpp_state_machine_v2.hpp"
#include <spdlog/spdlog.h>

namespace smpp_client {

// ──────────────────────────────────────────────────────────────────────────────
// ConnectedState Implementation
// ──────────────────────────────────────────────────────────────────────────────
void ConnectedState::on_bind(SmppStateMachine& context,
                             uint32_t bind_type,
                             const std::string& system_id,
                             const std::string& password) {
    spdlog::debug("CONNECTED: Accepting BIND request - type=0x{:08x}, system_id={}",
                  bind_type, system_id);

    // TODO: Validate credentials (system_id, password)
    // TODO: Check against allowed IPs
    // TODO: Generate bind response

    // Transition to BINDING state while waiting for response
    context.transition_to_binding();

    // Later, when bind response is received, it will call handle_bind_resp()
    // which will transition to BOUND_TX, BOUND_RX, or BOUND_TRX
}

// ──────────────────────────────────────────────────────────────────────────────
// BindingState Implementation
// ──────────────────────────────────────────────────────────────────────────────
void BindingState::on_bind_resp(SmppStateMachine& context,
                                uint32_t bind_type,
                                uint32_t status_code) {
    spdlog::debug("BINDING: Processing BIND response - type=0x{:08x}, status=0x{:08x}",
                  bind_type, status_code);

    constexpr uint32_t ESME_ROK = 0x00000000;  // Success

    if (status_code == ESME_ROK) {
        // Successful BIND - transition to appropriate BOUND state
        if (bind_type == 0x80000002) {  // BindTransmitterResp
            context.transition_to_bound_tx();
        } else if (bind_type == 0x80000001) {  // BindReceiverResp
            context.transition_to_bound_rx();
        } else if (bind_type == 0x80000009) {  // BindTransceiverResp
            context.transition_to_bound_trx();
        }
    } else {
        // BIND failed
        spdlog::error("BINDING: BIND failed with status 0x{:08x}", status_code);
        context.transition_to_disconnected();
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// BoundTxState Implementation
// ──────────────────────────────────────────────────────────────────────────────
void BoundTxState::on_unbind(SmppStateMachine& context) {
    spdlog::info("BOUND_TX: Processing UNBIND request");
    // Transition to UNBINDING while waiting for UNBIND_RESP
    context.transition_to_unbinding();
}

void BoundTxState::on_enquire_link(SmppStateMachine& context) {
    spdlog::debug("BOUND_TX: Received ENQUIRE_LINK, sending response");
    // TODO: Send ENQUIRE_LINK_RESP to client
    // Remain in BOUND_TX state
}

void BoundTxState::on_enquire_link_resp(SmppStateMachine& context, uint32_t status_code) {
    spdlog::debug("BOUND_TX: Received ENQUIRE_LINK_RESP, status=0x{:08x}", status_code);
    // Keep-alive response received, remain in BOUND_TX state
}

// ──────────────────────────────────────────────────────────────────────────────
// BoundRxState Implementation
// ──────────────────────────────────────────────────────────────────────────────
void BoundRxState::on_unbind(SmppStateMachine& context) {
    spdlog::info("BOUND_RX: Processing UNBIND request");
    context.transition_to_unbinding();
}

void BoundRxState::on_enquire_link(SmppStateMachine& context) {
    spdlog::debug("BOUND_RX: Received ENQUIRE_LINK, sending response");
    // TODO: Send ENQUIRE_LINK_RESP to client
    // Remain in BOUND_RX state
}

void BoundRxState::on_enquire_link_resp(SmppStateMachine& context, uint32_t status_code) {
    spdlog::debug("BOUND_RX: Received ENQUIRE_LINK_RESP, status=0x{:08x}", status_code);
    // Keep-alive response received, remain in BOUND_RX state
}

// ──────────────────────────────────────────────────────────────────────────────
// BoundTrxState Implementation
// ──────────────────────────────────────────────────────────────────────────────
void BoundTrxState::on_unbind(SmppStateMachine& context) {
    spdlog::info("BOUND_TRX: Processing UNBIND request");
    context.transition_to_unbinding();
}

void BoundTrxState::on_enquire_link(SmppStateMachine& context) {
    spdlog::debug("BOUND_TRX: Received ENQUIRE_LINK, sending response");
    // TODO: Send ENQUIRE_LINK_RESP to client
    // Remain in BOUND_TRX state
}

void BoundTrxState::on_enquire_link_resp(SmppStateMachine& context, uint32_t status_code) {
    spdlog::debug("BOUND_TRX: Received ENQUIRE_LINK_RESP, status=0x{:08x}", status_code);
    // Keep-alive response received, remain in BOUND_TRX state
}

// ──────────────────────────────────────────────────────────────────────────────
// UnbindingState Implementation
// ──────────────────────────────────────────────────────────────────────────────
void UnbindingState::on_unbind_resp(SmppStateMachine& context, uint32_t status_code) {
    spdlog::debug("UNBINDING: Processing UNBIND response, status=0x{:08x}", status_code);

    constexpr uint32_t ESME_ROK = 0x00000000;  // Success

    if (status_code == ESME_ROK) {
        spdlog::info("UNBINDING: UNBIND successful, disconnecting");
        context.transition_to_disconnected();
    } else {
        spdlog::error("UNBINDING: UNBIND failed with status 0x{:08x}", status_code);
        context.transition_to_error("UNBIND failed");
    }
}

} // namespace smpp_client
