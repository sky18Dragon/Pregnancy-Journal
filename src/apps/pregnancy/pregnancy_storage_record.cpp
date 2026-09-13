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
    const PregnancyProfile &profile)
{
    PregnancyStorageRecord record = {};
    record.magic = kPregnancyStorageMagic;
    record.version = kPregnancyStorageVersion;
    record.schema_version = profile.schema_version;
    record.use_due_date_as_primary = profile.use_due_date_as_primary ? 1U : 0U;
    record.lmp_year = profile.last_menstrual_period.year;
    record.lmp_month = profile.last_menstrual_period.month;
    record.lmp_day = profile.last_menstrual_period.day;
    record.due_year = profile.estimated_due_date.year;
    record.due_month = profile.estimated_due_date.month;
    record.due_day = profile.estimated_due_date.day;
    record.checksum = checksum_without_tail(record);
    return record;
}

PregnancyStorageRecord pregnancy_storage_record_make(
    const PregnancyDate &due_date)
{
    PregnancyProfile profile = {};
    if (!pregnancy_profile_from_due_date(due_date, profile)) {
        return {};
    }
    return pregnancy_storage_record_make(profile);
}

bool pregnancy_storage_record_validate(
    const PregnancyStorageRecord &record)
{
    return record.magic == kPregnancyStorageMagic &&
           record.version == kPregnancyStorageVersion &&
           (record.use_due_date_as_primary == 0U ||
            record.use_due_date_as_primary == 1U) &&
           pregnancy_profile_valid(pregnancy_storage_record_profile(record)) &&
           record.checksum == checksum_without_tail(record);
}


PregnancyProfile pregnancy_storage_record_profile(
    const PregnancyStorageRecord &record)
{
    PregnancyProfile profile = {};
    profile.schema_version = record.schema_version;
    profile.last_menstrual_period = {
        record.lmp_year, record.lmp_month, record.lmp_day};
    profile.estimated_due_date = {
        record.due_year, record.due_month, record.due_day};
    profile.use_due_date_as_primary = record.use_due_date_as_primary != 0U;
    return profile;
}

PregnancyDate pregnancy_storage_record_due_date(
    const PregnancyStorageRecord &record)
{
    return {record.due_year, record.due_month, record.due_day};
}
