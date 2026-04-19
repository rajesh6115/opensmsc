#include "smpp_session.hpp"

SmppSession::SmppSession()
    : state_(State::UNBOUND), sequence_counter_(0)
{
}

SmppSession::State SmppSession::state() const
{
    return state_;
}

void SmppSession::set_state(State new_state)
{
    state_ = new_state;
}

std::string SmppSession::system_id() const
{
    return system_id_;
}

void SmppSession::set_system_id(const std::string& id)
{
    system_id_ = id;
}

uint32_t SmppSession::next_sequence_number()
{
    return ++sequence_counter_;
}

bool SmppSession::is_bound() const
{
    return state_ != State::UNBOUND;
}
