#pragma once

#include <cstddef>
#include <cstdint>

// The core model is adapted from the MIT-licensed TamaPoke and
// ESP32-TamaPetchi projects. Hardware access stays outside this module.
// 核心模型改编自MIT许可的TamaPoke和ESP32-TamaPetchi项目，硬件访问保留在模块外。

constexpr uint32_t kPetCoreStateVersion = 2U;
constexpr size_t kPetRecentDialogueCount = 5U;

enum class PetLifeStage : uint8_t {
    Egg,
    Hatchling,
    Child,
    Youth,
    Adult,
    Elder,
};

enum class PetActivity : uint8_t {
    Idle,
    Eating,
    Playing,
    Sleeping,
    Reserved,
    Petting,
    Evolving,
};

enum class PetMood : uint8_t {
    Ecstatic,
    Happy,
    Content,
    Neutral,
    Sad,
    Upset,
    Miserable,
    Hungry,
    Tired,
};

enum class PetCoreAction : uint8_t {
    Feed,
    Pet,
    Play,
    Rest,
};

enum class PetPersonalityBranch : uint8_t {
    Undecided,
    Foodie,
    Affectionate,
    Active,
};

struct PetPersonalityDecision {
    PetPersonalityBranch automatic_branch =
        PetPersonalityBranch::Undecided;
    bool choice_required = false;
};

struct PetNeeds {
    uint8_t food = 80U;
    uint8_t joy = 80U;
    uint8_t energy = 80U;
    uint8_t hygiene = 100U;
};

struct PetEnvironmentSnapshot {
    bool rtc_valid = false;
    bool elapsed_time_is_offline = false;
    uint32_t rtc_epoch_seconds = 0U;
};

struct PetCoreProfile {
    uint32_t maximum_offline_minutes = 14U * 24U * 60U;
    uint8_t food_decay_per_minute = 2U;
    uint8_t joy_decay_per_minute = 1U;
    uint8_t energy_decay_per_minute = 1U;
    uint8_t hygiene_decay_per_minute = 1U;
    uint8_t sleeping_energy_restore_per_minute = 6U;
    uint8_t sleeping_joy_floor = 35U;
    uint8_t sleeping_hygiene_floor = 45U;
    uint8_t care_need_floor = 40U;
    uint8_t daily_bond_limit = 8U;
    uint16_t hatchling_growth_limit = 30U;
    uint16_t child_growth_limit = 120U;
    uint16_t youth_growth_limit = 280U;
    uint16_t daily_growth_limit = 10U;
    uint8_t growth_multiplier = 1U;
    uint8_t bond_multiplier = 1U;
};

struct PetCoreState {
    uint32_t version = kPetCoreStateVersion;
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
    PetPersonalityBranch branch = PetPersonalityBranch::Undecided;
    bool evolution_ready = false;
    uint16_t recent_dialogue_ids[kPetRecentDialogueCount] = {};
};

struct PetCoreActionResult {
    bool changed = false;
    bool evolution_became_ready = false;
    int16_t growth_delta = 0;
    int8_t bond_delta = 0;
    PetActivity activity = PetActivity::Idle;
};

const PetCoreProfile &pet_core_production_profile();
const PetCoreProfile &pet_core_test_profile();

// Applies an RTC reading and advances bounded offline pet simulation.
// 应用RTC读数，并执行有上限的宠物离线模拟。
void pet_core_apply_environment(PetCoreState &state,
                                const PetEnvironmentSnapshot &snapshot,
                                const PetCoreProfile &profile);

// Applies one user interaction to the persistent model.
// 将一次用户互动应用到持久化模型。
PetCoreActionResult pet_core_apply_action(PetCoreState &state,
                                          PetCoreAction action,
                                          const PetCoreProfile &profile);

// Applies one interaction against an explicit care-day identifier.
// 使用明确的照料日编号执行一次互动，供加速测试时间线复用。
PetCoreActionResult pet_core_apply_action_for_day(
    PetCoreState &state,
    PetCoreAction action,
    const PetCoreProfile &profile,
    uint32_t care_day_key);

// Advances a known number of virtual minutes without reading hardware.
// 在不读取硬件的情况下推进指定的虚拟分钟数。
void pet_core_advance_minutes(PetCoreState &state,
                              uint32_t minutes,
                              const PetCoreProfile &profile,
                              bool offline);

// Advances one simulated day and resets daily reward counters.
// 推进一个模拟日期，并重置当天奖励计数。
void pet_core_advance_day(PetCoreState &state);

PetMood pet_core_mood(const PetCoreState &state);
bool pet_core_can_evolve(const PetCoreState &state,
                         const PetCoreProfile &profile);
bool pet_core_evolve(PetCoreState &state,
                     const PetCoreProfile &profile);

// Compares the three accumulated scores using the six-point route margin.
// 使用六分领先线比较三项累计分数，返回自动路线或需要选择的结果。
PetPersonalityDecision pet_core_personality_decision(
    const PetCoreState &state);

bool pet_core_choose_personality(PetCoreState &state,
                                 PetPersonalityBranch branch);
void pet_core_sanitize(PetCoreState &state,
                       const PetCoreProfile &profile);
const char *pet_core_stage_name(PetLifeStage stage);
const char *pet_core_activity_name(PetActivity activity);
const char *pet_core_mood_name(PetMood mood);
const char *pet_core_personality_name(PetPersonalityBranch branch);
