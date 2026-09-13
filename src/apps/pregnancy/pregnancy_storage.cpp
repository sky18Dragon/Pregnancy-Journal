#include "pregnancy_storage.h"

#include <cstddef>

#include "app_log.h"
#include "nvs.h"
#include "pregnancy_storage_record.h"

namespace {

constexpr char kTag[] = "pregnancy_storage";
constexpr char kNamespace[] = "pregnancy";
constexpr char kConfigKey[] = "config";
constexpr char kBackupKey[] = "config_b";

struct PregnancyStorageRecordV1 {
    uint32_t magic;
    uint16_t version;
    uint16_t due_year;
    uint8_t due_month;
    uint8_t due_day;
    uint8_t reserved[2];
    uint32_t checksum;
};

uint32_t checksum_v1(const PregnancyStorageRecordV1 &record)
{
    constexpr uint32_t kOffset = 2166136261U;
    constexpr uint32_t kPrime = 16777619U;
    const auto *bytes = reinterpret_cast<const uint8_t *>(&record);
    uint32_t checksum = kOffset;
    for (size_t index = 0U;
         index < offsetof(PregnancyStorageRecordV1, checksum);
         ++index) {
        checksum ^= bytes[index];
        checksum *= kPrime;
    }
    return checksum;
}

bool migrate_v1(const void *data, size_t size, PregnancyProfile &profile)
{
    if (size != sizeof(PregnancyStorageRecordV1)) return false;
    const auto &legacy = *static_cast<const PregnancyStorageRecordV1 *>(data);
    if (legacy.magic != kPregnancyStorageMagic || legacy.version != 1U ||
        legacy.checksum != checksum_v1(legacy)) {
        return false;
    }
    return pregnancy_profile_from_due_date(
        {legacy.due_year, legacy.due_month, legacy.due_day}, profile);
}

bool decode_record(const uint8_t *bytes, size_t size,
                   PregnancyProfile &profile, bool &migrated)
{
    migrated = false;
    if (size == sizeof(PregnancyStorageRecord)) {
        const auto &record =
            *reinterpret_cast<const PregnancyStorageRecord *>(bytes);
        if (!pregnancy_storage_record_validate(record)) return false;
        profile = pregnancy_storage_record_profile(record);
        return true;
    }
    migrated = migrate_v1(bytes, size, profile);
    return migrated;
}

bool load_key(nvs_handle_t handle, const char *key, PregnancyProfile &profile,
              bool &migrated)
{
    uint8_t bytes[sizeof(PregnancyStorageRecord)] = {};
    size_t size = sizeof(bytes);
    if (nvs_get_blob(handle, key, bytes, &size) != ESP_OK) return false;
    return decode_record(bytes, size, profile, migrated);
}

}  // namespace

esp_err_t pregnancy_storage_load(PregnancyProfile &profile, bool &found)
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

    bool migrated = false;
    bool recovered = false;
    found = load_key(handle, kConfigKey, profile, migrated);
    if (!found) {
        found = load_key(handle, kBackupKey, profile, migrated);
        recovered = found;
    }
    nvs_close(handle);
    if (!found) {
        STICKY_LOGW(kTag,
                    "pregnancy_storage=load validity=invalid result=ignored");
        return ESP_OK;
    }
    STICKY_LOGI(kTag,
                "pregnancy_storage=load source=%s due=%04u-%02u-%02u migrated=%u recovered=%u result=ok",
                profile.use_due_date_as_primary ? "due_date" : "lmp",
                static_cast<unsigned>(profile.estimated_due_date.year),
                static_cast<unsigned>(profile.estimated_due_date.month),
                static_cast<unsigned>(profile.estimated_due_date.day),
                static_cast<unsigned>(migrated),
                static_cast<unsigned>(recovered));
    if (migrated || recovered) pregnancy_storage_save(profile);
    return ESP_OK;
}

esp_err_t pregnancy_storage_save(const PregnancyProfile &profile)
{
    app_log_register_tag(kTag);
    if (!pregnancy_profile_valid(profile)) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle = 0;
    esp_err_t result = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (result != ESP_OK) {
        return result;
    }
    const PregnancyStorageRecord record =
        pregnancy_storage_record_make(profile);
    result = nvs_set_blob(handle, kBackupKey, &record, sizeof(record));
    if (result == ESP_OK) {
        result = nvs_commit(handle);
    }
    if (result == ESP_OK)
        result = nvs_set_blob(handle, kConfigKey, &record, sizeof(record));
    if (result == ESP_OK) result = nvs_commit(handle);
    nvs_close(handle);
    if (result == ESP_OK) {
        STICKY_LOGI(kTag,
                    "pregnancy_storage=save due=%04u-%02u-%02u result=ok",
                    static_cast<unsigned>(profile.estimated_due_date.year),
                    static_cast<unsigned>(profile.estimated_due_date.month),
                    static_cast<unsigned>(profile.estimated_due_date.day));
    }
    return result;
}

esp_err_t pregnancy_storage_load(PregnancyDate &due_date, bool &found)
{
    PregnancyProfile profile = {};
    const esp_err_t result = pregnancy_storage_load(profile, found);
    if (found) due_date = profile.estimated_due_date;
    return result;
}

esp_err_t pregnancy_storage_save(const PregnancyDate &due_date)
{
    PregnancyProfile profile = {};
    return pregnancy_profile_from_due_date(due_date, profile)
               ? pregnancy_storage_save(profile)
               : ESP_ERR_INVALID_ARG;
}
