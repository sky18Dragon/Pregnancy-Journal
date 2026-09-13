#pragma once

#include <cstdint>

#include "pregnancy_state.h"

constexpr uint32_t kPregnancyStorageMagic = 0x50524731U;
constexpr uint16_t kPregnancyStorageVersion = 2U;

struct PregnancyStorageRecord {
    uint32_t magic = 0U;
    uint16_t version = 0U;
    uint8_t schema_version = 0U;
    uint8_t use_due_date_as_primary = 1U;
    uint16_t lmp_year = 0U;
    uint8_t lmp_month = 0U;
    uint8_t lmp_day = 0U;
    uint16_t due_year = 0U;
    uint8_t due_month = 0U;
    uint8_t due_day = 0U;
    uint32_t checksum = 0U;
};

PregnancyStorageRecord pregnancy_storage_record_make(
    const PregnancyProfile &profile);
PregnancyStorageRecord pregnancy_storage_record_make(
    const PregnancyDate &due_date);
bool pregnancy_storage_record_validate(
    const PregnancyStorageRecord &record);
PregnancyDate pregnancy_storage_record_due_date(
    const PregnancyStorageRecord &record);
PregnancyProfile pregnancy_storage_record_profile(
    const PregnancyStorageRecord &record);
