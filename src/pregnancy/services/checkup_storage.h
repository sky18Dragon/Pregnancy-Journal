#pragma once

#include <cstdint>

#include "checkup_service.h"
#include "esp_err.h"

constexpr uint32_t kCheckupStorageMagic = 0x43484B31U;
constexpr uint16_t kCheckupStorageVersion = 1U;

struct CheckupStorageRecord {
    uint32_t magic = 0U;
    uint16_t version = 0U;
    uint16_t size = 0U;
    Checkup item = {};
    uint32_t checksum = 0U;
};

bool checkup_storage_record_valid(const CheckupStorageRecord &record);
