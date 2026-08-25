#include <cassert>
#include <string>

#include "sticky_app_power_policy.h"

int main()
{
    assert(sticky_power_sleep_chime_enabled(
        StickyPowerSleepTrigger::SideButtonChord));
    assert(!sticky_power_sleep_chime_enabled(
        StickyPowerSleepTrigger::ScheduledEventComplete));
    assert(!sticky_power_sleep_chime_enabled(
        StickyPowerSleepTrigger::AppIdleTimeout));
    assert(std::string(sticky_power_sleep_trigger_name(
               StickyPowerSleepTrigger::SideButtonChord)) ==
           "side_button_chord");
    return 0;
}
