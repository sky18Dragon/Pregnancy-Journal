#pragma once

#include <cstdint>

namespace sticky_storage {

enum class SdCardState : uint8_t {
    Absent = 0,
    Mounting,
    ReadyReadWrite,
    ReadyReadOnly,
    Full,
    Fault,
    Ejecting,
    Unmounted,
};

enum class SdCardAccess : uint8_t {
    ReadWrite = 0,
    ReadOnly,
};

// Pure lifecycle rule for the future shared SD service. It owns no driver,
// file handle or task. Requests are valid only while their captured generation
// matches and the current state grants the requested access.
class SdCardStateMachine {
  public:
    explicit SdCardStateMachine(uint32_t initial_generation = 0U);

    SdCardState state() const;
    uint32_t generation() const;
    bool mounted() const;

    bool begin_mount(bool card_present);
    bool complete_mount(SdCardAccess access);
    bool fail_mount(bool card_present);
    void card_removed();

    bool mark_full();
    bool mark_space_available();
    bool mark_read_only();
    void mark_fault();
    bool clear_fault(bool card_present);

    void invalidate_requests() { advance_generation(); }

    bool begin_eject();
    bool complete_eject();
    bool fail_eject();

    bool can_read() const;
    bool can_write() const;
    bool accepts(uint32_t request_generation, bool write) const;

  private:
    void advance_generation();

    SdCardState state_;
    uint32_t generation_;
    bool mounted_;
};

}  // namespace sticky_storage
