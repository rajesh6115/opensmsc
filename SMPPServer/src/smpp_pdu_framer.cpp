#include "smpp_pdu_framer.hpp"

#include <cstring>
#include <algorithm>

uint32_t SmppPduFramer::be32_to_host(const uint8_t* ptr)
{
    return ((uint32_t)ptr[0] << 24) | ((uint32_t)ptr[1] << 16) |
           ((uint32_t)ptr[2] << 8) | ((uint32_t)ptr[3]);
}

void SmppPduFramer::reset()
{
    state_ = State::WAITING_HEADER;
    full_pdu_.clear();
    command_length_ = 0;
    command_id_ = 0;
    command_status_ = 0;
    sequence_number_ = 0;
}

void SmppPduFramer::parse_header()
{
    if (full_pdu_.size() < 16) return;

    command_length_ = be32_to_host(full_pdu_.data() + 0);
    command_id_ = be32_to_host(full_pdu_.data() + 4);
    command_status_ = be32_to_host(full_pdu_.data() + 8);
    sequence_number_ = be32_to_host(full_pdu_.data() + 12);
}

bool SmppPduFramer::feed_bytes(const uint8_t* data, size_t len)
{
    if (len == 0) return false;

    size_t to_read = bytes_needed();
    size_t can_read = std::min(len, to_read);

    full_pdu_.insert(full_pdu_.end(), data, data + can_read);

    switch (state_) {
    case State::WAITING_HEADER:
        if (full_pdu_.size() >= 16) {
            parse_header();
            if (command_length_ >= 16) {
                state_ = State::WAITING_BODY;
            } else {
                return false; // Invalid header
            }
        }
        break;

    case State::WAITING_BODY:
        if (full_pdu_.size() >= command_length_) {
            state_ = State::PDU_COMPLETE;
            return true;
        }
        break;

    case State::PDU_COMPLETE:
        break;
    }

    return false;
}

size_t SmppPduFramer::bytes_needed() const
{
    switch (state_) {
    case State::WAITING_HEADER:
        return 16 - full_pdu_.size();

    case State::WAITING_BODY:
        return command_length_ - full_pdu_.size();

    case State::PDU_COMPLETE:
        return 0;
    }

    return 0;
}
