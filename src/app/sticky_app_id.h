#pragma once

enum class StickyAppId {
    // Keep the persisted numeric values stable. IDs 0, 5 and 6 belonged to
    // the retired Home, Journal and Advice applications.
    Settings = 1,
    Pregnancy = 2,
    Reminder = 3,
    Checkup = 4,
};

bool sticky_app_id_valid(StickyAppId app);

// Returns the stable English identifier used by launcher logs.
// 返回应用选择日志使用的稳定英文标识。
const char *sticky_app_id_name(StickyAppId app);
