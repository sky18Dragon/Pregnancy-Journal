#pragma once

#include "desktop_pet_state.h"
#include "esp_err.h"

// Loads the newest valid A/B slot, then accepts a legacy record for migration.
// 读取最新且有效的A/B槽位，并在需要时迁移旧版存档。
esp_err_t desktop_pet_storage_load(DesktopPetState &state, bool &found);

// Saves the complete state to the older or invalid A/B slot in NVS.
// 将完整状态写入NVS中较旧或无效的A/B槽位。
esp_err_t desktop_pet_storage_save(const DesktopPetState &state);

// Removes the legacy key and both desktop-pet save slots from NVS.
// 删除NVS中的旧版存档键和两份桌宠槽位。
esp_err_t desktop_pet_storage_reset();
