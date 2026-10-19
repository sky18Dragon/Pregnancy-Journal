#pragma once

#include <cstdint>

#include "esp_err.h"
#include "side_key_tracker.h"

enum class StickyButtonEvent : uint8_t {
    None,
    PressDown,
    LongPressStart,
    LongPressEnd,
    SingleClick,
    DoubleClick,
    SleepChord,
    SideSingle,  // one side key pressed on its own (see StickyButtonRecord::side)
};

// One button event with the time it happened (esp_timer, microseconds), so
// consumers can debounce and order presses by when they were made rather than
// by when they are processed.
// 带发生时刻的按键事件，使用方按按下时刻而非处理时刻防抖、排序。
struct StickyButtonRecord {
    StickyButtonEvent event = StickyButtonEvent::None;
    SideKey side = SideKey::Left;  // meaningful for SideSingle
    int64_t at_us = 0;
};

// Initializes the active-low top button with the reference debounce settings.
// 使用硬件示例的消抖参数初始化低电平有效的顶部按键。
esp_err_t sticky_button_init();

// Consumes one event: a top-button event, a side-key single press, or the
// two-second side-button sleep chord. Single consumer only.
// 读取一个事件：顶键事件、侧键单按，或持续两秒的双侧键休眠组合事件；仅限单个消费者。
bool sticky_button_take_record(StickyButtonRecord &record);

// Compatibility form for the hardware test programs: top-button events and the
// sleep chord only; side-key single presses are skipped.
// 硬件测试程序使用的兼容接口：只返回顶键事件和休眠组合键，跳过侧键单按。
bool sticky_button_take_event(StickyButtonEvent &event);
