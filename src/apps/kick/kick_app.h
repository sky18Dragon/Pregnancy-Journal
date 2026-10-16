#pragma once

#include <cstdint>

#include "esp_err.h"

class Canvas;

esp_err_t kick_app_start(Canvas &canvas);
esp_err_t kick_app_pause();
esp_err_t kick_app_resume();
// Queues a top-key press made at `at_us` (esp_timer microseconds) as one counted
// movement; used while the counter page is showing. Safe to call from any task.
// 把顶键按下（按下时刻 at_us）排成一次计数；计数页显示时使用，可在任意任务调用。
void kick_app_capture_press(int64_t at_us);
esp_err_t kick_app_prepare_power_sleep(uint32_t &current, uint32_t &next);
uint32_t kick_app_power_sleep_timeout_ms();
