#pragma once

#include "esp_err.h"
#include "ui_language.h"

// Loads the saved language, defaulting to English when no setting exists.
// 读取已保存语言；首次使用时默认英文。
esp_err_t ui_language_storage_load();

// Updates the active language and persists it in NVS.
// 切换当前语言并保存到NVS。
esp_err_t ui_language_storage_save(UiLanguage language);

