#pragma once

#include "esp_err.h"

class Canvas;

// Starts the standalone Pomodoro application and opens its setup page.
// 启动独立番茄钟APP，并直接进入时间选择主页。
esp_err_t pomodoro_app_start(Canvas &canvas);
