#include <cassert>

#include "scheduler.h"

int main()
{
    StickyScheduler scheduler;
    StickyScheduledEvent event = {};
    assert(!scheduler.next(100U, event));
    assert(scheduler.schedule(1U, 300U,
                              StickyScheduledEventType::DailyRefresh));
    assert(scheduler.schedule(2U, 200U,
                              StickyScheduledEventType::Reminder));
    assert(scheduler.schedule(3U, 200U,
                              StickyScheduledEventType::Checkup));
    assert(scheduler.next(100U, event));
    assert(event.id == 2U && event.epoch_seconds == 200U);
    assert(event.type == StickyScheduledEventType::Reminder);
    assert(scheduler.schedule(2U, 240U,
                              StickyScheduledEventType::Reminder));
    assert(scheduler.count() == 3U);
    assert(scheduler.next(100U, event));
    assert(event.id == 3U && event.type == StickyScheduledEventType::Checkup);
    assert(scheduler.cancel(3U));
    assert(scheduler.cancel(2U));
    assert(scheduler.next(100U, event) && event.id == 1U);
    assert(!scheduler.next(300U, event));
    assert(!scheduler.cancel(99U));
    assert(!scheduler.schedule(0U, 500U));
    return 0;
}
