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

// Reports whether deep sleep can preserve the current timer experience.
// 返回当前番茄钟体验是否允许进入深度睡眠。
bool pomodoro_app_power_sleep_allowed();

// Pauses the app and saves a restorable setup or paused timer page.
// 暂停APP，并保存可恢复的设置页或暂停计时页。
esp_err_t pomodoro_app_prepare_power_sleep();

// Returns the battery idle timeout for the current page; zero blocks it.
// 返回当前页面的电池空闲休眠时间；零表示暂缓自动休眠。
uint32_t pomodoro_app_power_sleep_timeout_ms();
