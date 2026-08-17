#pragma once

#include "esp_err.h"

// Keeps the Sticky power latch active after startup.
// 在启动后保持 Sticky 的电源锁存状态。
esp_err_t board_power_init();

