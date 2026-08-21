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

// Stops the polling task before touch power is removed for deep sleep.
// 在深度睡眠关闭触摸供电前，安全停止轮询任务。
esp_err_t sticky_touch_stop();

// Consumes the oldest queued press event. Holding a finger does not create
// more events.
// 按顺序取出最早的按下事件；手指持续按住不会重复产生事件。
bool sticky_touch_take_press(StickyTouchPress &press);

// Discards all queued presses recorded for a previous interaction context.
// 丢弃为上一个交互场景记录的全部排队按下事件。
void sticky_touch_clear_press();

// Returns the most recent physical touch timestamp in milliseconds.
// 返回最近一次实体触摸发生的毫秒时间戳。
uint32_t sticky_touch_last_activity_ms();
