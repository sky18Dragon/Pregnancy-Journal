#pragma once

#include <cstdint>

#include "reminder.h"

constexpr uint32_t kReminderStorageMagic = 0x524D4431U;
constexpr uint16_t kReminderStorageVersion = 1U;

struct ReminderStorageRecord {
    uint32_t magic = 0U;
    uint16_t version = 0U;
    uint16_t payload_size = 0U;
    Reminder reminder = {};
    uint32_t checksum = 0U;
};

ReminderStorageRecord reminder_storage_record_make(const Reminder &reminder);
bool reminder_storage_record_valid(const ReminderStorageRecord &record);
