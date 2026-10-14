#pragma once

#include <cstdint>

#include "esp_err.h"
#include "weight_service.h"

constexpr uint32_t kWeightStorageMagic = 0x57475431U;
constexpr uint16_t kWeightStorageVersion = 1U;

struct WeightStorageRecord {
  uint32_t magic = 0U;
  uint16_t version = 0U;
  uint16_t size = 0U;
  WeightRecord item = {};
  uint32_t checksum = 0U;
};

bool weight_storage_record_valid(const WeightStorageRecord &record);
