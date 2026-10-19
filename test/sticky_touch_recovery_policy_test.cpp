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

    // Valid geometry is exactly the verified 480x800 read; everything else is retried.
    assert(sticky_touch_geometry_valid(true, 480U, 800U));
    assert(!sticky_touch_geometry_valid(true, 2048U, 2048U));
    assert(!sticky_touch_geometry_valid(true, 800U, 480U));
    assert(!sticky_touch_geometry_valid(false, 480U, 800U));
    assert(!sticky_touch_geometry_valid(true, 0U, 0U));
    assert(kStickyTouchInitAttempts >= 2U);   // start-up repeats the reset flow

    // Background retry back-off: 2 s, 4 s, 8 s, 16 s, then every 30 s.
    assert(sticky_touch_geometry_retry_delay_ms(0U) == 2000U);
    assert(sticky_touch_geometry_retry_delay_ms(1U) == 4000U);
    assert(sticky_touch_geometry_retry_delay_ms(2U) == 8000U);
    assert(sticky_touch_geometry_retry_delay_ms(3U) == 16000U);
    assert(sticky_touch_geometry_retry_delay_ms(4U) == 30000U);
    assert(sticky_touch_geometry_retry_delay_ms(100U) == 30000U);
    for (uint32_t n = 0U; n < 20U; ++n) {   // never shrinks, never exceeds the cap
        assert(sticky_touch_geometry_retry_delay_ms(n + 1U) >= sticky_touch_geometry_retry_delay_ms(n));
        assert(sticky_touch_geometry_retry_delay_ms(n) <= kStickyTouchGeometryRetryMaxMs);
    }

    // A retry runs only for a dead controller: invalid geometry, no contact ever read, idle, and due.
    assert(sticky_touch_geometry_retry_due(false, false, false, 5000U, 5000U));
    assert(sticky_touch_geometry_retry_due(false, false, false, 5001U, 5000U));
    assert(!sticky_touch_geometry_retry_due(false, false, false, 4999U, 5000U)); // not yet
    assert(!sticky_touch_geometry_retry_due(true, false, false, 9000U, 5000U));  // geometry fine
    assert(!sticky_touch_geometry_retry_due(false, true, false, 9000U, 5000U));  // touch works
    assert(!sticky_touch_geometry_retry_due(false, false, true, 9000U, 5000U));  // finger down
    assert(sticky_touch_geometry_retry_due(false, false, false, 100U, UINT32_MAX - 100U)); // clock wrap

    assert(!sticky_touch_recovery_required(4U));
    assert(sticky_touch_recovery_required(5U));
    assert(!sticky_touch_contact_stuck(1000U, 10999U));
    assert(sticky_touch_contact_stuck(1000U, 11000U));
    assert(sticky_touch_contact_stuck(UINT32_MAX - 4999U, 5001U));
    return 0;
}
