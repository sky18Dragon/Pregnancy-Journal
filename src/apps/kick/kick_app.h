#pragma once

#include <cstdint>

#include "esp_err.h"

class Canvas;

esp_err_t kick_app_start(Canvas &canvas);
// Asks the app to start recording the next time it shows (or once its card data
// has loaded, within a few seconds), using the remembered method. A running
// counter is left as it is. Safe to call from any task.
// 请求应用在下次显示（或卡内数据加载完成，几秒内）时按记住的方式开始记录；进行中的计数保持原样。
void kick_app_request_start();
esp_err_t kick_app_pause();
esp_err_t kick_app_resume();
// Queues a top-key press made at `at_us` (esp_timer microseconds) as one counted
// movement; used while the counter page is showing. Safe to call from any task.
// 把顶键按下（按下时刻 at_us）排成一次计数；计数页显示时使用，可在任意任务调用。
void kick_app_capture_press(int64_t at_us);
esp_err_t kick_app_prepare_power_sleep(uint32_t &current, uint32_t &next);
uint32_t kick_app_power_sleep_timeout_ms();
