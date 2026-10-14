#include "sticky_sleep_schedule.h"

#include <limits>

void sticky_sleep_schedule_configure(StickyScheduler &scheduler, uint32_t now,
                                     uint32_t app_event)
{
    scheduler = StickyScheduler{};
    constexpr uint64_t kDaySeconds = 86400U;
    uint64_t daily = static_cast<uint64_t>(now) / kDaySeconds * kDaySeconds +
                     3U * 3600U;
    if (daily <= now) daily += kDaySeconds;
    if (daily <= std::numeric_limits<uint32_t>::max())
        scheduler.schedule(1U, static_cast<uint32_t>(daily),
                           StickyScheduledEventType::DailyRefresh);
    if (app_event > now)
        scheduler.schedule(4U, app_event, StickyScheduledEventType::System);
}

uint64_t sticky_sleep_schedule_wake_us(const StickyScheduler &scheduler,
                                      uint32_t now)
{
    StickyScheduledEvent event = {};
    if (!scheduler.next(now, event)) return 0U;
    constexpr uint32_t kLeadSeconds = 15U;
    uint32_t wake = event.epoch_seconds > kLeadSeconds
                        ? event.epoch_seconds - kLeadSeconds
                        : event.epoch_seconds;
    if (wake <= now) wake = now + 1U;
    return static_cast<uint64_t>(wake - now) * 1000000ULL;
}

bool sticky_sleep_schedule_is_daily_window(uint8_t hour, uint8_t minute)
{
    return (hour == 2U && minute == 59U) ||
           (hour == 3U && minute <= 2U);
}

uint32_t sticky_sleep_retry_delay_ms(uint8_t consecutive_failures)
{
    constexpr uint32_t kFirstRetryMs = 60U * 1000U;
    constexpr uint32_t kMaximumRetryMs = 30U * 60U * 1000U;
    uint32_t delay = kFirstRetryMs;
    for (uint8_t failure = 1U;
         failure < consecutive_failures && delay < kMaximumRetryMs;
         ++failure) {
        delay *= 2U;
    }
    return delay < kMaximumRetryMs ? delay : kMaximumRetryMs;
}
