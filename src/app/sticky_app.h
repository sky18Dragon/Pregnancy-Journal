#pragma once

#include "esp_err.h"
#include "key_policy.h"
#include "persistent_state.h"
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
// An app publishes how the hardware keys act on the page it is showing; call it
// whenever the page changes. Until an app publishes, its keys stay Locked.
// 应用在页面变化时发布当前页对硬件按键的反应；尚未发布时按 Locked 处理。
void sticky_app_set_key_policy(StickyAppId app, KeyPolicy policy);
// Called after the storage worker has removed the current PREGJNL dataset.
// The coordinator invalidates all repository-backed caches and schedules.
void sticky_app_notify_application_data_deleted();

bool sticky_app_preferences_save_failed();

// The kick record method the user picked last (Unset until they pick one), and
// the call that records a new pick. Safe to call from any task: the coordinator
// owns the saved preferences and applies the change on its own task.
// 胎动记录方式的上次选择及其记录；可在任意任务调用，由协调器在自己的任务里落盘。
StickyKickModePref sticky_app_kick_mode_pref();
// The double-click target and the call that changes it (same rules as above).
// 双击目标及其修改接口（规则同上）。
StickyShortcutTarget sticky_app_shortcut_target();
void sticky_app_set_shortcut_target(StickyShortcutTarget target);
void sticky_app_set_kick_mode_pref(StickyKickModePref pref);
