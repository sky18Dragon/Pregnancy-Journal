#pragma once

#include <cstdint>

#include "esp_err.h"

class Canvas;

esp_err_t weight_app_start(Canvas &canvas);
// Asks the app to open today's weight entry the next time it shows (or once its
// card data has loaded, within a few seconds). A page holding unsaved input is
// left as it is. Safe to call from any task.
// 请求应用在下次显示（或卡内数据加载完成，几秒内）时打开今天的体重输入；带未保存输入的页面保持原样。
void weight_app_request_record_today();
esp_err_t weight_app_pause();
esp_err_t weight_app_resume();
esp_err_t weight_app_prepare_power_sleep(uint32_t &current, uint32_t &next);
uint32_t weight_app_power_sleep_timeout_ms();
