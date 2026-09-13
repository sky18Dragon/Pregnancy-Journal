#pragma once

#include "esp_err.h"
#include "reminder_service.h"

esp_err_t reminder_storage_load(ReminderService &service);
esp_err_t reminder_storage_save(const ReminderService &service);
bool reminder_storage_next_event(uint32_t now, uint32_t &epoch_seconds);
