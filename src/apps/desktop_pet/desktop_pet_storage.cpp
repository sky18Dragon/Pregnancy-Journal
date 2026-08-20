#include "desktop_pet_storage.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <iterator>

#include "nvs.h"

namespace {

constexpr char kNamespace[] = "sticky_pet";
constexpr char kStateKey[] = "state";
constexpr uint32_t kLegacyStateVersion = 2U;
constexpr uint32_t kLegacyCoreStateVersion = 1U;
constexpr uint32_t kLegacyDesktopStateVersion = 3U;
constexpr uint32_t kLegacyDesktopStateVersionV4 = 4U;
constexpr uint32_t kLegacyDesktopStateVersionV5 = 5U;

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

struct LegacyPetCoreStateV1 {
    uint32_t version = kLegacyCoreStateVersion;
    PetLifeStage stage = PetLifeStage::Hatchling;
    PetActivity activity = PetActivity::Idle;
    PetNeeds needs = {};
    uint16_t growth = 10U;
    uint8_t bond = 18U;
    uint16_t day = 1U;
    uint32_t age_minutes = 0U;
    uint32_t last_rtc_epoch_seconds = 0U;
    uint32_t current_day_key = 0U;
    uint32_t last_care_day_key = 0U;
    uint16_t care_streak = 0U;
    uint16_t best_care_streak = 0U;
    uint16_t care_mistakes = 0U;
    uint8_t mistake_cooldown_minutes = 0U;
    uint8_t waste_count = 0U;
    uint16_t waste_minute_accumulator = 0U;
    uint16_t growth_earned_today = 0U;
    uint8_t bond_earned_today = 0U;
    uint8_t feed_count_today = 0U;
    uint8_t pet_count_today = 0U;
    uint8_t play_count_today = 0U;
    uint16_t foodie_score = 0U;
    uint16_t affectionate_score = 0U;
    uint16_t active_score = 0U;
    bool evolution_ready = false;
    uint16_t recent_dialogue_ids[kPetRecentDialogueCount] = {};
};

struct LegacyDesktopPetStateV3 {
    uint32_t version = kLegacyDesktopStateVersion;
    LegacyPetCoreStateV1 pet = {};
};

struct LegacyDesktopPetStateV4 {
    uint32_t version = kLegacyDesktopStateVersionV4;
    PetCoreState pet = {};
};

struct LegacyDesktopPetStateV5 {
    uint32_t version = kLegacyDesktopStateVersionV5;
    PetCoreState pet = {};
    uint8_t hatch_taps = 0U;
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
    if (state.pet.stage == PetLifeStage::Egg) {
        state.hatch_taps = std::min<uint8_t>(
            state.hatch_taps,
            static_cast<uint8_t>(kDesktopPetRequiredHatchTaps - 1U));
    } else {
        state.hatch_taps = kDesktopPetRequiredHatchTaps;
    }
    state.pet.growth = std::min<uint16_t>(
        state.pet.growth, desktop_pet_state_growth_limit(state));
    state.name[kDesktopPetNameMaximumLength] = '\0';
    if (state.name[0] != '\0') {
        char name_copy[kDesktopPetNameMaximumLength + 1U] = {};
        std::memcpy(name_copy, state.name, sizeof(name_copy));
        if (!desktop_pet_state_set_name(state, name_copy)) {
            state.name[0] = '\0';
        }
    }
}

// Converts the accepted version-2 Hatchling record into the shared core.
// 将已使用的版本2幼兔存档转换为共享核心状态。
void migrate_v2(const LegacyDesktopPetStateV2 &legacy,
                DesktopPetState &state)
{
    state = {};
    state.pet.stage = PetLifeStage::Hatchling;
    state.hatch_taps = kDesktopPetRequiredHatchTaps;
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

// Copies the complete version-3 Child record into the current core.
// 将完整的版本3儿童期存档复制到当前核心结构。
void migrate_v3(const LegacyDesktopPetStateV3 &legacy,
                DesktopPetState &state)
{
    state = {};
    state.hatch_taps = kDesktopPetRequiredHatchTaps;
    state.pet.stage = legacy.pet.stage == PetLifeStage::Egg
                          ? PetLifeStage::Hatchling
                          : legacy.pet.stage;
    state.pet.activity = legacy.pet.activity;
    state.pet.needs = legacy.pet.needs;
    state.pet.growth = legacy.pet.growth;
    state.pet.bond = legacy.pet.bond;
    state.pet.day = legacy.pet.day;
    state.pet.age_minutes = legacy.pet.age_minutes;
    state.pet.last_rtc_epoch_seconds = legacy.pet.last_rtc_epoch_seconds;
    state.pet.current_day_key = legacy.pet.current_day_key;
    state.pet.last_care_day_key = legacy.pet.last_care_day_key;
    state.pet.care_streak = legacy.pet.care_streak;
    state.pet.best_care_streak = legacy.pet.best_care_streak;
    state.pet.care_mistakes = legacy.pet.care_mistakes;
    state.pet.mistake_cooldown_minutes =
        legacy.pet.mistake_cooldown_minutes;
    state.pet.waste_count = legacy.pet.waste_count;
    state.pet.waste_minute_accumulator =
        legacy.pet.waste_minute_accumulator;
    state.pet.growth_earned_today = legacy.pet.growth_earned_today;
    state.pet.bond_earned_today = legacy.pet.bond_earned_today;
    state.pet.feed_count_today = legacy.pet.feed_count_today;
    state.pet.pet_count_today = legacy.pet.pet_count_today;
    state.pet.play_count_today = legacy.pet.play_count_today;
    state.pet.foodie_score = legacy.pet.foodie_score;
    state.pet.affectionate_score = legacy.pet.affectionate_score;
    state.pet.active_score = legacy.pet.active_score;
    state.pet.evolution_ready = legacy.pet.evolution_ready;
    std::copy(std::begin(legacy.pet.recent_dialogue_ids),
              std::end(legacy.pet.recent_dialogue_ids),
              std::begin(state.pet.recent_dialogue_ids));
    sanitize(state);
}

// Preserves every accepted version-4 pet field and marks it as already hatched.
// 保留版本4存档的全部宠物字段，并标记为已完成孵化。
void migrate_v4(const LegacyDesktopPetStateV4 &legacy,
                DesktopPetState &state)
{
    state = {};
    state.pet = legacy.pet;
    if (state.pet.stage == PetLifeStage::Egg) {
        state.pet.stage = PetLifeStage::Hatchling;
    }
    state.hatch_taps = kDesktopPetRequiredHatchTaps;
    sanitize(state);
}

// Preserves version-5 hatching progress with an optional empty name.
// 保留版本5的孵化进度，并允许已有宠物暂时保持未命名状态。
void migrate_v5(const LegacyDesktopPetStateV5 &legacy,
                DesktopPetState &state)
{
    state = {};
    state.pet = legacy.pet;
    state.hatch_taps = legacy.hatch_taps;
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

    constexpr size_t kMaximumRecordSize = std::max(
        {sizeof(DesktopPetState),
         sizeof(LegacyDesktopPetStateV5),
         sizeof(LegacyDesktopPetStateV4),
         sizeof(LegacyDesktopPetStateV3),
         sizeof(LegacyDesktopPetStateV2)});
    std::array<uint8_t, kMaximumRecordSize> bytes = {};
    if (size > bytes.size()) {
        nvs_close(handle);
        state = {};
        return ESP_OK;
    }
    result = nvs_get_blob(handle, kStateKey, bytes.data(), &size);
    nvs_close(handle);
    if (result != ESP_OK || size < sizeof(uint32_t)) {
        state = {};
        return result == ESP_OK ? ESP_OK : result;
    }

    uint32_t stored_version = 0U;
    std::memcpy(&stored_version, bytes.data(), sizeof(stored_version));
    if (stored_version == kDesktopPetStateVersion &&
        size == sizeof(DesktopPetState)) {
        std::memcpy(&state, bytes.data(), sizeof(state));
        sanitize(state);
        found = true;
        return ESP_OK;
    }
    if (stored_version == kLegacyDesktopStateVersionV5 &&
        size == sizeof(LegacyDesktopPetStateV5)) {
        LegacyDesktopPetStateV5 legacy = {};
        std::memcpy(&legacy, bytes.data(), sizeof(legacy));
        migrate_v5(legacy, state);
        found = true;
        return ESP_OK;
    }
    if (stored_version == kLegacyDesktopStateVersionV4 &&
        size == sizeof(LegacyDesktopPetStateV4)) {
        LegacyDesktopPetStateV4 legacy = {};
        std::memcpy(&legacy, bytes.data(), sizeof(legacy));
        migrate_v4(legacy, state);
        found = true;
        return ESP_OK;
    }
    if (stored_version == kLegacyDesktopStateVersion &&
        size == sizeof(LegacyDesktopPetStateV3)) {
        LegacyDesktopPetStateV3 legacy = {};
        std::memcpy(&legacy, bytes.data(), sizeof(legacy));
        migrate_v3(legacy, state);
        found = true;
        return ESP_OK;
    }
    if (stored_version == kLegacyStateVersion &&
        size == sizeof(LegacyDesktopPetStateV2)) {
        LegacyDesktopPetStateV2 legacy = {};
        std::memcpy(&legacy, bytes.data(), sizeof(legacy));
        migrate_v2(legacy, state);
        found = true;
        return ESP_OK;
    }

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
