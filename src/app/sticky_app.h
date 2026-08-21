#pragma once

#include "esp_err.h"

class Canvas;

// Starts the top-button launcher and the default desktop-pet application.
// 启动顶部按键应用选择器和默认桌宠应用。
esp_err_t sticky_app_start(Canvas &canvas);
