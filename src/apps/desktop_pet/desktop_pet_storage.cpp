#include "desktop_pet_storage.h"

#include <algorithm>

#include "nvs.h"

namespace {

constexpr char kNamespace[] = "sticky_pet";
constexpr char kStateKey[] = "state";
constexpr uint32_t kLegacyStateVersion = 2U;

struct LegacyDesktopPetStateV2 {
    uint32_t version = kLegacyStateVersion;
    uint16_t growth = 10U;
    uint8_t love = 18U;
    uint16_t day = 1U;
    uint16_t growth_earned_today = 0U;
    uint8_t love_earned_today = 0U;
    uint8_t feed_count_today = 0U;
    uint8_t pet_count_today = 0U;
    uint8_t play_count_today = 0U;
    uint16_t foodie_score = 0U;
    uint16_t affectionate_score = 0U;
    uint16_t active_score = 0U;
};

const PetCoreProfile &storage_profile()
{
#if STICKY_DESKTOP_PET_TEST_MODE
    return pet_core_test_profile();
#else
    return pet_core_production_profile();
#endif
}

void sanitize(DesktopPetState &state)
{
    state.version = kDesktopPetStateVersion;
    pet_core_sanitize(state.pet, storage_profile());
    state.pet.growth = std::min<uint16_t>(
        state.pet.growth, desktop_pet_state_growth_limit(state));
}

// Converts the accepted version-2 Hatchling record into the shared core.
// 将已使用的版本2幼兔存档转换为共享核心状态。
void migrate_v2(const LegacyDesktopPetStateV2 &legacy,
                DesktopPetState &state)
{
    state = {};
    state.pet.growth = legacy.growth;
    state.pet.bond = legacy.love;
    state.pet.day = legacy.day;
    state.pet.growth_earned_today = legacy.growth_earned_today;
    state.pet.bond_earned_today = legacy.love_earned_today;
    state.pet.feed_count_today = legacy.feed_count_today;
    state.pet.pet_count_today = legacy.pet_count_today;
    state.pet.play_count_today = legacy.play_count_today;
    state.pet.foodie_score = legacy.foodie_score;
    state.pet.affectionate_score = legacy.affectionate_score;
    state.pet.active_score = legacy.active_score;
    sanitize(state);
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

    size_t size = 0U;
    result = nvs_get_blob(handle, kStateKey, nullptr, &size);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(handle);
        state = {};
        return ESP_OK;
    }
    if (result != ESP_OK) {
        nvs_close(handle);
        return result;
    }

    if (size == sizeof(state)) {
        result = nvs_get_blob(handle, kStateKey, &state, &size);
        nvs_close(handle);
        if (result != ESP_OK || state.version != kDesktopPetStateVersion) {
            state = {};
            return result == ESP_OK ? ESP_OK : result;
        }
        sanitize(state);
        found = true;
        return ESP_OK;
    }

    if (size == sizeof(LegacyDesktopPetStateV2)) {
        LegacyDesktopPetStateV2 legacy = {};
        result = nvs_get_blob(handle, kStateKey, &legacy, &size);
        nvs_close(handle);
        if (result != ESP_OK || legacy.version != kLegacyStateVersion) {
            state = {};
            return result == ESP_OK ? ESP_OK : result;
        }
        migrate_v2(legacy, state);
        found = true;
        return ESP_OK;
    }

    nvs_close(handle);
    state = {};
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
