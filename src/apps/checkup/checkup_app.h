#pragma once
#include <cstdint>
#include "esp_err.h"
class Canvas;
esp_err_t checkup_app_start(Canvas &canvas);
esp_err_t checkup_app_pause();
esp_err_t checkup_app_resume();
esp_err_t checkup_app_prepare_power_sleep(uint32_t &current, uint32_t &next);
uint32_t checkup_app_power_sleep_timeout_ms();
