#include <cassert>
#include <cstdint>
#include <limits>

#include "sticky_sleep_schedule.h"

int main()
{
    StickyScheduler scheduler;
    // Even an already-populated old-version queue loses its business alerts.
    assert(scheduler.schedule(2U, 1001U, StickyScheduledEventType::Reminder));
    assert(scheduler.schedule(3U, 1002U, StickyScheduledEventType::Checkup));
    sticky_sleep_schedule_configure(scheduler, 1000U, 0U);
    StickyScheduledEvent event;
    assert(scheduler.count() == 1U);
    assert(scheduler.next(1000U, event));
    assert(event.type == StickyScheduledEventType::DailyRefresh);
    assert(event.epoch_seconds == 10800U);
    assert(sticky_sleep_schedule_wake_us(scheduler, 1000U) == 9785000000ULL);

    sticky_sleep_schedule_configure(scheduler, 10800U, 0U);
    assert(scheduler.next(10800U, event));
    assert(event.epoch_seconds == 97200U);
    sticky_sleep_schedule_configure(scheduler, 10801U, 10805U);
    assert(scheduler.count() == 2U);
    assert(scheduler.next(10801U, event));
    assert(event.type == StickyScheduledEventType::System);
    assert(sticky_sleep_schedule_wake_us(scheduler, 10801U) == 1000000ULL);
    sticky_sleep_schedule_configure(scheduler, 10801U, 10801U);
    assert(scheduler.count() == 1U);

    const uint32_t last = std::numeric_limits<uint32_t>::max();
    sticky_sleep_schedule_configure(scheduler, last, 0U);
    assert(scheduler.count() == 0U);
    assert(sticky_sleep_schedule_wake_us(scheduler, last) == 0U);
    sticky_sleep_schedule_configure(scheduler, last - 1U, last);
    assert(sticky_sleep_schedule_wake_us(scheduler, last - 1U) == 1000000ULL);

    assert(sticky_sleep_schedule_is_daily_window(2U, 59U));
    assert(sticky_sleep_schedule_is_daily_window(3U, 0U));
    assert(sticky_sleep_schedule_is_daily_window(3U, 2U));
    assert(!sticky_sleep_schedule_is_daily_window(2U, 58U));
    assert(!sticky_sleep_schedule_is_daily_window(3U, 3U));
    assert(!sticky_sleep_schedule_is_daily_window(2U, 255U));
    // Rolled-back idle sleeps back off instead of retrying every poll.
    assert(sticky_sleep_retry_delay_ms(0U) == 60000U);
    assert(sticky_sleep_retry_delay_ms(1U) == 60000U);
    assert(sticky_sleep_retry_delay_ms(2U) == 120000U);
    assert(sticky_sleep_retry_delay_ms(5U) == 960000U);
    assert(sticky_sleep_retry_delay_ms(6U) == 1800000U);
    assert(sticky_sleep_retry_delay_ms(255U) == 1800000U);

    return 0;
}
