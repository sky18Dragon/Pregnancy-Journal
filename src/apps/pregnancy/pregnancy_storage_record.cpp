#include "pregnancy_storage_record.h"

#include <cstddef>
#include <cstdint>

namespace {

uint32_t checksum_without_tail(const PregnancyStorageRecord &record)
{
    constexpr uint32_t kFnvOffset = 2166136261U;
    constexpr uint32_t kFnvPrime = 16777619U;
    const auto *bytes = reinterpret_cast<const uint8_t *>(&record);
    uint32_t checksum = kFnvOffset;
    for (size_t index = 0U;
         index < offsetof(PregnancyStorageRecord, checksum);
         ++index) {
        checksum ^= bytes[index];
        checksum *= kFnvPrime;
    }
    return checksum;
}

}  // namespace

PregnancyStorageRecord pregnancy_storage_record_make(
    const PregnancyDate &due_date)
{
    PregnancyStorageRecord record = {};
    record.magic = kPregnancyStorageMagic;
    record.version = kPregnancyStorageVersion;
    record.due_year = due_date.year;
    record.due_month = due_date.month;
    record.due_day = due_date.day;
    record.checksum = checksum_without_tail(record);
    return record;
}

bool pregnancy_storage_record_validate(
    const PregnancyStorageRecord &record)
{
    const PregnancyDate due_date =
        pregnancy_storage_record_due_date(record);
    return record.magic == kPregnancyStorageMagic &&
           record.version == kPregnancyStorageVersion &&
           pregnancy_date_valid(due_date) &&
           record.checksum == checksum_without_tail(record);
}

PregnancyDate pregnancy_storage_record_due_date(
    const PregnancyStorageRecord &record)
{
    return {record.due_year, record.due_month, record.due_day};
}
