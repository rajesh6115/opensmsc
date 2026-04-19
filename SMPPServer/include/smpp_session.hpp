#pragma once

#include <cstdint>
#include <string>

class SmppSession
{
public:
    enum class State
    {
        UNBOUND,
        BOUND_TRANSMITTER,
        BOUND_RECEIVER,
        BOUND_TRANSCEIVER
    };

    SmppSession();

    State state() const;
    void set_state(State new_state);

    std::string system_id() const;
    void set_system_id(const std::string& id);

    uint32_t next_sequence_number();

    bool is_bound() const;

private:
    State state_;
    std::string system_id_;
    uint32_t sequence_counter_;
};
