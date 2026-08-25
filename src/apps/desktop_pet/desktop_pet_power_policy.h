#pragma once

#include <cstdint>

#include "desktop_pet_state.h"

// Reports whether this life stage may own an autonomous RTC wake event.
// 返回当前成长阶段是否允许安排自主RTC唤醒事件。
bool desktop_pet_power_autonomous_wake_allowed(
    const DesktopPetState &state);

// Returns the next departure or return event that should wake the device.
// 返回下一次需要唤醒设备的出发或回家事件时间。
uint32_t desktop_pet_power_next_event_epoch(
    const DesktopPetState &state,
    uint32_t current_epoch_seconds);
