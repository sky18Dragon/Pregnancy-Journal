#pragma once

#include <cstdint>

#include "sticky_imu.h"

class Canvas;

// Renders the ordinary home placeholder in the selected settled orientation.
// 按最终放稳姿态绘制普通主页占位画面。
void app_page_render_base(Canvas &canvas, StickyImuOrientation orientation);

// Renders the ten-second touch confirmation page for a Pomodoro candidate.
// 绘制番茄钟候选动作的10秒触摸确认页。
void app_page_render_pomodoro_confirmation(
    Canvas &canvas,
    StickyImuOrientation orientation);

// Renders the active timer with the supplied remaining time.
// 绘制正在运行的番茄钟及剩余时间。
void app_page_render_pomodoro_running(
    Canvas &canvas,
    StickyImuOrientation orientation,
    uint32_t remaining_seconds);

// Renders the completed Pomodoro page.
// 绘制番茄钟完成页。
void app_page_render_pomodoro_done(
    Canvas &canvas,
    StickyImuOrientation orientation);
