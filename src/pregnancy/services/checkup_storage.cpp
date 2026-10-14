#include "checkup_storage.h"

#include <cstddef>
namespace {
uint32_t checksum(const CheckupStorageRecord &record)
{
    const auto *bytes = reinterpret_cast<const uint8_t *>(&record);
    uint32_t hash = 2166136261U;
    for (size_t i = 0U;
         i < offsetof(CheckupStorageRecord, checksum); ++i) {
        hash ^= bytes[i]; hash *= 16777619U;
    }
    return hash;
}
}  // namespace

bool checkup_storage_record_valid(const CheckupStorageRecord &record)
{
    return record.magic == kCheckupStorageMagic &&
           record.version == kCheckupStorageVersion &&
           record.size == sizeof(Checkup) && checkup_valid(record.item) &&
           record.checksum == checksum(record);
}
