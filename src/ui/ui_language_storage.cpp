#include "ui_language_storage.h"

#include "nvs.h"

namespace {

constexpr char kNamespace[] = "ui_settings";
constexpr char kLanguageKey[] = "language";

}  // namespace

esp_err_t ui_language_storage_load()
{
    nvs_handle_t handle = 0;
    esp_err_t result = nvs_open(kNamespace, NVS_READONLY, &handle);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        ui_language_set(UiLanguage::English);
        return ESP_OK;
    }
    if (result != ESP_OK) {
        return result;
    }

    uint8_t value = static_cast<uint8_t>(UiLanguage::English);
    result = nvs_get_u8(handle, kLanguageKey, &value);
    nvs_close(handle);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        ui_language_set(UiLanguage::English);
        return ESP_OK;
    }
    if (result != ESP_OK) {
        return result;
    }
    ui_language_set(value == static_cast<uint8_t>(
                                UiLanguage::ChineseSimplified)
                        ? UiLanguage::ChineseSimplified
                        : UiLanguage::English);
    return ESP_OK;
}

esp_err_t ui_language_storage_save(UiLanguage language)
{
    nvs_handle_t handle = 0;
    esp_err_t result = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (result != ESP_OK) {
        return result;
    }
    result = nvs_set_u8(handle,
                        kLanguageKey,
                        static_cast<uint8_t>(language));
    if (result == ESP_OK) {
        result = nvs_commit(handle);
    }
    nvs_close(handle);
    if (result == ESP_OK) {
        ui_language_set(language);
    }
    return result;
}

