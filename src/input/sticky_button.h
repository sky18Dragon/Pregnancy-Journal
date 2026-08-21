#pragma once

#include "esp_err.h"

// Initializes the active-low top button with the reference debounce settings.
// 使用硬件示例的消抖参数初始化低电平有效的顶部按键。
esp_err_t sticky_button_init();

// Consumes one queued single-click event.
// 读取并清除一个排队的单击事件。
bool sticky_button_take_click();
