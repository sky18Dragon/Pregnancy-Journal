#include "sticky_touch_recovery_policy.h"

bool sticky_touch_geometry_valid(
    bool read_succeeded,
    uint16_t reported_width,
    uint16_t reported_height)
{
    return read_succeeded &&
           reported_width == kStickyTouchSensorWidth &&
           reported_height == kStickyTouchSensorHeight;
}

StickyTouchSensorResolution sticky_touch_select_sensor_resolution(
    bool read_succeeded,
    uint16_t reported_width,
    uint16_t reported_height)
{
    const bool reported_geometry_valid = sticky_touch_geometry_valid(
        read_succeeded, reported_width, reported_height);
    return {
        kStickyTouchSensorWidth,
        kStickyTouchSensorHeight,
        !reported_geometry_valid,
    };
}

uint32_t sticky_touch_geometry_retry_delay_ms(uint32_t retries_so_far)
{
    if (retries_so_far >= 4U) {
        return kStickyTouchGeometryRetryMaxMs;
    }
    const uint32_t delay = kStickyTouchGeometryRetryFirstMs << retries_so_far;
    return delay < kStickyTouchGeometryRetryMaxMs
               ? delay
               : kStickyTouchGeometryRetryMaxMs;
}

bool sticky_touch_geometry_retry_due(
    bool geometry_valid,
    bool touch_seen,
    bool touching,
    uint32_t now_ms,
    uint32_t due_ms)
{
    return !geometry_valid && !touch_seen && !touching &&
           static_cast<int32_t>(now_ms - due_ms) >= 0;
}

bool sticky_touch_recovery_required(uint32_t consecutive_failures)
{
    return consecutive_failures >= kStickyTouchReadFailureThreshold;
}

bool sticky_touch_contact_stuck(uint32_t started_at_ms, uint32_t now_ms)
{
    return now_ms - started_at_ms >= kStickyTouchMaximumContactMs;
}
