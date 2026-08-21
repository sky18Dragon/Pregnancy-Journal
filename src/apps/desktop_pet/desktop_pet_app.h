#pragma once

#include "esp_err.h"

class Canvas;

// Starts the standalone portrait desktop-pet application.
// 启动独立运行的竖屏桌宠应用。
esp_err_t desktop_pet_app_start(Canvas &canvas);

// Leaves transient pet pages and redraws the root page for the current state.
// 退出桌宠临时页面，并按当前真实状态重绘根页面。
esp_err_t desktop_pet_app_return_home();

// Cooperatively pauses display and touch ownership at a safe loop boundary.
// 在安全循环边界协作式暂停屏幕和触摸控制权。
esp_err_t desktop_pet_app_pause();

// Resumes the pet and redraws its preserved current page.
// 恢复桌宠并重新绘制保留的当前页面。
esp_err_t desktop_pet_app_resume();
