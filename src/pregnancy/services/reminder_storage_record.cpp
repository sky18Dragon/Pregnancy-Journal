#include "reminder_storage_record.h"

#include <cstddef>

namespace {
uint32_t checksum(const ReminderStorageRecord &record)
{
    const auto *bytes = reinterpret_cast<const uint8_t *>(&record);
    uint32_t hash = 2166136261U;
    for (size_t index = 0U; index < offsetof(ReminderStorageRecord, checksum);
         ++index) {
        hash ^= bytes[index];
        hash *= 16777619U;
    }
    return hash;
}
}  // namespace

ReminderStorageRecord reminder_storage_record_make(const Reminder &reminder)
{
    ReminderStorageRecord record = {};
    record.magic = kReminderStorageMagic;
    record.version = kReminderStorageVersion;
    record.payload_size = sizeof(Reminder);
    record.reminder = reminder;
    record.checksum = checksum(record);
    return record;
}

bool reminder_storage_record_valid(const ReminderStorageRecord &record)
{
    return record.magic == kReminderStorageMagic &&
           record.version == kReminderStorageVersion &&
           record.payload_size == sizeof(Reminder) &&
           reminder_valid(record.reminder) && record.checksum == checksum(record);
}
