#pragma once

#include <cstdint>

struct DesktopPetDailyGreeting {
    bool visible = false;
    uint32_t absence_days = 0U;
    const char *message = nullptr;
};

// Builds the once-per-calendar-day greeting from two trusted day keys.
// 根据前后两个可信日期编号生成每日一次的回归问候。
DesktopPetDailyGreeting desktop_pet_daily_greeting(
    uint32_t previous_day_key,
    uint32_t current_day_key);

// Returns the milestone day count, or zero for an ordinary streak value.
// 返回需要庆祝的连续天数；普通连续天数返回零。
uint16_t desktop_pet_daily_milestone(uint16_t care_streak);
