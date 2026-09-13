#include "scheduler.h"

bool StickyScheduler::schedule(uint32_t id, uint32_t epoch_seconds,
                               StickyScheduledEventType type)
{
    if (id == 0U || epoch_seconds == 0U) {
        return false;
    }
    StickyScheduledEvent *available = nullptr;
    for (StickyScheduledEvent &event : events_) {
        if (event.enabled && event.id == id) {
            event.epoch_seconds = epoch_seconds;
            event.type = type;
            return true;
        }
        if (!event.enabled && available == nullptr) {
            available = &event;
        }
    }
    if (available == nullptr) {
        return false;
    }
    *available = {id, epoch_seconds, true, type};
    return true;
}

bool StickyScheduler::cancel(uint32_t id)
{
    for (StickyScheduledEvent &event : events_) {
        if (event.enabled && event.id == id) {
            event = {};
            return true;
        }
    }
    return false;
}

bool StickyScheduler::next(uint32_t now, StickyScheduledEvent &result) const
{
    const StickyScheduledEvent *best = nullptr;
    for (const StickyScheduledEvent &event : events_) {
        if (!event.enabled || event.epoch_seconds <= now) {
            continue;
        }
        if (best == nullptr || event.epoch_seconds < best->epoch_seconds) {
            best = &event;
        }
    }
    if (best == nullptr) {
        return false;
    }
    result = *best;
    return true;
}

size_t StickyScheduler::count() const
{
    size_t result = 0U;
    for (const StickyScheduledEvent &event : events_) {
        result += event.enabled ? 1U : 0U;
    }
    return result;
}
