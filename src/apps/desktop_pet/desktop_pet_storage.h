#pragma once

#include "desktop_pet_state.h"
#include "esp_err.h"

// Loads the pet record or returns a fresh default when no record exists.
// 读取桌宠存档；没有存档时返回一份新的默认状态。
esp_err_t desktop_pet_storage_load(DesktopPetState &state, bool &found);

// Saves the complete versioned pet record to NVS.
// 将完整且带版本号的桌宠状态保存到 NVS。
esp_err_t desktop_pet_storage_save(const DesktopPetState &state);

// Removes only the desktop-pet record from NVS.
// 只删除桌宠自己的 NVS 存档。
esp_err_t desktop_pet_storage_reset();

