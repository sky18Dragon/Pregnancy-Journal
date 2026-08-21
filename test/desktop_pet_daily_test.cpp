#include <cassert>

#include "desktop_pet_daily.h"

int main()
{
    assert(!desktop_pet_daily_greeting(0U, 100U).visible);
    assert(!desktop_pet_daily_greeting(100U, 100U).visible);
    assert(!desktop_pet_daily_greeting(101U, 100U).visible);

    const DesktopPetDailyGreeting next_day =
        desktop_pet_daily_greeting(100U, 101U);
    assert(next_day.visible);
    assert(next_day.absence_days == 1U);

    const DesktopPetDailyGreeting short_return =
        desktop_pet_daily_greeting(100U, 103U);
    assert(short_return.visible);
    assert(short_return.absence_days == 3U);

    const DesktopPetDailyGreeting long_return =
        desktop_pet_daily_greeting(100U, 110U);
    assert(long_return.visible);
    assert(long_return.absence_days == 10U);

    assert(desktop_pet_daily_milestone(1U) == 0U);
    assert(desktop_pet_daily_milestone(3U) == 3U);
    assert(desktop_pet_daily_milestone(7U) == 7U);
    assert(desktop_pet_daily_milestone(14U) == 0U);
    assert(desktop_pet_daily_milestone(30U) == 30U);
    assert(desktop_pet_daily_milestone(31U) == 0U);
    assert(desktop_pet_daily_milestone(100U) == 100U);
    assert(desktop_pet_daily_milestone(101U) == 0U);
    return 0;
}
