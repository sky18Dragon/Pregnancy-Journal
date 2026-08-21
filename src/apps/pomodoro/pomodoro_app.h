#pragma once

#include "canvas.h"
#include "esp_err.h"

// Starts the standalone Pomodoro application and opens its setup page.
// 启动独立番茄钟APP，并直接进入时间选择主页。
esp_err_t pomodoro_app_start(Canvas &canvas);

// Stores the portrait direction used by drawing and touch conversion.
// 保存番茄钟绘图与触摸换算共用的竖屏方向。
void pomodoro_app_set_display_rotation(CanvasRotation rotation);

esp_err_t pomodoro_app_pause();
esp_err_t pomodoro_app_resume();
