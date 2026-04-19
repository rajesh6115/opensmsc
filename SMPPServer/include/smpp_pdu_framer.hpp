#pragma once

#include <cstdint>
#include <vector>
#include <memory>

class SmppPduFramer {
public:
    enum class State {
        WAITING_HEADER,      // Reading 16-byte SMPP header
        WAITING_BODY,        // Reading PDU body
        PDU_COMPLETE         // Complete PDU ready
    };

    // Reset framer state
    void reset();

    // Feed bytes to the framer. Returns true if PDU is complete.
    bool feed_bytes(const uint8_t* data, size_t len);

    // Check current state
    State get_state() const { return state_; }

    // Get the complete PDU (only valid when PDU_COMPLETE)
    const std::vector<uint8_t>& get_pdu() const { return full_pdu_; }
    std::vector<uint8_t>& get_pdu() { return full_pdu_; }

    // Get header fields (only valid when state >= WAITING_BODY)
    uint32_t get_command_length() const { return command_length_; }
    uint32_t get_command_id() const { return command_id_; }
    uint32_t get_command_status() const { return command_status_; }
    uint32_t get_sequence_number() const { return sequence_number_; }

    // Bytes remaining to read for next state
    size_t bytes_needed() const;

private:
    State state_ = State::WAITING_HEADER;
    std::vector<uint8_t> full_pdu_;

    uint32_t command_length_ = 0;
    uint32_t command_id_ = 0;
    uint32_t command_status_ = 0;
    uint32_t sequence_number_ = 0;

    // Helper: parse header from buffer
    void parse_header();

    // Helper: convert big-endian bytes to uint32
    static uint32_t be32_to_host(const uint8_t* ptr);
};
