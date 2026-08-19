#pragma once

#include <cstdint>

struct StickyShakeDetectorState {
    float previous_x_g = 0.0F;
    float previous_y_g = 0.0F;
    float previous_z_g = 0.0F;
    uint32_t window_started_ms = 0U;
    uint32_t last_peak_ms = 0U;
    uint32_t cooldown_until_ms = 0U;
    uint8_t peak_count = 0U;
    bool has_previous_sample = false;
};

struct StickyShakeDetectorResult {
    float delta_g = 0.0F;
    uint8_t peak_count = 0U;
    bool peak = false;
    bool detected = false;
};

// Consumes one accelerometer sample and recognizes a short sequence of
// separated high-energy changes as one deliberate shake.
// 输入一次加速度采样，将短时间内连续出现的明显变化识别为一次主动摇晃。
StickyShakeDetectorResult sticky_shake_detector_update(
    StickyShakeDetectorState &state,
    float x_g,
    float y_g,
    float z_g,
    uint32_t now_ms);
