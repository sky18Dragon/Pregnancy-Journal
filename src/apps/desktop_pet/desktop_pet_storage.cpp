#include "desktop_pet_storage.h"

#include <algorithm>

#include "nvs.h"

namespace {

constexpr char kNamespace[] = "sticky_pet";
constexpr char kStateKey[] = "state";

void sanitize(DesktopPetState &state)
{
    state.version = kDesktopPetStateVersion;
    state.growth = std::min<uint16_t>(
        state.growth, kDesktopPetHatchlingGrowthLimit);
    state.love = std::min<uint8_t>(state.love, 100U);
    state.day = std::max<uint16_t>(state.day, 1U);
}

}  // namespace

esp_err_t desktop_pet_storage_load(DesktopPetState &state, bool &found)
{
    found = false;
    nvs_handle_t handle = 0;
    esp_err_t result = nvs_open(kNamespace, NVS_READONLY, &handle);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        state = {};
        return ESP_OK;
    }
    if (result != ESP_OK) {
        return result;
    }

    size_t size = sizeof(state);
    result = nvs_get_blob(handle, kStateKey, &state, &size);
    nvs_close(handle);
    if (result == ESP_ERR_NVS_NOT_FOUND || size != sizeof(state) ||
        state.version != kDesktopPetStateVersion) {
        state = {};
        return ESP_OK;
    }
    if (result != ESP_OK) {
        return result;
    }

    sanitize(state);
    found = true;
    return ESP_OK;
}

esp_err_t desktop_pet_storage_save(const DesktopPetState &state)
{
    nvs_handle_t handle = 0;
    esp_err_t result = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (result != ESP_OK) {
        return result;
    }
    result = nvs_set_blob(handle, kStateKey, &state, sizeof(state));
    if (result == ESP_OK) {
        result = nvs_commit(handle);
    }
    nvs_close(handle);
    return result;
}

esp_err_t desktop_pet_storage_reset()
{
    nvs_handle_t handle = 0;
    esp_err_t result = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (result != ESP_OK) {
        return result;
    }
    result = nvs_erase_key(handle, kStateKey);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        result = ESP_OK;
    }
    if (result == ESP_OK) {
        result = nvs_commit(handle);
    }
    nvs_close(handle);
    return result;
}
