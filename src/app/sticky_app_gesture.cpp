#include "sticky_app_gesture.h"

#include <algorithm>

namespace {

constexpr uint32_t kMinimumDurationMs = 60U;
constexpr uint32_t kMaximumDurationMs = 1600U;

int absolute_value(int value)
{
    return value >= 0 ? value : -value;
}

bool valid_vertical_swipe(const StickyAppGestureSample &sample,
                          int vertical_distance)
{
    if (sample.width <= 0 || sample.height <= 0 ||
        sample.duration_ms < kMinimumDurationMs ||
        sample.duration_ms > kMaximumDurationMs) {
        return false;
    }
    const int horizontal_distance =
        absolute_value(sample.end_x - sample.start_x);
    return vertical_distance >= std::max(96, sample.height / 5) &&
           horizontal_distance * 3 <= vertical_distance * 2;
}

}  // namespace

StickyAppGestureAction sticky_app_gesture_classify(
    const StickyAppGestureSample &sample)
{
    const int vertical_delta = sample.end_y - sample.start_y;
    if (!sample.launcher_open) {
        const int bottom_edge_height = std::max(48, sample.height / 8);
        if (sample.start_y < sample.height - bottom_edge_height ||
            vertical_delta >= 0 ||
            !valid_vertical_swipe(sample, -vertical_delta)) {
            return StickyAppGestureAction::None;
        }
        return StickyAppGestureAction::OpenLauncher;
    }

    const int close_start_bottom = sample.height * 7 / 10;
    if (sample.start_y > close_start_bottom ||
        vertical_delta <= 0 ||
        !valid_vertical_swipe(sample, vertical_delta)) {
        return StickyAppGestureAction::None;
    }
    return StickyAppGestureAction::CloseLauncher;
}
