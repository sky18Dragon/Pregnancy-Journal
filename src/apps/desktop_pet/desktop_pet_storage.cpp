#include "desktop_pet_storage.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <iterator>

#include "app_log.h"
#include "desktop_pet_storage_record.h"
#include "nvs.h"

namespace {

constexpr char kNamespace[] = "sticky_pet";
constexpr char kTag[] = "desktop_pet_storage";
constexpr char kLegacyStateKey[] = "state";
constexpr const char *kSlotKeys[kPetSaveSlotCount] = {
    "state_a",
    "state_b",
};
constexpr uint32_t kLegacyStateVersion = 2U;
constexpr uint32_t kLegacyCoreStateVersion = 1U;
constexpr uint32_t kLegacyDesktopStateVersion = 3U;
constexpr uint32_t kLegacyDesktopStateVersionV4 = 4U;
constexpr uint32_t kLegacyDesktopStateVersionV5 = 5U;
constexpr uint32_t kLegacyDesktopStateVersionV6 = 6U;

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

struct LegacyDesktopPetStateV6 {
    uint32_t version = kLegacyDesktopStateVersionV6;
    PetCoreState pet = {};
    uint8_t hatch_taps = 0U;
    char name[kDesktopPetNameMaximumLength + 1U] = {};
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
    desktop_pet_outing_sanitize_plan(state.outing_plan);
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

// Preserves the named version-6 pet and starts with no outing decision.
// 保留版本6中已经命名的宠物，并从尚未生成外出决定开始。
void migrate_v6(const LegacyDesktopPetStateV6 &legacy,
                DesktopPetState &state)
{
    state = {};
    state.pet = legacy.pet;
    state.hatch_taps = legacy.hatch_taps;
    std::memcpy(state.name, legacy.name, sizeof(state.name));
    sanitize(state);
}

esp_err_t read_slot(nvs_handle_t handle,
                    const char *key,
                    DesktopPetStorageRecord &record,
                    bool &present)
{
    record = {};
    present = false;
    size_t size = 0U;
    esp_err_t result = nvs_get_blob(handle, key, nullptr, &size);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (result != ESP_OK) {
        return result;
    }
    present = true;
    if (size != sizeof(record)) {
        return ESP_OK;
    }
    return nvs_get_blob(handle, key, &record, &size);
}

esp_err_t read_slots(nvs_handle_t handle,
                     DesktopPetStorageRecord *records,
                     bool *present)
{
    for (size_t index = 0U; index < kPetSaveSlotCount; ++index) {
        const esp_err_t result = read_slot(
            handle, kSlotKeys[index], records[index], present[index]);
        if (result != ESP_OK) {
            return result;
        }
    }
    return ESP_OK;
}

esp_err_t load_legacy_state(nvs_handle_t handle,
                            DesktopPetState &state,
                            bool &found)
{
    size_t size = 0U;
    esp_err_t result = nvs_get_blob(
        handle, kLegacyStateKey, nullptr, &size);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        state = {};
        return ESP_OK;
    }
    if (result != ESP_OK) {
        return result;
    }

    constexpr size_t kMaximumRecordSize = std::max(
        {sizeof(DesktopPetState),
         sizeof(LegacyDesktopPetStateV6),
         sizeof(LegacyDesktopPetStateV5),
         sizeof(LegacyDesktopPetStateV4),
         sizeof(LegacyDesktopPetStateV3),
         sizeof(LegacyDesktopPetStateV2)});
    std::array<uint8_t, kMaximumRecordSize> bytes = {};
    if (size > bytes.size()) {
        state = {};
        return ESP_OK;
    }
    result = nvs_get_blob(
        handle, kLegacyStateKey, bytes.data(), &size);
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
    if (stored_version == kLegacyDesktopStateVersionV6 &&
        size == sizeof(LegacyDesktopPetStateV6)) {
        LegacyDesktopPetStateV6 legacy = {};
        std::memcpy(&legacy, bytes.data(), sizeof(legacy));
        migrate_v6(legacy, state);
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

esp_err_t erase_key_if_present(nvs_handle_t handle, const char *key)
{
    const esp_err_t result = nvs_erase_key(handle, key);
    return result == ESP_ERR_NVS_NOT_FOUND ? ESP_OK : result;
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

    DesktopPetStorageRecord records[kPetSaveSlotCount] = {};
    bool present[kPetSaveSlotCount] = {};
    result = read_slots(handle, records, present);
    if (result != ESP_OK) {
        nvs_close(handle);
        return result;
    }
    const int newest = desktop_pet_storage_record_select_newest(
        records, kPetSaveSlotCount);
    if (newest >= 0) {
        const size_t selected = static_cast<size_t>(newest);
        state = records[selected].state;
        sanitize(state);
        found = true;
        nvs_close(handle);
        const bool peer_valid = desktop_pet_storage_record_validate(
            records[(selected + 1U) % kPetSaveSlotCount]);
        STICKY_LOGI(kTag,
                    "pet_storage=load source=slot_%c sequence=%u peer_valid=%u result=ok",
                    selected == 0U ? 'a' : 'b',
                    static_cast<unsigned>(
                        records[selected].header.sequence),
                    peer_valid ? 1U : 0U);
        return ESP_OK;
    }
    result = load_legacy_state(handle, state, found);
    nvs_close(handle);
    if (result == ESP_OK) {
        STICKY_LOGI(kTag,
                    "pet_storage=load source=%s slot_a_present=%u slot_b_present=%u result=ok",
                    found ? "legacy" : "new",
                    present[0] ? 1U : 0U,
                    present[1] ? 1U : 0U);
    }
    return result;
}

esp_err_t desktop_pet_storage_save(const DesktopPetState &state)
{
    nvs_handle_t handle = 0;
    esp_err_t result = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (result != ESP_OK) {
        return result;
    }
    DesktopPetStorageRecord records[kPetSaveSlotCount] = {};
    bool present[kPetSaveSlotCount] = {};
    result = read_slots(handle, records, present);
    if (result != ESP_OK) {
        nvs_close(handle);
        return result;
    }
    const int newest = desktop_pet_storage_record_select_newest(
        records, kPetSaveSlotCount);
    const uint32_t sequence = newest >= 0
        ? records[static_cast<size_t>(newest)].header.sequence + 1U
        : 1U;
    const size_t write_slot =
        desktop_pet_storage_record_select_write_slot(
            records, kPetSaveSlotCount);
    const DesktopPetStorageRecord next_record =
        desktop_pet_storage_record_make(
            state, sequence, state.pet.last_rtc_epoch_seconds);
    result = nvs_set_blob(
        handle,
        kSlotKeys[write_slot],
        &next_record,
        sizeof(next_record));
    if (result == ESP_OK) {
        result = nvs_commit(handle);
    }
    nvs_close(handle);
#if STICKY_LOG_DESKTOP_PET_ENABLED
    if (result == ESP_OK) {
        STICKY_LOGD(kTag,
                    "pet_storage=save target=slot_%c sequence=%u result=ok",
                    write_slot == 0U ? 'a' : 'b',
                    static_cast<unsigned>(sequence));
    }
#endif
    return result;
}

esp_err_t desktop_pet_storage_reset()
{
    nvs_handle_t handle = 0;
    esp_err_t result = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (result != ESP_OK) {
        return result;
    }
    result = erase_key_if_present(handle, kLegacyStateKey);
    for (size_t index = 0U; index < kPetSaveSlotCount; ++index) {
        const esp_err_t slot_result = erase_key_if_present(
            handle, kSlotKeys[index]);
        if (result == ESP_OK && slot_result != ESP_OK) {
            result = slot_result;
        }
    }
    if (result == ESP_OK) {
        result = nvs_commit(handle);
    }
    nvs_close(handle);
    return result;
}
