#include "desktop_pet_daily.h"

DesktopPetDailyGreeting desktop_pet_daily_greeting(
    uint32_t previous_day_key,
    uint32_t current_day_key)
{
    if (previous_day_key == 0U || current_day_key <= previous_day_key) {
        return {};
    }

    const uint32_t absence_days = current_day_key - previous_day_key;
    if (absence_days == 1U) {
        return {true, absence_days,
                "GOOD MORNING! I'M GLAD YOU'RE HERE."};
    }
    if (absence_days <= 3U) {
        return {true, absence_days,
                "WELCOME BACK! I SAVED YOUR SPOT."};
    }
    return {true, absence_days,
            "YOU'RE BACK! LET'S TAKE TODAY GENTLY."};
}

uint16_t desktop_pet_daily_milestone(uint16_t care_streak)
{
    switch (care_streak) {
    case 3U:
    case 7U:
    case 30U:
    case 100U:
        return care_streak;
    default:
        return 0U;
    }
}
