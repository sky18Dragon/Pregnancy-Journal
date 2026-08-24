#pragma once

#include <cstdint>

enum class StickyAppGestureAction : uint8_t {
    None,
    OpenLauncher,
    CloseLauncher,
};

struct StickyAppGestureSample {
    int width = 0;
    int height = 0;
    int start_x = 0;
    int start_y = 0;
    int end_x = 0;
    int end_y = 0;
    uint32_t duration_ms = 0U;
    bool launcher_open = false;
};

// Classifies vertical launcher gestures in the current logical orientation.
// 按当前画面方向识别应用栏的纵向手势。
StickyAppGestureAction sticky_app_gesture_classify(
    const StickyAppGestureSample &sample);
