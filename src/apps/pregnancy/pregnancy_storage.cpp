#include "pregnancy_storage.h"

#include "app_log.h"
#include "nvs.h"
#include "pregnancy_storage_record.h"

namespace {

constexpr char kTag[] = "pregnancy_storage";
constexpr char kNamespace[] = "pregnancy";
constexpr char kConfigKey[] = "config";

}  // namespace

esp_err_t pregnancy_storage_load(PregnancyDate &due_date, bool &found)
{
    app_log_register_tag(kTag);
    found = false;
    nvs_handle_t handle = 0;
    esp_err_t result = nvs_open(kNamespace, NVS_READONLY, &handle);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (result != ESP_OK) {
        return result;
    }

    PregnancyStorageRecord record = {};
    size_t size = sizeof(record);
    result = nvs_get_blob(handle, kConfigKey, &record, &size);
    nvs_close(handle);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (result != ESP_OK) {
        return result;
    }
    if (size != sizeof(record) ||
        !pregnancy_storage_record_validate(record)) {
        STICKY_LOGW(kTag,
                    "pregnancy_storage=load validity=invalid result=ignored");
        return ESP_OK;
    }

    due_date = pregnancy_storage_record_due_date(record);
    found = true;
    STICKY_LOGI(kTag,
                "pregnancy_storage=load due=%04u-%02u-%02u result=ok",
                static_cast<unsigned>(due_date.year),
                static_cast<unsigned>(due_date.month),
                static_cast<unsigned>(due_date.day));
    return ESP_OK;
}

esp_err_t pregnancy_storage_save(const PregnancyDate &due_date)
{
    app_log_register_tag(kTag);
    if (!pregnancy_date_valid(due_date)) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle = 0;
    esp_err_t result = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (result != ESP_OK) {
        return result;
    }
    const PregnancyStorageRecord record =
        pregnancy_storage_record_make(due_date);
    result = nvs_set_blob(handle, kConfigKey, &record, sizeof(record));
    if (result == ESP_OK) {
        result = nvs_commit(handle);
    }
    nvs_close(handle);
    if (result == ESP_OK) {
        STICKY_LOGI(kTag,
                    "pregnancy_storage=save due=%04u-%02u-%02u result=ok",
                    static_cast<unsigned>(due_date.year),
                    static_cast<unsigned>(due_date.month),
                    static_cast<unsigned>(due_date.day));
    }
    return result;
}
