#pragma once

#include <cstdint>

#include "esp_err.h"

enum class StickyButtonEvent : uint8_t {
    None,
    PressDown,
    SingleClick,
    DoubleClick,
    SleepChord,
};

// Initializes the active-low top button with the reference debounce settings.
// 使用硬件示例的消抖参数初始化低电平有效的顶部按键。
esp_err_t sticky_button_init();

// Consumes one AI-button event or a two-second side-button sleep chord.
// 读取一个AI按键事件，或持续两秒的双侧键休眠组合事件。
bool sticky_button_take_event(StickyButtonEvent &event);
