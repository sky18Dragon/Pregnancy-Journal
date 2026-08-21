#pragma once

#include <cstdint>

#include "esp_err.h"

// Keeps the Sticky power latch active after startup.
// 在启动后保持 Sticky 的电源锁存状态。
esp_err_t board_power_init();

// Holds the board rails, powers down peripherals, and enters deep sleep.
// 保持板级电源锁存、关闭外设，并进入深度睡眠；该函数不会返回。
[[noreturn]] void board_power_enter_deep_sleep(
    uint64_t timer_wakeup_us = 0U);
