#pragma once

#include "esp_err.h"

class Canvas;

// Starts the standalone landscape status-board application.
// 启动独立的横屏状态牌APP。
esp_err_t status_board_app_start(Canvas &canvas);
