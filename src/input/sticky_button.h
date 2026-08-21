#pragma once

#include <cstdint>

#include "esp_err.h"

enum class StickyButtonEvent : uint8_t {
    None,
    PressDown,
    SingleClick,
    DoubleClick,
};

// Initializes the active-low top button with the reference debounce settings.
// 使用硬件示例的消抖参数初始化低电平有效的顶部按键。
esp_err_t sticky_button_init();

// Consumes one queued press-down, single-click, or double-click event.
// 读取并清除一个排队的按下、单击或双击事件。
bool sticky_button_take_event(StickyButtonEvent &event);
