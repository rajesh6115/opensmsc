# Protocol Layer: Class Specifications

**Date**: 2026-04-19  
**Version**: 2.0 (Revised to use smppcxx)  
**Layer**: Protocol Layer (SMPP Protocol Handling)  
**Primary**: smppcxx library (external)  
**Custom**: SmppProtocolAdapter (streaming + smppcxx integration)  

---

## Overview

The Protocol Layer uses the proven smppcxx library for SMPP protocol handling.

- **smppcxx**: Handles SMPP PDU parsing/encoding, provides typed objects (BindReceiver, BindTransmitter, etc.)
- **SmppProtocolAdapter**: Handles TCP streaming and bridges smppcxx with our async I/O

No custom protocol parsing—we leverage smppcxx's well-tested implementation.

---

## smppcxx Library Overview

smppcxx provides:

### 1. Header Class Hierarchy

```
Smpp::Header (base)
  ├── Smpp::Request
  │   ├── Smpp::BindReceiver
  │   ├── Smpp::BindTransmitter
  │   ├── Smpp::BindTransceiver
  │   ├── Smpp::Unbind
  │   └── Smpp::EnquireLink
  └── Smpp::Response
      ├── Smpp::BindReceiverResp
      ├── Smpp::BindTransmitterResp
      ├── Smpp::BindTransceiverResp
      ├── Smpp::UnbindResp
      └── Smpp::EnquireLinkResp
```

### 2. Key Methods

Every PDU class has:

```cpp
class BindReceiver : public Smpp::Request {
    // Parse from raw bytes
    BindReceiver(const Smpp::Uint8* buffer);
    
    // Serialize to bytes
    const Smpp::Uint8* encode();
    Smpp::Uint32 command_length() const;
    
    // Field accessors
    const SystemId& system_id() const;
    const Password& password() const;
    
    // Setters
    void sequence_number(const Smpp::SequenceNumber& seq);
    void command_status(Smpp::Uint32 status);
};
```

### 3. Usage Example

```cpp
#include <smpp.hpp>

// Parse incoming PDU
const uint8_t* pdu_bytes = ...;  // Full PDU (header + body)
Smpp::BindReceiver bind_req(pdu_bytes);

// Access fields
std::string user = bind_req.system_id();
std::string pass = bind_req.password();

// Build response
Smpp::BindReceiverResp resp;
resp.sequence_number(bind_req.sequence_number());
resp.command_status(0);  // ESME_ROK = success

// Encode to bytes
const uint8_t* resp_bytes = resp.encode();
size_t resp_len = resp.command_length();
```

---

## Class 1: SmppProtocolAdapter

### Purpose

Bridge between TCP streaming and smppcxx. Handles PDU assembly from streaming bytes and provides typed smppcxx objects.

### Location

- **Header**: `SMPPServer/include/smpp_protocol_adapter.hpp`
- **Source**: `SMPPServer/src/smpp_protocol_adapter.cpp`

### Responsibilities

**Streaming Input Handling**:
- Buffer incoming TCP bytes
- Detect complete SMPP PDUs (command_length validation)
- Assemble partial messages across multiple TCP reads

**PDU Object Creation**:
- Convert raw bytes → typed smppcxx object (BindReceiver, BindTransmitter, etc.)
- Parse command_id from header to determine correct class to instantiate
- Return polymorphic Smpp::Header* pointer

**Response Encoding**:
- Take smppcxx response object (BindReceiverResp, etc.)
- Encode to bytes via smppcxx's encode() method
- Return byte buffer for TCP transmission

### Class Signature

```cpp
class SmppProtocolAdapter {
public:
    SmppProtocolAdapter();
    
    // Feed TCP bytes, get complete SMPP PDUs
    std::vector<std::unique_ptr<Smpp::Header>> process_tcp_data(
        const std::vector<uint8_t>& tcp_bytes);
    
    // Encode smppcxx response to TCP bytes
    std::vector<uint8_t> encode_response(const Smpp::Header& response);
    
    // Query state
    bool has_complete_pdu() const;
    size_t bytes_needed() const;
    
private:
    // Streaming state
    std::vector<uint8_t> buffer_;
    
    // PDU assembly helpers
    bool can_extract_pdu_() const;
    std::unique_ptr<Smpp::Header> extract_pdu_();
    std::unique_ptr<Smpp::Header> parse_pdu_(const uint8_t* pdu_bytes);
    
    // Helper to get correct class from command_id
    std::unique_ptr<Smpp::Header> instantiate_pdu_(
        const uint8_t* pdu_bytes,
        Smpp::Uint32 command_id);
};
```

### Processing Flow

```
process_tcp_data(tcp_bytes)
  ├─ Append tcp_bytes to buffer_
  │
  ├─ While has_complete_pdu():
  │  ├─ Extract first 4 bytes: command_length
  │  ├─ Check if buffer has full PDU (command_length bytes)
  │  ├─ Extract command_id from bytes [4:8]
  │  │
  │  ├─ Call instantiate_pdu_() to create typed object:
  │  │  └─ switch(command_id):
  │  │     ├─ BIND_RECEIVER (0x00000001) → new Smpp::BindReceiver(pdu_ptr)
  │  │     ├─ BIND_TRANSMITTER (0x00000002) → new Smpp::BindTransmitter(pdu_ptr)
  │  │     ├─ BIND_TRANSCEIVER (0x00000009) → new Smpp::BindTransceiver(pdu_ptr)
  │  │     ├─ UNBIND (0x00000006) → new Smpp::Unbind(pdu_ptr)
  │  │     ├─ ENQUIRE_LINK (0x0000000F) → new Smpp::EnquireLink(pdu_ptr)
  │  │     └─ Unknown → throw or return error response
  │  │
  │  ├─ Remove consumed bytes from buffer_
  │  └─ Add to results vector
  │
  └─ Return vector of Smpp::Header* pointers

encode_response(response)
  ├─ Call response.encode() → uint8_t*
  ├─ Get response.command_length() → size_t
  ├─ Create vector from bytes
  └─ Return for TCP transmission
```

### Key Implementation Details

**Detecting Complete PDU**:
```cpp
bool SmppProtocolAdapter::can_extract_pdu_() const {
    if (buffer_.size() < 4) {
        return false;  // Can't even read command_length
    }
    
    // Extract command_length (big-endian, bytes 0-3)
    uint32_t command_length = 
        (uint32_t(buffer_[0]) << 24) |
        (uint32_t(buffer_[1]) << 16) |
        (uint32_t(buffer_[2]) << 8) |
        uint32_t(buffer_[3]);
    
    // Check if we have full PDU
    return buffer_.size() >= command_length;
}

std::unique_ptr<Smpp::Header> SmppProtocolAdapter::extract_pdu_() {
    // Extract command_length
    uint32_t command_length = 
        (uint32_t(buffer_[0]) << 24) |
        (uint32_t(buffer_[1]) << 16) |
        (uint32_t(buffer_[2]) << 8) |
        uint32_t(buffer_[3]);
    
    // Extract and parse
    const uint8_t* pdu_ptr = buffer_.data();
    auto pdu = parse_pdu_(pdu_ptr);
    
    // Remove from buffer
    buffer_.erase(buffer_.begin(), buffer_.begin() + command_length);
    
    return pdu;
}
```

**Creating Typed Objects**:
```cpp
std::unique_ptr<Smpp::Header> SmppProtocolAdapter::instantiate_pdu_(
    const uint8_t* pdu_bytes,
    Smpp::Uint32 command_id) {
    
    try {
        switch (command_id) {
        case 0x00000001:  // BIND_RECEIVER
            return std::make_unique<Smpp::BindReceiver>(pdu_bytes);
        case 0x00000002:  // BIND_TRANSMITTER
            return std::make_unique<Smpp::BindTransmitter>(pdu_bytes);
        case 0x00000009:  // BIND_TRANSCEIVER
            return std::make_unique<Smpp::BindTransceiver>(pdu_bytes);
        case 0x00000006:  // UNBIND
            return std::make_unique<Smpp::Unbind>(pdu_bytes);
        case 0x0000000F:  // ENQUIRE_LINK
            return std::make_unique<Smpp::EnquireLink>(pdu_bytes);
        default:
            throw std::invalid_argument("Unknown command ID");
        }
    } catch (const std::exception& ex) {
        throw std::runtime_error("Failed to parse PDU: " + std::string(ex.what()));
    }
}
```

**Encoding Response**:
```cpp
std::vector<uint8_t> SmppProtocolAdapter::encode_response(
    const Smpp::Header& response) {
    
    // smppcxx's encode() returns pointer and command_length() gives size
    const uint8_t* encoded = response.encode();
    uint32_t length = response.command_length();
    
    return std::vector<uint8_t>(encoded, encoded + length);
}
```

### Dependencies

- **smppcxx** (external library via CMakeLists.txt FetchContent)

---

## Integration with Existing Classes

### SmppConnection Usage

```cpp
class SmppConnection {
private:
    std::unique_ptr<SmppProtocolAdapter> protocol_adapter_;
    
    void on_body_read(const asio::error_code& ec, size_t bytes_read) {
        // Assemble full PDU from header + body
        std::vector<uint8_t> full_pdu = header_buffer_;
        full_pdu.insert(full_pdu.end(), body_buffer_.begin(), body_buffer_.end());
        
        // Feed to protocol adapter
        auto pdu_objects = protocol_adapter_->process_tcp_data(full_pdu);
        
        // Process each PDU
        for (auto& pdu : pdu_objects) {
            // pdu is Smpp::Header*, can be cast to specific type
            auto response = process_pdu(std::move(pdu), *session_);
            
            // Encode response
            auto response_bytes = protocol_adapter_->encode_response(response);
            write_pdu(response_bytes);
        }
        
        // Continue reading next PDU
        read_pdu_header();
    }
};
```

### SmppMessageProcessor Usage

```cpp
class SmppMessageProcessor {
    Smpp::Header* process_pdu(
        std::unique_ptr<Smpp::Header> request,
        SmppSession& session) {
        
        // Get command ID
        Smpp::Uint32 cmd_id = request->command_id();
        
        // Route to handler based on type
        if (auto* bind_rx = dynamic_cast<Smpp::BindReceiver*>(request.get())) {
            return bind_rx_handler_->handle(*bind_rx, session);
        } else if (auto* bind_tx = dynamic_cast<Smpp::BindTransmitter*>(request.get())) {
            return bind_tx_handler_->handle(*bind_tx, session);
        }
        // ... etc
        
        return nullptr;  // Unknown command
    }
};
```

### Handlers Usage

```cpp
class BindTransmitterHandler {
public:
    Smpp::Header* handle(
        const Smpp::BindTransmitter& request,
        SmppSession& session) {
        
        // Extract fields via smppcxx
        std::string username = request.system_id();
        std::string password = request.password();
        Smpp::Uint32 seq = request.sequence_number();
        
        // Validate...
        bool auth_ok = dbus_auth_->authenticate(username, password, ip);
        
        // Build response
        auto* response = new Smpp::BindTransmitterResp();
        response->sequence_number(seq);
        response->command_status(auth_ok ? 0 : 0x0000000E);  // ESME_RINVPASWD
        
        return response;  // Caller owns pointer
    }
};
```

---

## Testing Checklist

- [ ] SmppProtocolAdapter buffers partial PDUs correctly
- [ ] SmppProtocolAdapter reassembles split messages
- [ ] SmppProtocolAdapter detects complete PDU by command_length
- [ ] SmppProtocolAdapter instantiates correct smppcxx class per command_id
- [ ] SmppProtocolAdapter handles unknown command IDs gracefully
- [ ] encode_response() produces valid SMPP bytes
- [ ] Integration with SmppConnection works end-to-end
- [ ] Handlers can extract fields from smppcxx objects
- [ ] Response objects build and encode correctly

---

**Document Status**: Complete (Revised for smppcxx)  
**Next**: Revise business_logic_layer.md and presentation_layer.md
