#pragma once

#include <string>
#include <memory>
#include <spdlog/spdlog.h>

namespace smpp_client {

// Forward declarations
class SmppStateMachine;
class BindPdu;
class UnbindPdu;
class EnquireLinkPdu;

// Exception for unsupported PDU operations in current state
class SmppStateException : public std::runtime_error {
public:
    explicit SmppStateException(const std::string& msg) : std::runtime_error(msg) {}
};

// Base state class - defines interface for all PDU handlers
class SmppStateBase {
public:
    virtual ~SmppStateBase() = default;

    // Get state name for logging/debugging
    virtual std::string state_name() const = 0;

    // PDU handlers - each state implements only what it accepts
    // Other states throw SmppStateException

    // BIND PDU (0x00000001, 0x00000002, 0x00000009)
    virtual void on_bind(SmppStateMachine& context,
                        uint32_t bind_type,           // BindTransmitter, BindReceiver, BindTransceiver
                        const std::string& system_id,
                        const std::string& password) {
        throw SmppStateException(
            "BIND PDU not accepted in state: " + state_name()
        );
    }

    // BIND Response PDU (0x80000001, 0x80000002, 0x80000009)
    virtual void on_bind_resp(SmppStateMachine& context,
                             uint32_t bind_type,
                             uint32_t status_code) {
        throw SmppStateException(
            "BIND_RESP PDU not accepted in state: " + state_name()
        );
    }

    // UNBIND PDU (0x00000006)
    virtual void on_unbind(SmppStateMachine& context) {
        throw SmppStateException(
            "UNBIND PDU not accepted in state: " + state_name()
        );
    }

    // UNBIND Response PDU (0x80000006)
    virtual void on_unbind_resp(SmppStateMachine& context, uint32_t status_code) {
        throw SmppStateException(
            "UNBIND_RESP PDU not accepted in state: " + state_name()
        );
    }

    // ENQUIRE_LINK PDU (0x00000015) - keep-alive request
    virtual void on_enquire_link(SmppStateMachine& context) {
        throw SmppStateException(
            "ENQUIRE_LINK PDU not accepted in state: " + state_name()
        );
    }

    // ENQUIRE_LINK Response PDU (0x80000015) - keep-alive response
    virtual void on_enquire_link_resp(SmppStateMachine& context, uint32_t status_code) {
        throw SmppStateException(
            "ENQUIRE_LINK_RESP PDU not accepted in state: " + state_name()
        );
    }

    // Future Phase 2 - SMS Operations
    virtual void on_submit_sm(SmppStateMachine& context,
                             const std::string& source_addr,
                             const std::string& dest_addr,
                             const std::string& message) {
        throw SmppStateException(
            "SUBMIT_SM PDU not accepted in state: " + state_name()
        );
    }

    virtual void on_submit_sm_resp(SmppStateMachine& context,
                                  uint32_t status_code,
                                  const std::string& message_id) {
        throw SmppStateException(
            "SUBMIT_SM_RESP PDU not accepted in state: " + state_name()
        );
    }

    virtual void on_deliver_sm(SmppStateMachine& context,
                              const std::string& source_addr,
                              const std::string& dest_addr,
                              const std::string& message) {
        throw SmppStateException(
            "DELIVER_SM PDU not accepted in state: " + state_name()
        );
    }

    virtual void on_deliver_sm_resp(SmppStateMachine& context, uint32_t status_code) {
        throw SmppStateException(
            "DELIVER_SM_RESP PDU not accepted in state: " + state_name()
        );
    }

protected:
    SmppStateBase() = default;
};

} // namespace smpp_client
