#pragma once

#include <cstddef>
#include <cstdint>

#include "core/pet_save_record.h"
#include "desktop_pet_state.h"

constexpr uint16_t kDesktopPetStorageFormatVersion = 2U;

struct DesktopPetStorageRecord {
    PetSaveHeader header = {};
    DesktopPetState state = {};
};

// Wraps the complete desktop-pet state in a checksummed slot record.
// 将完整桌宠状态封装为带校验值的槽位记录。
DesktopPetStorageRecord desktop_pet_storage_record_make(
    const DesktopPetState &state,
    uint32_t sequence,
    uint32_t saved_epoch_seconds);

bool desktop_pet_storage_record_validate(
    const DesktopPetStorageRecord &record);

int desktop_pet_storage_record_select_newest(
    const DesktopPetStorageRecord *records,
    size_t count);

size_t desktop_pet_storage_record_select_write_slot(
    const DesktopPetStorageRecord *records,
    size_t count);
