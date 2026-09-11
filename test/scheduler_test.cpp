#include <cassert>

#include "scheduler.h"

int main()
{
    StickyScheduler scheduler;
    StickyScheduledEvent event = {};
    assert(!scheduler.next(100U, event));
    assert(scheduler.schedule(1U, 300U));
    assert(scheduler.schedule(2U, 200U));
    assert(scheduler.next(100U, event));
    assert(event.id == 2U && event.epoch_seconds == 200U);
    assert(scheduler.cancel(2U));
    assert(scheduler.next(100U, event) && event.id == 1U);
    assert(!scheduler.next(300U, event));
    return 0;
}
