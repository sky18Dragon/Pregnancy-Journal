#include "weight_storage.h"

#include <cstddef>
namespace {
uint32_t checksum(const WeightStorageRecord &record) {
  const auto *bytes = reinterpret_cast<const uint8_t *>(&record);
  uint32_t hash = 2166136261U;
  for (size_t i = 0U; i < offsetof(WeightStorageRecord, checksum); ++i) {
    hash ^= bytes[i];
    hash *= 16777619U;
  }
  return hash;
}
} // namespace

bool weight_storage_record_valid(const WeightStorageRecord &record) {
  return record.magic == kWeightStorageMagic &&
         record.version == kWeightStorageVersion &&
         record.size == sizeof(WeightRecord) &&
         weight_record_valid(record.item) &&
         record.checksum == checksum(record);
}
