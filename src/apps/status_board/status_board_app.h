#pragma once

#include "canvas.h"
#include "esp_err.h"

// Starts the standalone landscape status-board application.
// 启动独立的横屏状态牌APP。
esp_err_t status_board_app_start(Canvas &canvas);

// Stores the landscape direction used by drawing and touch conversion.
// 保存状态牌绘图与触摸换算共用的横屏方向。
void status_board_app_set_display_rotation(CanvasRotation rotation);

esp_err_t status_board_app_pause();
esp_err_t status_board_app_resume();
