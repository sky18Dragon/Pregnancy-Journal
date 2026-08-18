#pragma once

#include "esp_err.h"

// Initializes the onboard buzzer in a silent state.
// 初始化板载蜂鸣器，并保持静音状态。
esp_err_t sticky_buzzer_init();

// Starts a repeating three-note alarm that remains active until stop is called.
// 启动循环三音提醒，直到调用停止函数才结束。
esp_err_t sticky_buzzer_start_alarm();

// Stops the active alarm tone immediately.
// 立即停止当前响铃。
esp_err_t sticky_buzzer_stop();
