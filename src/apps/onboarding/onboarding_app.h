#pragma once

#include "esp_err.h"

class Canvas;

// Runs the blocking first-boot tutorial before application tasks start.
// 在各APP任务启动前，以阻塞方式运行首次开机教程。
esp_err_t onboarding_app_run_if_needed(Canvas &canvas);

// Opens the complete tutorial immediately, regardless of its NVS marker.
// 忽略NVS完成标记并立即打开完整教程。
esp_err_t onboarding_app_run(Canvas &canvas);

// Clears the completion marker so the next boot opens the tutorial again.
// 清除完成标记，使教程在下一次启动时重新出现。
esp_err_t onboarding_app_reset_completion();
