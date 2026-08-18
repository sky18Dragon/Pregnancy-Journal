#pragma once

#include <cstdint>

#include "esp_err.h"

struct StickyTouchPress {
    uint16_t x = 0;
    uint16_t y = 0;
    uint32_t captured_at_ms = 0;
};

// Powers the GT911 and starts the coordinate sampling task.
// 为GT911上电并启动坐标采样任务。
esp_err_t sticky_touch_init();

// Consumes the oldest queued press event. Holding a finger does not create
// more events.
// 按顺序取出最早的按下事件；手指持续按住不会重复产生事件。
bool sticky_touch_take_press(StickyTouchPress &press);

// Discards all queued presses recorded for a previous interaction context.
// 丢弃为上一个交互场景记录的全部排队按下事件。
void sticky_touch_clear_press();
