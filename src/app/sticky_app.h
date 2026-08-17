#pragma once

#include "esp_err.h"

class Canvas;

// Starts the orientation-driven page and Pomodoro state machine.
// 启动由放稳姿态驱动的页面与番茄钟状态机。
esp_err_t sticky_app_start(Canvas &canvas);
