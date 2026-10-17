#pragma once

#include <cstdint>

#include "esp_err.h"
#include "sticky_app_id.h"

class Canvas;

enum class StickyAppRotationPolicy : uint8_t {
    FixedLandscape,
    FixedPortrait,
    FollowDevice,
};

struct StickyAppDescriptor {
    StickyAppId id;
    const char *name;
    const char *label;
    StickyAppRotationPolicy rotation;
    esp_err_t (*start)(Canvas &canvas);
    esp_err_t (*pause)();
    esp_err_t (*resume)();
    esp_err_t (*prepare_sleep)(uint32_t &current_epoch,
                               uint32_t &next_event_epoch);
    uint32_t (*sleep_timeout_ms)();
    bool (*sleep_allowed)();
    // Optional: receives the top-key press (with the time it was made) while
    // the app shows a KeyPolicy::Capture page. Called from the coordinator task,
    // so it must only queue the press.
    // 可选：应用显示 Capture 页时接收顶键按下（带按下时刻）；在协调器任务中调用，只能入队。
    void (*capture_press)(int64_t at_us) = nullptr;
    // Optional: draws the whole sleep status (battery and moon) itself, only called when the app's
    // own page is on screen. Return true when it did; the system then adds nothing, so the app's
    // layout is the only one on the frame. Return false (or leave null) for the system overlay.
    // 可选：应用自行绘制完整的休眠状态（电量与月亮），仅当应用自己的页面在屏幕上时调用；返回 true
    // 则系统不再叠加，休眠帧上只有应用自己的版式；返回 false 或为空则使用系统叠加。
    bool (*draw_sleep_status)(Canvas &canvas) = nullptr;
};
