#pragma once

#include <cstdint>

struct StickyShakeDetectorState {
    float previous_x_g = 0.0F;
    float previous_y_g = 0.0F;
    float previous_z_g = 0.0F;
    uint32_t candidate_started_ms = 0U;
    uint32_t session_started_ms = 0U;
    uint32_t last_peak_ms = 0U;
    uint8_t candidate_peak_count = 0U;
    bool has_previous_sample = false;
    bool session_active = false;
};

struct StickyShakeDetectorResult {
    float delta_g = 0.0F;
    uint32_t active_duration_ms = 0U;
    uint8_t candidate_peak_count = 0U;
    bool peak = false;
    bool session_started = false;
    bool session_active = false;
    bool session_stopped = false;
};

// Consumes one accelerometer sample and tracks a shake session that remains
// active only while separated high-energy changes continue to arrive.
// 输入一次加速度采样；只有持续出现间隔合理的明显变化，摇晃会话才保持有效。
StickyShakeDetectorResult sticky_shake_detector_update(
    StickyShakeDetectorState &state,
    float x_g,
    float y_g,
    float z_g,
    uint32_t now_ms);
