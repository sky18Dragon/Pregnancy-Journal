#pragma once

#include "esp_err.h"

// Enables the onboard BQ25616 charging path and external-power detection.
// 启用板载BQ25616充电路径和外部电源检测。
esp_err_t board_charger_init();
