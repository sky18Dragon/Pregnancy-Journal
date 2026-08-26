#include <cassert>
#include <cstdint>

#include "sticky_touch_recovery_policy.h"

int main()
{
    const StickyTouchSensorResolution verified =
        sticky_touch_select_sensor_resolution(true, 480U, 800U);
    assert(verified.width == 480U);
    assert(verified.height == 800U);
    assert(!verified.used_fallback);

    const StickyTouchSensorResolution unreadable =
        sticky_touch_select_sensor_resolution(false, 0U, 0U);
    assert(unreadable.width == 480U);
    assert(unreadable.height == 800U);
    assert(unreadable.used_fallback);

    const StickyTouchSensorResolution generic_default =
        sticky_touch_select_sensor_resolution(true, 2048U, 2048U);
    assert(generic_default.width == 480U);
    assert(generic_default.height == 800U);
    assert(generic_default.used_fallback);

    assert(!sticky_touch_recovery_required(4U));
    assert(sticky_touch_recovery_required(5U));
    assert(!sticky_touch_contact_stuck(1000U, 10999U));
    assert(sticky_touch_contact_stuck(1000U, 11000U));
    assert(sticky_touch_contact_stuck(UINT32_MAX - 4999U, 5001U));
    return 0;
}
