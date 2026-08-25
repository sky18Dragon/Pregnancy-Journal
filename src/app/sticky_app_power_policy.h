#pragma once

#include <cstdint>

enum class StickyPowerSleepTrigger : uint8_t {
    SideButtonChord,
    ScheduledEventComplete,
    AppIdleTimeout,
};

// Returns the stable log source for one deep-sleep trigger.
// 返回一次深睡触发方式对应的稳定日志名称。
const char *sticky_power_sleep_trigger_name(
    StickyPowerSleepTrigger trigger);

// Reports whether this trigger should play the user confirmation chime.
// 返回当前触发方式是否需要播放用户确认提示音。
bool sticky_power_sleep_chime_enabled(
    StickyPowerSleepTrigger trigger);
