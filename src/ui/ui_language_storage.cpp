#include "ui_language_storage.h"

#include "settings_store.h"

esp_err_t ui_language_storage_load()
{
    StickyDeviceSettings settings = {};
    const esp_err_t result = sticky_settings_load(settings);
    ui_language_set(settings.language);
    return result;
}

esp_err_t ui_language_storage_save(UiLanguage language)
{
    StickyDeviceSettings settings = {};
    esp_err_t result = sticky_settings_load(settings);
    if (result == ESP_OK) {
        settings.language = language;
        result = sticky_settings_save(settings);
    }
    if (result == ESP_OK) {
        ui_language_set(language);
    }
    return result;
}
