#pragma once

#include <cstdint>

#include "canvas.h"
#include "esp_err.h"

// Starts the pregnancy dashboard with grip-aware landscape/portrait layouts.
// 启动支持随握持方向切换横竖排版的孕周看板APP。
esp_err_t pregnancy_app_start(Canvas &canvas);
void pregnancy_app_set_display_rotation(CanvasRotation rotation);
void pregnancy_app_request_setup();
// Makes the next resume show the dashboard instead of a detail page. Setup
// pages are left alone. A fresh start always opens the dashboard anyway.
// 下次恢复时显示首页而非详情页；不打断设置向导页。
void pregnancy_app_request_dashboard();
esp_err_t pregnancy_app_pause();
esp_err_t pregnancy_app_resume();

// Saves a stable page and schedules the next daily rollover when possible.
// 保存稳定页面，并在条件允许时安排下一次跨日更新。
esp_err_t pregnancy_app_prepare_power_sleep(
    uint32_t &current_epoch_seconds,
    uint32_t &next_event_epoch_seconds);

uint32_t pregnancy_app_power_sleep_timeout_ms();
// Draws the dashboard's sleep status (battery + moon in the menu slot) and returns true; false on
// other pages, where the system overlay is used.
bool pregnancy_app_draw_sleep_status(Canvas &canvas);
