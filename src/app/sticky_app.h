#pragma once

#include "esp_err.h"
#include "sticky_app_id.h"

class Canvas;

// Starts the top-button launcher and the default Baby Week application.
// 启动顶部按键应用选择器和默认桌宠应用。
esp_err_t sticky_app_start(Canvas &canvas);

// Posts a navigation request to the coordinator. Application tasks use this
// instead of directly switching the active application.
void sticky_app_request(StickyAppId app);
void sticky_app_request_launcher();
void sticky_app_request_language_toggle();
// Called after the storage worker has removed the current PREGJNL dataset.
// The coordinator invalidates all repository-backed caches and schedules.
void sticky_app_notify_application_data_deleted();

bool sticky_app_preferences_save_failed();
