#pragma once

enum class StickyAppId {
  // The numeric values are persisted in saved state, so they are never
  // renumbered. IDs 0 and 3 to 6 belonged to retired applications (Home,
  // Reminder, Checkup, Journal and Advice) and must not be reused; a saved state
  // that still names one of them falls back to the default application.
  Settings = 1,
  Pregnancy = 2,
  Weight = 7,
  Kicks = 8,
};

bool sticky_app_id_valid(StickyAppId app);

// Returns the stable English identifier used by launcher logs.
// 返回应用选择日志使用的稳定英文标识。
const char *sticky_app_id_name(StickyAppId app);
