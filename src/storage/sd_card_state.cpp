#include "sd_card_state.h"

#include <limits>

namespace sticky_storage {

SdCardStateMachine::SdCardStateMachine(uint32_t initial_generation)
    : state_(SdCardState::Absent), generation_(initial_generation),
      mounted_(false)
{
}

SdCardState SdCardStateMachine::state() const { return state_; }

uint32_t SdCardStateMachine::generation() const { return generation_; }

bool SdCardStateMachine::mounted() const { return mounted_; }

void SdCardStateMachine::advance_generation()
{
    generation_ = generation_ == std::numeric_limits<uint32_t>::max()
                      ? 1U
                      : generation_ + 1U;
}

bool SdCardStateMachine::begin_mount(bool card_present)
{
    if (!card_present) {
        if (!mounted_) {
            state_ = SdCardState::Absent;
        }
        return false;
    }
    if (mounted_ || (state_ != SdCardState::Absent &&
                     state_ != SdCardState::Unmounted &&
                     state_ != SdCardState::Fault)) {
        return false;
    }
    advance_generation();
    state_ = SdCardState::Mounting;
    return true;
}

bool SdCardStateMachine::complete_mount(SdCardAccess access)
{
    if (state_ != SdCardState::Mounting || mounted_) {
        return false;
    }
    mounted_ = true;
    state_ = access == SdCardAccess::ReadOnly
                 ? SdCardState::ReadyReadOnly
                 : SdCardState::ReadyReadWrite;
    return true;
}

bool SdCardStateMachine::fail_mount(bool card_present)
{
    if (state_ != SdCardState::Mounting || mounted_) {
        return false;
    }
    state_ = card_present ? SdCardState::Fault : SdCardState::Absent;
    return true;
}

void SdCardStateMachine::card_removed()
{
    mounted_ = false;
    state_ = SdCardState::Absent;
}

bool SdCardStateMachine::mark_full()
{
    if (state_ != SdCardState::ReadyReadWrite || !mounted_) {
        return false;
    }
    state_ = SdCardState::Full;
    return true;
}

bool SdCardStateMachine::mark_space_available()
{
    if (state_ != SdCardState::Full || !mounted_) {
        return false;
    }
    state_ = SdCardState::ReadyReadWrite;
    return true;
}

bool SdCardStateMachine::mark_read_only()
{
    if (!mounted_ || (state_ != SdCardState::ReadyReadWrite &&
                      state_ != SdCardState::Full)) {
        return false;
    }
    state_ = SdCardState::ReadyReadOnly;
    return true;
}

void SdCardStateMachine::mark_fault()
{
    state_ = SdCardState::Fault;
}

bool SdCardStateMachine::clear_fault(bool card_present)
{
    if (state_ != SdCardState::Fault || mounted_) {
        return false;
    }
    state_ = card_present ? SdCardState::Unmounted : SdCardState::Absent;
    return true;
}

bool SdCardStateMachine::begin_eject()
{
    if (!mounted_ || (state_ != SdCardState::ReadyReadWrite &&
                      state_ != SdCardState::ReadyReadOnly &&
                      state_ != SdCardState::Full &&
                      state_ != SdCardState::Fault)) {
        return false;
    }
    state_ = SdCardState::Ejecting;
    return true;
}

bool SdCardStateMachine::complete_eject()
{
    if (state_ != SdCardState::Ejecting || !mounted_) {
        return false;
    }
    mounted_ = false;
    state_ = SdCardState::Unmounted;
    return true;
}

bool SdCardStateMachine::fail_eject()
{
    if (state_ != SdCardState::Ejecting || !mounted_) {
        return false;
    }
    state_ = SdCardState::Fault;
    return true;
}

bool SdCardStateMachine::can_read() const
{
    return mounted_ &&
           (state_ == SdCardState::ReadyReadWrite ||
            state_ == SdCardState::ReadyReadOnly ||
            state_ == SdCardState::Full);
}

bool SdCardStateMachine::can_write() const
{
    return mounted_ && state_ == SdCardState::ReadyReadWrite;
}

bool SdCardStateMachine::accepts(uint32_t request_generation, bool write) const
{
    if (request_generation == 0U || request_generation != generation_) {
        return false;
    }
    return write ? can_write() : can_read();
}

}  // namespace sticky_storage
