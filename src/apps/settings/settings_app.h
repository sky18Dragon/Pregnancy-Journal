#pragma once

#include <cstdint>

#include "esp_err.h"

class Canvas;

esp_err_t settings_app_start(Canvas &canvas);
esp_err_t settings_app_pause();
esp_err_t settings_app_resume();
esp_err_t settings_app_prepare_power_sleep(uint32_t &current_epoch,
                                           uint32_t &next_event_epoch);
uint32_t settings_app_power_sleep_timeout_ms();
bool settings_app_power_sleep_allowed();
bool settings_app_take_home_request();
bool settings_app_take_pregnancy_request();
