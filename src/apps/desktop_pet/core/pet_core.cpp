#include "pet_core.h"

#include <algorithm>
#include <iterator>
#include <limits>

namespace {

constexpr uint32_t kSecondsPerMinute = 60U;
constexpr uint32_t kSecondsPerDay = 24U * 60U * 60U;
constexpr uint16_t kAutomaticBranchMargin = 6U;

const PetCoreProfile kProductionProfile = {};

const PetCoreProfile kTestProfile = {
    14U * 24U * 60U,
    2U,
    1U,
    1U,
    1U,
    6U,
    35U,
    45U,
    40U,
    40U,
    30U,
    120U,
    280U,
    100U,
    10U,
    5U,
};

uint8_t clamp_percent(int value)
{
    return static_cast<uint8_t>(std::clamp(value, 0, 100));
}

uint16_t saturating_add_u16(uint16_t value, uint16_t amount)
{
    const uint32_t sum = static_cast<uint32_t>(value) + amount;
    return static_cast<uint16_t>(std::min<uint32_t>(
        sum, std::numeric_limits<uint16_t>::max()));
}

uint8_t saturating_increment_u8(uint8_t value)
{
    return value == std::numeric_limits<uint8_t>::max()
               ? value
               : static_cast<uint8_t>(value + 1U);
}

uint16_t growth_limit(const PetCoreState &state,
                      const PetCoreProfile &profile)
{
    switch (state.stage) {
    case PetLifeStage::Hatchling:
        return profile.hatchling_growth_limit;
    case PetLifeStage::Child:
        return profile.child_growth_limit;
    case PetLifeStage::Youth:
        return profile.youth_growth_limit;
    case PetLifeStage::Egg:
    case PetLifeStage::Adult:
    case PetLifeStage::Elder:
    default:
        return state.growth;
    }
}

void update_evolution_ready(PetCoreState &state,
                            const PetCoreProfile &profile)
{
    state.evolution_ready = pet_core_can_evolve(state, profile);
}

void apply_growth(PetCoreState &state,
                  uint16_t amount,
                  const PetCoreProfile &profile,
                  PetCoreActionResult &result)
{
    const uint32_t multiplied =
        static_cast<uint32_t>(amount) * profile.growth_multiplier;
    const uint16_t requested = static_cast<uint16_t>(std::min<uint32_t>(
        multiplied, std::numeric_limits<uint16_t>::max()));
    const uint16_t daily_remaining =
        state.growth_earned_today >= profile.daily_growth_limit
            ? 0U
            : static_cast<uint16_t>(profile.daily_growth_limit -
                                    state.growth_earned_today);
    const uint16_t limit = growth_limit(state, profile);
    const uint16_t remaining = state.growth >= limit ? 0U
                                                      : limit - state.growth;
    const uint16_t awarded =
        std::min(requested, std::min(daily_remaining, remaining));
    state.growth = saturating_add_u16(state.growth, awarded);
    state.growth_earned_today =
        saturating_add_u16(state.growth_earned_today, awarded);
    result.growth_delta = static_cast<int16_t>(awarded);
}

void apply_bond(PetCoreState &state,
                uint8_t amount,
                const PetCoreProfile &profile,
                PetCoreActionResult &result)
{
    const uint16_t multiplied =
        static_cast<uint16_t>(amount) * profile.bond_multiplier;
    const uint8_t daily_remaining =
        state.bond_earned_today >= profile.daily_bond_limit
            ? 0U
            : static_cast<uint8_t>(profile.daily_bond_limit -
                                   state.bond_earned_today);
    const uint8_t awarded = static_cast<uint8_t>(std::min<uint16_t>(
        multiplied, daily_remaining));
    const uint8_t total_remaining =
        state.bond >= 100U ? 0U : static_cast<uint8_t>(100U - state.bond);
    const uint8_t applied = std::min(awarded, total_remaining);
    state.bond = static_cast<uint8_t>(state.bond + applied);
    state.bond_earned_today =
        static_cast<uint8_t>(state.bond_earned_today + applied);
    result.bond_delta = static_cast<int8_t>(applied);
}

void start_new_day(PetCoreState &state, uint32_t day_key)
{
    if (state.current_day_key != 0U && day_key > state.current_day_key) {
        const uint32_t elapsed_days = day_key - state.current_day_key;
        state.day = saturating_add_u16(
            state.day,
            static_cast<uint16_t>(std::min<uint32_t>(
                elapsed_days, std::numeric_limits<uint16_t>::max())));
    }
    state.current_day_key = day_key;
    state.growth_earned_today = 0U;
    state.bond_earned_today = 0U;
    state.feed_count_today = 0U;
    state.pet_count_today = 0U;
    state.play_count_today = 0U;
}

void register_care_day(PetCoreState &state)
{
    const uint32_t day_key = state.current_day_key;
    if (day_key == 0U || day_key == state.last_care_day_key) {
        return;
    }
    if (state.last_care_day_key == 0U ||
        day_key == state.last_care_day_key + 1U) {
        state.care_streak = saturating_add_u16(state.care_streak, 1U);
    } else {
        state.care_streak = 1U;
    }
    state.last_care_day_key = day_key;
    state.best_care_streak =
        std::max(state.best_care_streak, state.care_streak);
}

void advance_awake_minute(PetCoreState &state,
                          const PetCoreProfile &profile,
                          bool offline)
{
    const int food_floor = offline ? 15 : 0;
    state.needs.food = static_cast<uint8_t>(std::max(
        food_floor,
        static_cast<int>(state.needs.food) -
            static_cast<int>(profile.food_decay_per_minute)));

    state.needs.energy = clamp_percent(
        static_cast<int>(state.needs.energy) -
        static_cast<int>(profile.energy_decay_per_minute));

    int joy_loss = profile.joy_decay_per_minute;
    if (state.needs.food < 30U) {
        joy_loss += 2;
    }
    const int joy_floor = offline ? 15 : 0;
    state.needs.joy = static_cast<uint8_t>(std::max(
        joy_floor, static_cast<int>(state.needs.joy) - joy_loss));
}

void advance_sleeping_minute(PetCoreState &state,
                             const PetCoreProfile &profile)
{
    state.needs.energy = clamp_percent(
        static_cast<int>(state.needs.energy) +
        static_cast<int>(profile.sleeping_energy_restore_per_minute));

    if ((state.age_minutes % 2U) == 0U) {
        if (state.needs.food > 30U) {
            --state.needs.food;
        }
        if (state.needs.joy > profile.sleeping_joy_floor) {
            --state.needs.joy;
        }
    }
}

void update_care_mistake(PetCoreState &state)
{
    if (state.mistake_cooldown_minutes > 0U) {
        --state.mistake_cooldown_minutes;
    }
    const uint8_t lowest = std::min(
        state.needs.food,
        std::min(state.needs.joy, state.needs.energy));
    if (lowest <= 10U && state.mistake_cooldown_minutes == 0U) {
        state.care_mistakes = saturating_add_u16(state.care_mistakes, 1U);
        state.mistake_cooldown_minutes = 60U;
        if (state.bond > 1U) {
            --state.bond;
        }
    }
}

}  // namespace

const PetCoreProfile &pet_core_production_profile()
{
    return kProductionProfile;
}

const PetCoreProfile &pet_core_test_profile()
{
    return kTestProfile;
}

void pet_core_apply_environment(PetCoreState &state,
                                const PetEnvironmentSnapshot &snapshot,
                                const PetCoreProfile &profile)
{
    if (!snapshot.rtc_valid || snapshot.rtc_epoch_seconds == 0U) {
        update_evolution_ready(state, profile);
        return;
    }

    // A clock moving backwards cannot describe elapsed pet time.
    // 向后跳变的时钟不能用于计算宠物经过时间。
    if (state.last_rtc_epoch_seconds != 0U &&
        snapshot.rtc_epoch_seconds < state.last_rtc_epoch_seconds) {
        update_evolution_ready(state, profile);
        return;
    }

    const uint32_t day_key = snapshot.rtc_epoch_seconds / kSecondsPerDay;
    if (state.stage == PetLifeStage::Egg) {
        state.current_day_key = day_key;
        state.last_rtc_epoch_seconds = snapshot.rtc_epoch_seconds;
        update_evolution_ready(state, profile);
        return;
    }

    if (state.last_rtc_epoch_seconds != 0U &&
        snapshot.rtc_epoch_seconds > state.last_rtc_epoch_seconds) {
        const uint32_t elapsed_seconds =
            snapshot.rtc_epoch_seconds - state.last_rtc_epoch_seconds;
        pet_core_advance_minutes(
            state,
            elapsed_seconds / kSecondsPerMinute,
            profile,
            snapshot.elapsed_time_is_offline);
    }

    if (state.current_day_key == 0U || day_key > state.current_day_key) {
        start_new_day(state, day_key);
    }
    state.last_rtc_epoch_seconds = snapshot.rtc_epoch_seconds;
    update_evolution_ready(state, profile);
}

PetCoreActionResult pet_core_apply_action(PetCoreState &state,
                                          PetCoreAction action,
                                          const PetCoreProfile &profile)
{
    PetCoreActionResult result = {};
    result.changed = true;
    const bool was_ready = state.evolution_ready;
    register_care_day(state);

    switch (action) {
    case PetCoreAction::Feed:
        result.activity = PetActivity::Eating;
        state.activity = result.activity;
        {
            const uint8_t rewarded_uses = 2U;
            const uint16_t growth = state.feed_count_today == 0U ? 4U : 2U;
            if (state.feed_count_today < rewarded_uses) {
                state.foodie_score = saturating_add_u16(
                    state.foodie_score, 3U);
                apply_growth(state, growth, profile, result);
                apply_bond(state, 1U, profile, result);
            }
        }
        state.feed_count_today =
            saturating_increment_u8(state.feed_count_today);
        state.needs.food = clamp_percent(state.needs.food + 30);
        break;
    case PetCoreAction::Pet:
        result.activity = PetActivity::Petting;
        state.activity = result.activity;
        if (state.pet_count_today < 3U) {
            state.affectionate_score =
                saturating_add_u16(state.affectionate_score, 2U);
            apply_growth(state, 1U, profile, result);
            apply_bond(state, 2U, profile, result);
        }
        state.pet_count_today =
            saturating_increment_u8(state.pet_count_today);
        state.needs.joy = clamp_percent(state.needs.joy + 5);
        break;
    case PetCoreAction::Play:
        result.activity = PetActivity::Playing;
        state.activity = result.activity;
        if (state.play_count_today < 1U) {
            state.active_score = saturating_add_u16(
                state.active_score, 6U);
            apply_growth(state, 3U, profile, result);
            apply_bond(state, 3U, profile, result);
        }
        state.play_count_today =
            saturating_increment_u8(state.play_count_today);
        state.needs.joy = clamp_percent(state.needs.joy + 12);
        state.needs.energy = clamp_percent(state.needs.energy - 8);
        state.needs.food = clamp_percent(state.needs.food - 4);
        break;
    case PetCoreAction::Rest:
        state.activity = state.activity == PetActivity::Sleeping
                             ? PetActivity::Idle
                             : PetActivity::Sleeping;
        result.activity = state.activity;
        break;
    }

    update_evolution_ready(state, profile);
    result.evolution_became_ready = !was_ready && state.evolution_ready;
    return result;
}

void pet_core_advance_day(PetCoreState &state)
{
    state.day = saturating_add_u16(state.day, 1U);
    state.growth_earned_today = 0U;
    state.bond_earned_today = 0U;
    state.feed_count_today = 0U;
    state.pet_count_today = 0U;
    state.play_count_today = 0U;
}

void pet_core_advance_minutes(PetCoreState &state,
                              uint32_t minutes,
                              const PetCoreProfile &profile,
                              bool offline)
{
    const uint32_t bounded_minutes =
        offline ? std::min(minutes, profile.maximum_offline_minutes) : minutes;
    for (uint32_t minute = 0U; minute < bounded_minutes; ++minute) {
        ++state.age_minutes;
        if (state.activity == PetActivity::Sleeping) {
            advance_sleeping_minute(state, profile);
        } else {
            advance_awake_minute(state, profile, offline);
        }
        update_care_mistake(state);
    }
    update_evolution_ready(state, profile);
}

PetMood pet_core_mood(const PetCoreState &state)
{
    if (state.needs.food <= 30U) {
        return PetMood::Hungry;
    }
    if (state.activity != PetActivity::Sleeping && state.needs.energy <= 35U) {
        return PetMood::Tired;
    }
    if (state.needs.joy >= 90U) {
        return PetMood::Ecstatic;
    }
    if (state.needs.joy >= 75U) {
        return PetMood::Happy;
    }
    if (state.needs.joy >= 60U) {
        return PetMood::Content;
    }
    if (state.needs.joy >= 45U) {
        return PetMood::Neutral;
    }
    if (state.needs.joy >= 30U) {
        return PetMood::Sad;
    }
    if (state.needs.joy >= 15U) {
        return PetMood::Upset;
    }
    return PetMood::Miserable;
}

bool pet_core_can_evolve(const PetCoreState &state,
                         const PetCoreProfile &profile)
{
    if (state.stage == PetLifeStage::Egg ||
        state.stage == PetLifeStage::Adult ||
        state.stage == PetLifeStage::Elder) {
        return false;
    }
    if (state.stage == PetLifeStage::Youth &&
        state.branch == PetPersonalityBranch::Undecided) {
        return false;
    }
    const uint8_t lowest = std::min(
        state.needs.food,
        std::min(state.needs.joy, state.needs.energy));
    return state.growth >= growth_limit(state, profile) &&
           lowest >= profile.care_need_floor;
}

bool pet_core_evolve(PetCoreState &state,
                     const PetCoreProfile &profile)
{
    if (!pet_core_can_evolve(state, profile)) {
        return false;
    }
    switch (state.stage) {
    case PetLifeStage::Hatchling:
        state.stage = PetLifeStage::Child;
        break;
    case PetLifeStage::Child:
        if (state.branch == PetPersonalityBranch::Undecided) {
            return false;
        }
        state.stage = PetLifeStage::Youth;
        break;
    case PetLifeStage::Youth:
        state.stage = PetLifeStage::Adult;
        break;
    case PetLifeStage::Egg:
    case PetLifeStage::Adult:
    case PetLifeStage::Elder:
    default:
        return false;
    }
    state.activity = PetActivity::Evolving;
    state.evolution_ready = false;
    return true;
}

PetPersonalityDecision pet_core_personality_decision(
    const PetCoreState &state)
{
    struct Score {
        PetPersonalityBranch branch;
        uint16_t value;
    };
    Score scores[] = {
        {PetPersonalityBranch::Foodie, state.foodie_score},
        {PetPersonalityBranch::Affectionate, state.affectionate_score},
        {PetPersonalityBranch::Active, state.active_score},
    };
    std::sort(std::begin(scores), std::end(scores),
              [](const Score &left, const Score &right) {
                  return left.value > right.value;
              });

    PetPersonalityDecision decision = {};
    if (scores[0].value >=
        static_cast<uint32_t>(scores[1].value) + kAutomaticBranchMargin) {
        decision.automatic_branch = scores[0].branch;
    } else {
        decision.choice_required = true;
    }
    return decision;
}

bool pet_core_choose_personality(PetCoreState &state,
                                 PetPersonalityBranch branch)
{
    if (branch == PetPersonalityBranch::Undecided ||
        state.branch != PetPersonalityBranch::Undecided) {
        return false;
    }
    state.branch = branch;
    return true;
}

void pet_core_sanitize(PetCoreState &state,
                       const PetCoreProfile &profile)
{
    state.version = kPetCoreStateVersion;
    state.needs.food = clamp_percent(state.needs.food);
    state.needs.joy = clamp_percent(state.needs.joy);
    state.needs.energy = clamp_percent(state.needs.energy);
    state.needs.hygiene = 100U;
    state.bond = clamp_percent(state.bond);
    state.waste_count = 0U;
    state.waste_minute_accumulator = 0U;
    if (state.activity == PetActivity::Reserved) {
        state.activity = PetActivity::Idle;
    }
    state.day = std::max<uint16_t>(state.day, 1U);
    if (static_cast<uint8_t>(state.branch) >
        static_cast<uint8_t>(PetPersonalityBranch::Active)) {
        state.branch = PetPersonalityBranch::Undecided;
    }
    update_evolution_ready(state, profile);
}

const char *pet_core_stage_name(PetLifeStage stage)
{
    switch (stage) {
    case PetLifeStage::Egg:
        return "egg";
    case PetLifeStage::Hatchling:
        return "hatchling";
    case PetLifeStage::Child:
        return "child";
    case PetLifeStage::Youth:
        return "youth";
    case PetLifeStage::Adult:
        return "adult";
    case PetLifeStage::Elder:
        return "elder";
    }
    return "unknown";
}

const char *pet_core_activity_name(PetActivity activity)
{
    switch (activity) {
    case PetActivity::Idle:
        return "idle";
    case PetActivity::Eating:
        return "eating";
    case PetActivity::Playing:
        return "playing";
    case PetActivity::Sleeping:
        return "sleeping";
    case PetActivity::Reserved:
        return "idle";
    case PetActivity::Petting:
        return "petting";
    case PetActivity::Evolving:
        return "evolving";
    }
    return "unknown";
}

const char *pet_core_mood_name(PetMood mood)
{
    switch (mood) {
    case PetMood::Ecstatic:
        return "ecstatic";
    case PetMood::Happy:
        return "happy";
    case PetMood::Content:
        return "content";
    case PetMood::Neutral:
        return "neutral";
    case PetMood::Sad:
        return "sad";
    case PetMood::Upset:
        return "upset";
    case PetMood::Miserable:
        return "miserable";
    case PetMood::Hungry:
        return "hungry";
    case PetMood::Tired:
        return "tired";
    }
    return "unknown";
}

const char *pet_core_personality_name(PetPersonalityBranch branch)
{
    switch (branch) {
    case PetPersonalityBranch::Foodie:
        return "foodie";
    case PetPersonalityBranch::Affectionate:
        return "affectionate";
    case PetPersonalityBranch::Active:
        return "active";
    case PetPersonalityBranch::Undecided:
    default:
        return "undecided";
    }
}
