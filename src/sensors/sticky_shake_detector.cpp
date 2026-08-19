#include "sticky_shake_detector.h"

#include <cmath>

namespace {

constexpr float kPeakDeltaG = 0.58F;
constexpr uint32_t kMinimumPeakSpacingMs = 100U;
constexpr uint32_t kShakeWindowMs = 1200U;
constexpr uint32_t kCooldownMs = 1500U;
constexpr uint8_t kRequiredPeakCount = 3U;

}  // namespace

StickyShakeDetectorResult sticky_shake_detector_update(
    StickyShakeDetectorState &state,
    float x_g,
    float y_g,
    float z_g,
    uint32_t now_ms)
{
    StickyShakeDetectorResult result = {};
    if (!state.has_previous_sample) {
        state.previous_x_g = x_g;
        state.previous_y_g = y_g;
        state.previous_z_g = z_g;
        state.has_previous_sample = true;
        return result;
    }

    const float delta_x = x_g - state.previous_x_g;
    const float delta_y = y_g - state.previous_y_g;
    const float delta_z = z_g - state.previous_z_g;
    result.delta_g = std::sqrt(delta_x * delta_x +
                               delta_y * delta_y +
                               delta_z * delta_z);
    state.previous_x_g = x_g;
    state.previous_y_g = y_g;
    state.previous_z_g = z_g;

    const bool cooldown_active =
        state.cooldown_until_ms != 0U &&
        static_cast<int32_t>(now_ms - state.cooldown_until_ms) < 0;
    if (cooldown_active) {
        state.peak_count = 0U;
        return result;
    }

    if (state.peak_count > 0U &&
        now_ms - state.window_started_ms > kShakeWindowMs) {
        state.peak_count = 0U;
    }

    const bool peak_spacing_ready =
        state.peak_count == 0U ||
        now_ms - state.last_peak_ms >= kMinimumPeakSpacingMs;
    if (result.delta_g < kPeakDeltaG || !peak_spacing_ready) {
        result.peak_count = state.peak_count;
        return result;
    }

    result.peak = true;
    if (state.peak_count == 0U) {
        state.window_started_ms = now_ms;
    }
    state.last_peak_ms = now_ms;
    ++state.peak_count;
    result.peak_count = state.peak_count;

    if (state.peak_count >= kRequiredPeakCount) {
        result.detected = true;
        state.peak_count = 0U;
        state.cooldown_until_ms = now_ms + kCooldownMs;
    }
    return result;
}
