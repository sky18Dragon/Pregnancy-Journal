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
};
