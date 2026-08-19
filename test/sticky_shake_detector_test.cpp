#include <cassert>

#include "sticky_shake_detector.h"

int main()
{
    StickyShakeDetectorState state = {};

    assert(!sticky_shake_detector_update(
                state, 0.0F, 0.0F, 1.0F, 0U)
                .detected);

    // A normal gradual orientation change stays below the peak threshold.
    // 普通缓慢转向的相邻采样变化不会达到摇晃峰值。
    for (uint32_t step = 1U; step <= 8U; ++step) {
        const float x = static_cast<float>(step) * 0.1F;
        const float z = 1.0F - static_cast<float>(step) * 0.1F;
        const StickyShakeDetectorResult result =
            sticky_shake_detector_update(state, x, 0.0F, z, step * 100U);
        assert(!result.detected);
    }

    StickyShakeDetectorResult result =
        sticky_shake_detector_update(state, -1.0F, 0.0F, 0.1F, 1000U);
    assert(result.peak && !result.detected);
    result = sticky_shake_detector_update(
        state, 1.0F, 0.0F, 0.1F, 1150U);
    assert(result.peak && !result.detected);
    result = sticky_shake_detector_update(
        state, -1.0F, 0.0F, 0.1F, 1300U);
    assert(result.peak && result.detected);

    // The cooldown absorbs continued movement from the same shake session.
    // 冷却时间会吸收同一次摇晃之后仍在持续的动作。
    result = sticky_shake_detector_update(
        state, 1.0F, 0.0F, 0.1F, 1450U);
    assert(!result.detected);
    return 0;
}
