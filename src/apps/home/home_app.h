#pragma once

#include <cstdint>

#include "esp_err.h"

class Canvas;

esp_err_t home_app_start(Canvas &canvas);
esp_err_t home_app_pause();
esp_err_t home_app_resume();
esp_err_t home_app_prepare_power_sleep(uint32_t &current_epoch_seconds,
                                       uint32_t &next_event_epoch_seconds);
uint32_t home_app_power_sleep_timeout_ms();

