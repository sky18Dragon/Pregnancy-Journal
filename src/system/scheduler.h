#pragma once

#include <cstddef>
#include <cstdint>

enum class StickyScheduledEventType : uint8_t {
    Reminder,
    Checkup,
    DailyRefresh,
    System,
};

struct StickyScheduledEvent {
    uint32_t id = 0U;
    uint32_t epoch_seconds = 0U;
    bool enabled = false;
    StickyScheduledEventType type = StickyScheduledEventType::System;
};

class StickyScheduler {
public:
    static constexpr size_t kCapacity = 8U;

    bool schedule(uint32_t id, uint32_t epoch_seconds,
                  StickyScheduledEventType type =
                      StickyScheduledEventType::System);
    bool cancel(uint32_t id);
    bool next(uint32_t now, StickyScheduledEvent &event) const;
    size_t count() const;

private:
    StickyScheduledEvent events_[kCapacity] = {};
};
