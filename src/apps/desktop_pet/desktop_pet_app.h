#pragma once

#include "esp_err.h"

class Canvas;

// Starts the standalone portrait desktop-pet application.
// 启动独立运行的竖屏桌宠应用。
esp_err_t desktop_pet_app_start(Canvas &canvas);

