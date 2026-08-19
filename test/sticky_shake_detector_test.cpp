#include <cassert>

#include "sticky_shake_detector.h"

int main()
{
    StickyShakeDetectorState state = {};

    assert(!sticky_shake_detector_update(
                state, 0.0F, 0.0F, 1.0F, 0U)
                .session_started);

    // A normal gradual orientation change stays below the peak threshold.
    // 普通缓慢转向的相邻采样变化不会达到摇晃峰值。
    for (uint32_t step = 1U; step <= 8U; ++step) {
        const float x = static_cast<float>(step) * 0.1F;
        const float z = 1.0F - static_cast<float>(step) * 0.1F;
        const StickyShakeDetectorResult result =
            sticky_shake_detector_update(state, x, 0.0F, z, step * 100U);
        assert(!result.session_started);
    }

    // One forceful movement may start the animation, but stopping immediately
    // ends the session before the answer sequence can finish.
    // 一次用力甩动可以启动动画，但随即停下会在答案动画结束前终止会话。
    StickyShakeDetectorResult result =
        sticky_shake_detector_update(state, -1.0F, 0.0F, 0.1F, 1000U);
    assert(result.peak && !result.session_started);
    result = sticky_shake_detector_update(
        state, 1.0F, 0.0F, 0.1F, 1150U);
    assert(result.peak && result.session_started && result.session_active);
    result = sticky_shake_detector_update(
        state, 1.0F, 0.0F, 0.1F, 1900U);
    assert(result.session_stopped && !result.session_active);

    // Repeated movement distributed across the full animation keeps the
    // session alive until the application is ready to reveal the answer.
    // 连续动作覆盖完整动画期间，会话会一直保持有效，直到应用准备揭晓答案。
    result = sticky_shake_detector_update(
        state, -1.0F, 0.0F, 0.1F, 2100U);
    assert(result.peak && !result.session_started);
    result = sticky_shake_detector_update(
        state, 1.0F, 0.0F, 0.1F, 2250U);
    assert(result.session_started && result.session_active);
    for (uint32_t now_ms = 2450U; now_ms <= 5450U; now_ms += 200U) {
        const bool left = ((now_ms - 2450U) / 200U) % 2U == 0U;
        result = sticky_shake_detector_update(
            state, left ? -1.0F : 1.0F, 0.0F, 0.1F, now_ms);
        assert(result.peak);
        assert(result.session_active);
        assert(!result.session_stopped);
    }
    assert(result.active_duration_ms >= 3000U);

    result = sticky_shake_detector_update(
        state, -1.0F, 0.0F, 0.1F, 6200U);
    assert(result.session_stopped && !result.session_active);
    return 0;
}
