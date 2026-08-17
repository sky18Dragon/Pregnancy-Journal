#pragma once

#include "esp_err.h"

// Powers the GT911 and starts the coordinate sampling task.
// 为GT911上电并启动坐标采样任务。
esp_err_t sticky_touch_init();
