#include "sticky_shake_detector.h"

#include <cmath>

namespace {

constexpr float kPeakDeltaG = 0.58F;
constexpr uint32_t kMinimumPeakSpacingMs = 100U;
constexpr uint32_t kCandidateWindowMs = 700U;
constexpr uint32_t kMaximumActiveGapMs = 650U;
constexpr uint8_t kRequiredStartPeakCount = 2U;

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

    // Ends the active session as soon as movement has been quiet for too long.
    // 当连续动作的间隔超过上限时，立即结束本次摇晃会话。
    if (state.session_active &&
        now_ms - state.last_peak_ms > kMaximumActiveGapMs) {
        result.active_duration_ms = now_ms - state.session_started_ms;
        state.session_active = false;
        state.candidate_peak_count = 0U;
        result.session_stopped = true;
    }

    if (!state.session_active && state.candidate_peak_count > 0U &&
        now_ms - state.candidate_started_ms > kCandidateWindowMs) {
        state.candidate_peak_count = 0U;
    }

    const bool peak_spacing_ready =
        (state.session_active || state.candidate_peak_count > 0U) &&
        now_ms - state.last_peak_ms >= kMinimumPeakSpacingMs;
    const bool first_candidate_peak =
        !state.session_active && state.candidate_peak_count == 0U;
    if (result.delta_g < kPeakDeltaG ||
        (!first_candidate_peak && !peak_spacing_ready)) {
        result.candidate_peak_count = state.candidate_peak_count;
        result.session_active = state.session_active;
        if (state.session_active) {
            result.active_duration_ms =
                now_ms - state.session_started_ms;
        }
        return result;
    }

    result.peak = true;
    state.last_peak_ms = now_ms;
    if (state.session_active) {
        result.session_active = true;
        result.active_duration_ms = now_ms - state.session_started_ms;
        return result;
    }

    if (state.candidate_peak_count == 0U) {
        state.candidate_started_ms = now_ms;
    }
    ++state.candidate_peak_count;
    result.candidate_peak_count = state.candidate_peak_count;

    if (state.candidate_peak_count >= kRequiredStartPeakCount) {
        state.session_active = true;
        state.session_started_ms = now_ms;
        state.candidate_peak_count = 0U;
        result.candidate_peak_count = 0U;
        result.session_started = true;
        result.session_active = true;
    }
    return result;
}
