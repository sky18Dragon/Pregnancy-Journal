#pragma once

#include <cstdint>

#include "scheduler.h"

// The offline four-app release schedules maintenance and app events only.
// Rebuild from scratch so no stale event can survive.
void sticky_sleep_schedule_configure(StickyScheduler &scheduler, uint32_t now,
                                     uint32_t app_event);
uint64_t sticky_sleep_schedule_wake_us(const StickyScheduler &scheduler,
                                      uint32_t now);
bool sticky_sleep_schedule_is_daily_window(uint8_t hour, uint8_t minute);

// Delay before an idle sleep is attempted again after a rolled-back attempt.
// Doubles from one minute up to thirty minutes so a missing RTC or busy card
// cannot turn the idle timer into a pause/resume/full-refresh loop.
uint32_t sticky_sleep_retry_delay_ms(uint8_t consecutive_failures);
