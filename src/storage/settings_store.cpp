#include "settings_store.h"

#include "nvs.h"

namespace {

constexpr char kNamespace[] = "sticky_core";
constexpr char kSlotA[] = "settings_a";
constexpr char kSlotB[] = "settings_b";

bool read_slot(nvs_handle_t handle,
               const char *key,
               StickyPersistentRecord &record)
{
    size_t size = sizeof(record);
    return nvs_get_blob(handle, key, &record, &size) == ESP_OK &&
           size == sizeof(record) && sticky_persistent_valid(record);
}

}  // namespace

esp_err_t sticky_settings_load(StickyDeviceSettings &settings)
{
    settings = {};
    nvs_handle_t handle = 0;
    const esp_err_t open_result = nvs_open(kNamespace, NVS_READONLY, &handle);
    if (open_result == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (open_result != ESP_OK) {
        return open_result;
    }
    StickyPersistentRecord a = {};
    StickyPersistentRecord b = {};
    const bool a_valid = read_slot(handle, kSlotA, a);
    const bool b_valid = read_slot(handle, kSlotB, b);
    nvs_close(handle);
    StickyPersistentRecord selected = {};
    if (sticky_persistent_select(a_valid ? &a : nullptr,
                                 b_valid ? &b : nullptr,
                                 selected)) {
        settings = selected.settings;
    }
    if (!sticky_app_id_valid(settings.last_app)) {
        settings.last_app = StickyAppId::Pregnancy;
    }
    if (settings.language != UiLanguage::ChineseSimplified) {
        settings.language = UiLanguage::English;
    }
    return ESP_OK;
}

esp_err_t sticky_settings_save(const StickyDeviceSettings &settings)
{
    nvs_handle_t handle = 0;
    esp_err_t result = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (result != ESP_OK) {
        return result;
    }
    StickyPersistentRecord a = {};
    StickyPersistentRecord b = {};
    const bool a_valid = read_slot(handle, kSlotA, a);
    const bool b_valid = read_slot(handle, kSlotB, b);
    uint32_t sequence = 1U;
    if (a_valid && a.sequence >= sequence) {
        sequence = a.sequence + 1U;
    }
    if (b_valid && b.sequence >= sequence) {
        sequence = b.sequence + 1U;
    }
    const char *target = !a_valid || (b_valid && a.sequence <= b.sequence)
                             ? kSlotA
                             : kSlotB;
    const StickyPersistentRecord record =
        sticky_persistent_make(settings, sequence);
    result = nvs_set_blob(handle, target, &record, sizeof(record));
    if (result == ESP_OK) {
        result = nvs_commit(handle);
    }
    nvs_close(handle);
    return result;
}
