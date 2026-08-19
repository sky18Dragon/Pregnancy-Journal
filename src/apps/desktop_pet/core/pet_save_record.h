#pragma once

#include <cstddef>
#include <cstdint>

#include "pet_core.h"

// Record validation and slot ordering are adapted from the MIT-licensed
// esp32-artoria-tamagotchi SaveManager.
// 存档校验与槽位排序改编自MIT许可的esp32-artoria-tamagotchi SaveManager。

constexpr uint32_t kPetSaveMagic = 0x53504554U;
constexpr uint16_t kPetSaveFormatVersion = 1U;
constexpr uint8_t kPetSaveSlotCount = 2U;

struct PetSaveHeader {
    uint32_t magic = kPetSaveMagic;
    uint16_t format_version = kPetSaveFormatVersion;
    uint16_t state_size = sizeof(PetCoreState);
    uint16_t checksum = 0U;
    uint16_t reserved = 0U;
    uint32_t sequence = 0U;
    uint32_t saved_epoch_seconds = 0U;
};

struct PetSaveRecord {
    PetSaveHeader header = {};
    PetCoreState state = {};
};

uint16_t pet_save_checksum(const uint8_t *data, size_t size);
PetSaveRecord pet_save_make_record(const PetCoreState &state,
                                   uint32_t sequence,
                                   uint32_t saved_epoch_seconds);
bool pet_save_validate(const PetSaveRecord &record);

// Returns the newest valid slot index, or -1 when neither slot is valid.
// 返回最新有效槽位索引；两个槽都无效时返回-1。
int pet_save_select_newest(const PetSaveRecord *records, size_t count);

// Returns the slot that should receive the next record.
// 返回下一次应写入的槽位。
size_t pet_save_select_write_slot(const PetSaveRecord *records, size_t count);
