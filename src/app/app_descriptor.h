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
    // Optional: called just before the sleep frame gets the system battery overlay and moon, only
    // when the app's own page is on screen. The app removes any indicator of its own there so the
    // two do not overlap.
    // 可选：休眠帧叠加系统电量与月亮之前调用（仅当应用自己的页面在屏幕上），应用在此移除自有指示以免重叠。
    void (*before_sleep_frame)(Canvas &canvas) = nullptr;
};
