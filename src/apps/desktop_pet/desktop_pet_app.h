#pragma once

#include <cstdint>

#include "esp_err.h"

class Canvas;

// Starts the standalone portrait desktop-pet application.
// 启动独立运行的竖屏桌宠应用。
esp_err_t desktop_pet_app_start(Canvas &canvas);

// Leaves transient pet pages and redraws the root page for the current state.
// 退出桌宠临时页面，并按当前真实状态重绘根页面。
esp_err_t desktop_pet_app_return_home();

// Saves the pet and returns the next arranged outing event in RTC seconds.
// 保存桌宠状态，并返回下一项已安排外出事件的RTC秒数；没有事件时返回零。
esp_err_t desktop_pet_app_prepare_power_sleep(
    uint32_t &current_epoch_seconds,
    uint32_t &next_event_epoch_seconds);

// Reports whether no editor or non-interruptible pet transition is active.
// 返回当前是否没有编辑页或不可中断的桌宠过场。
bool desktop_pet_app_power_sleep_allowed();

// Cooperatively pauses display and touch ownership at a safe loop boundary.
// 在安全循环边界协作式暂停屏幕和触摸控制权。
esp_err_t desktop_pet_app_pause();

// Resumes the pet and redraws its preserved current page.
// 恢复桌宠并重新绘制保留的当前页面。
esp_err_t desktop_pet_app_resume();

// Takes one user request to reopen the complete firmware tutorial.
// 取出一次用户主动重新打开完整固件教程的请求。
bool desktop_pet_app_take_onboarding_request();
