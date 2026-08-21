#pragma once

#include "canvas.h"
#include "esp_err.h"

// Starts the standalone landscape status-board application.
// 启动独立的横屏状态牌APP。
esp_err_t status_board_app_start(Canvas &canvas);

// Stores the landscape direction used by drawing and touch conversion.
// 保存状态牌绘图与触摸换算共用的横屏方向。
void status_board_app_set_display_rotation(CanvasRotation rotation);

esp_err_t status_board_app_pause();
esp_err_t status_board_app_resume();

// Pauses the app and saves the stable status page in RTC memory.
// 暂停APP，并把稳定的状态页面保存到RTC内存。
esp_err_t status_board_app_prepare_power_sleep();

// Returns the battery idle timeout for the current page; zero blocks it.
// 返回当前页面的电池空闲休眠时间；零表示暂缓自动休眠。
uint32_t status_board_app_power_sleep_timeout_ms();
