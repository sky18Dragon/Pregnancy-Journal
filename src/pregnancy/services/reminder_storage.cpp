#include "reminder_storage.h"

#include <cstdio>

#include "app_log.h"
#include "nvs.h"
#include "reminder_storage_record.h"

namespace {
constexpr char kTag[] = "REMINDER";
constexpr char kNamespace[] = "reminders";

void key_for_slot(size_t slot, char key[8])
{
    std::snprintf(key, 8U, "r%02u", static_cast<unsigned>(slot));
}
}  // namespace

esp_err_t reminder_storage_load(ReminderService &service)
{
    service.clear();
    nvs_handle_t handle = 0;
    esp_err_t result = nvs_open(kNamespace, NVS_READONLY, &handle);
    if (result == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (result != ESP_OK) return result;
    for (size_t slot = 0U; slot < ReminderService::kCapacity; ++slot) {
        char key[8] = {};
        key_for_slot(slot, key);
        ReminderStorageRecord record = {};
        size_t size = sizeof(record);
        result = nvs_get_blob(handle, key, &record, &size);
        if (result == ESP_ERR_NVS_NOT_FOUND) continue;
        if (result == ESP_OK && size == sizeof(record) &&
            reminder_storage_record_valid(record)) {
            service.create(record.reminder);
        } else {
            STICKY_LOGW(kTag, "storage=load slot=%u result=skipped_corrupt",
                        static_cast<unsigned>(slot));
        }
    }
    nvs_close(handle);
    return ESP_OK;
}

esp_err_t reminder_storage_save(const ReminderService &service)
{
    nvs_handle_t handle = 0;
    esp_err_t result = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (result != ESP_OK) return result;
    for (size_t slot = 0U; slot < ReminderService::kCapacity; ++slot) {
        char key[8] = {};
        key_for_slot(slot, key);
        const Reminder *item = service.at(slot);
        if (item == nullptr) {
            const esp_err_t erased = nvs_erase_key(handle, key);
            if (erased != ESP_OK && erased != ESP_ERR_NVS_NOT_FOUND) {
                result = erased;
                break;
            }
        } else {
            const ReminderStorageRecord record =
                reminder_storage_record_make(*item);
            result = nvs_set_blob(handle, key, &record, sizeof(record));
            if (result != ESP_OK) break;
        }
    }
    if (result == ESP_OK || result == ESP_ERR_NVS_NOT_FOUND)
        result = nvs_commit(handle);
    nvs_close(handle);
    return result;
}

bool reminder_storage_next_event(uint32_t now, uint32_t &epoch_seconds)
{
    ReminderService service;
    return reminder_storage_load(service) == ESP_OK &&
           service.next(now, &epoch_seconds) != nullptr;
}
