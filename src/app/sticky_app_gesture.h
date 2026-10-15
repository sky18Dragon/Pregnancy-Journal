#pragma once

#include <cstdint>

enum class StickyAppGestureAction : uint8_t {
    None,
    OpenLauncher,
    CloseLauncher,
    NextApp,      // swipe left along the bottom edge
    PreviousApp,  // swipe right along the bottom edge
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

// Classifies launcher gestures in the current logical orientation: vertical
// swipes open/close the launcher; a long horizontal swipe that starts in the
// bottom edge band (launcher closed) switches to the next/previous app.
// 按当前画面方向识别手势：纵向滑动开关应用栏；应用栏关闭时，从底边起的长横滑切换相邻应用。
StickyAppGestureAction sticky_app_gesture_classify(
    const StickyAppGestureSample &sample);
