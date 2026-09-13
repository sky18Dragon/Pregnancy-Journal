#pragma once

#include <cstdint>

#include "canvas.h"
#include "esp_err.h"

// Starts the standalone landscape pregnancy-week dashboard.
// 启动独立的横屏孕周看板APP。
esp_err_t pregnancy_app_start(Canvas &canvas);
void pregnancy_app_set_display_rotation(CanvasRotation rotation);
void pregnancy_app_request_setup();
esp_err_t pregnancy_app_pause();
esp_err_t pregnancy_app_resume();

// Saves a stable page and schedules the next daily rollover when possible.
// 保存稳定页面，并在条件允许时安排下一次跨日更新。
esp_err_t pregnancy_app_prepare_power_sleep(
    uint32_t &current_epoch_seconds,
    uint32_t &next_event_epoch_seconds);

uint32_t pregnancy_app_power_sleep_timeout_ms();
