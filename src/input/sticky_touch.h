#pragma once

#include <cstdint>

#include "esp_err.h"

// One released tap in logical display coordinates.
// 一次已经抬起的轻触，坐标采用逻辑屏幕方向。
struct StickyTouchPress {
    uint16_t x = 0;
    uint16_t y = 0;
    uint32_t captured_at_ms = 0;
};

// One complete contact path used to recognize launcher swipe gestures.
// 一条完整接触轨迹，用于识别应用选择器滑动手势。
struct StickyTouchInteraction {
    uint16_t start_x = 0;
    uint16_t start_y = 0;
    uint16_t end_x = 0;
    uint16_t end_y = 0;
    uint32_t started_at_ms = 0;
    uint32_t ended_at_ms = 0;
};

// Powers the GT911 and starts the coordinate sampling task.
// 为GT911上电并启动坐标采样任务。
esp_err_t sticky_touch_init();

// Stops the polling task before touch power is removed for deep sleep.
// 在深度睡眠关闭触摸供电前，安全停止轮询任务。
esp_err_t sticky_touch_stop();

// Consumes the oldest queued tap after the finger is released without a drag.
// 手指未发生拖动并抬起后，按顺序取出最早的轻触事件。
bool sticky_touch_take_press(StickyTouchPress &press);

// Consumes one completed finger path from contact to release.
// 取出一条从手指接触到抬起为止的完整轨迹。
bool sticky_touch_take_interaction(StickyTouchInteraction &interaction);

// Discards all queued presses recorded for a previous interaction context.
// 丢弃为上一个交互场景记录的全部排队按下事件。
void sticky_touch_clear_press();

// Discards completed paths belonging to a previous app context.
// 丢弃属于上一个APP场景的已完成触摸轨迹。
void sticky_touch_clear_interaction();

// Returns the most recent physical touch timestamp in milliseconds.
// 返回最近一次实体触摸发生的毫秒时间戳。
uint32_t sticky_touch_last_activity_ms();
