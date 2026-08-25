#include "sticky_app_power_policy.h"

const char *sticky_power_sleep_trigger_name(
    StickyPowerSleepTrigger trigger)
{
    switch (trigger) {
    case StickyPowerSleepTrigger::SideButtonChord:
        return "side_button_chord";
    case StickyPowerSleepTrigger::ScheduledEventComplete:
        return "scheduled_event_complete";
    case StickyPowerSleepTrigger::AppIdleTimeout:
        return "app_idle_timeout";
    }
    return "unknown";
}

bool sticky_power_sleep_chime_enabled(
    StickyPowerSleepTrigger trigger)
{
    return trigger == StickyPowerSleepTrigger::SideButtonChord;
}
