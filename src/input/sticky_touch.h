#pragma once

#include <cstdint>

#include "esp_err.h"

struct StickyTouchPress {
    uint16_t x = 0;
    uint16_t y = 0;
};

// Powers the GT911 and starts the coordinate sampling task.
// 为GT911上电并启动坐标采样任务。
esp_err_t sticky_touch_init();

// Consumes one new press event. Holding a finger does not create more events.
// 取出一次新按下事件；手指持续按住不会重复产生事件。
bool sticky_touch_take_press(StickyTouchPress &press);

// Discards a press recorded before a new interactive page becomes visible.
// 丢弃新交互页面显示前已记录的按下事件。
void sticky_touch_clear_press();
