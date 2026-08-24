#include <cassert>
#include <cstdint>
#include <cstring>

#include "pet_animation_queue.h"
#include "pet_core.h"
#include "pet_dialogue.h"
#include "pet_idle_scheduler.h"
#include "pet_rtc_time.h"
#include "pet_save_record.h"

namespace {

int s_started = 0;
int s_completed = 0;

void on_start()
{
    ++s_started;
}

void on_complete()
{
    ++s_completed;
}

}  // namespace

int main()
{
    const PetCoreProfile &profile = pet_core_production_profile();
    PetCoreState state = {};

    PetRtcDateTime rtc = {2024U, 2U, 29U, 12U, 30U, 0U};
    uint32_t rtc_epoch = 0U;
    assert(pet_rtc_time_valid(rtc));
    assert(pet_rtc_time_to_epoch(rtc, rtc_epoch));
    PetRtcDateTime restored_rtc = {};
    assert(pet_rtc_time_from_epoch(rtc_epoch, restored_rtc));
    assert(restored_rtc.year == 2024U);
    assert(restored_rtc.month == 2U);
    assert(restored_rtc.day == 29U);
    assert(restored_rtc.hour == 12U);
    assert(restored_rtc.minute == 30U);
    assert(restored_rtc.second == 0U);
    rtc.day = 30U;
    assert(!pet_rtc_time_valid(rtc));

    // The PCF8563 snapshot establishes the calendar baseline.
    // PCF8563快照建立日历基线。
    PetEnvironmentSnapshot first_time = {};
    first_time.rtc_valid = true;
    first_time.rtc_epoch_seconds = 10U * 86400U;
    pet_core_apply_environment(state, first_time, profile);
    assert(state.current_day_key == 10U);
    assert(state.day == 1U);

    PetEnvironmentSnapshot later = first_time;
    later.rtc_epoch_seconds += 120U;
    pet_core_apply_environment(state, later, profile);
    assert(state.age_minutes == 2U);
    assert(state.needs.food == 76U);
    assert(state.needs.energy == 78U);

    const uint8_t food_before = state.needs.food;
    const PetCoreActionResult feed =
        pet_core_apply_action(state, PetCoreAction::Feed, profile);
    assert(feed.changed);
    assert(feed.activity == PetActivity::Eating);
    assert(state.needs.food > food_before);
    assert(state.needs.food == 100U);
    assert(feed.growth_delta == 4);
    assert(state.care_streak == 1U);

    // An explicit accelerated-day key advances the same streak rules.
    // 明确的加速日期编号沿用相同的连续照料规则。
    PetCoreState accelerated_streak = {};
    pet_core_apply_action_for_day(
        accelerated_streak, PetCoreAction::Pet, profile, 100U);
    assert(accelerated_streak.care_streak == 1U);
    pet_core_apply_action_for_day(
        accelerated_streak, PetCoreAction::Feed, profile, 100U);
    assert(accelerated_streak.care_streak == 1U);
    pet_core_apply_action_for_day(
        accelerated_streak, PetCoreAction::Play, profile, 101U);
    assert(accelerated_streak.care_streak == 2U);
    pet_core_apply_action_for_day(
        accelerated_streak, PetCoreAction::Pet, profile, 103U);
    assert(accelerated_streak.care_streak == 1U);

    state.needs.joy = 50U;
    const PetCoreActionResult pet =
        pet_core_apply_action(state, PetCoreAction::Pet, profile);
    assert(pet.activity == PetActivity::Petting);
    assert(state.needs.joy == 55U);
    assert(state.affectionate_score == 2U);

    const PetCoreActionResult rest =
        pet_core_apply_action(state, PetCoreAction::Rest, profile);
    assert(rest.activity == PetActivity::Sleeping);
    const uint8_t energy_before_sleep = state.needs.energy;
    pet_core_advance_minutes(state, 1U, profile, false);
    assert(state.needs.energy > energy_before_sleep);

    PetEnvironmentSnapshot next_day = later;
    next_day.rtc_epoch_seconds = 11U * 86400U;
    pet_core_apply_environment(state, next_day, profile);
    assert(state.day == 2U);
    assert(state.feed_count_today == 0U);

    PetCoreState long_offline = {};
    long_offline.last_rtc_epoch_seconds = 100U;
    long_offline.current_day_key = 1U;
    PetEnvironmentSnapshot much_later = {};
    much_later.rtc_valid = true;
    much_later.elapsed_time_is_offline = true;
    much_later.rtc_epoch_seconds =
        100U + (profile.maximum_offline_minutes + 5000U) * 60U;
    pet_core_apply_environment(long_offline, much_later, profile);
    assert(long_offline.age_minutes == profile.maximum_offline_minutes);
    assert(long_offline.needs.food >= 15U);

    // An unhatched egg records the RTC baseline without consuming care needs.
    // 未孵化的蛋只记录RTC基线，不消耗任何照料值。
    PetCoreState egg = {};
    egg.stage = PetLifeStage::Egg;
    const PetNeeds egg_needs = egg.needs;
    pet_core_apply_environment(egg, first_time, profile);
    assert(egg.last_rtc_epoch_seconds == first_time.rtc_epoch_seconds);
    assert(egg.current_day_key == 10U);
    assert(egg.needs.food == egg_needs.food);
    assert(egg.needs.joy == egg_needs.joy);
    assert(egg.needs.energy == egg_needs.energy);

    // A backward RTC correction preserves the last trusted timestamp.
    // RTC向后校正时保留上一次可信时间戳。
    PetCoreState backward_clock = {};
    backward_clock.last_rtc_epoch_seconds = 20U * 86400U;
    backward_clock.current_day_key = 20U;
    PetEnvironmentSnapshot earlier_time = {};
    earlier_time.rtc_valid = true;
    earlier_time.rtc_epoch_seconds = 19U * 86400U;
    pet_core_apply_environment(backward_clock, earlier_time, profile);
    assert(backward_clock.last_rtc_epoch_seconds == 20U * 86400U);
    assert(backward_clock.current_day_key == 20U);

    PetCoreState ready = {};
    ready.growth = profile.hatchling_growth_limit;
    ready.needs = {80U, 80U, 80U, 80U};
    assert(pet_core_can_evolve(ready, profile));
    assert(pet_core_evolve(ready, profile));
    assert(ready.stage == PetLifeStage::Child);

    // Evolution depends only on the three care needs visible to the user.
    // 成长只取决于用户可见的食物、心情和精力三项照料值。
    PetCoreState legacy_dirty_state = {};
    legacy_dirty_state.growth = profile.hatchling_growth_limit;
    legacy_dirty_state.needs.food = 80U;
    legacy_dirty_state.needs.joy = 80U;
    legacy_dirty_state.needs.energy = 80U;
    legacy_dirty_state.needs.hygiene = 0U;
    legacy_dirty_state.waste_count = 3U;
    assert(pet_core_can_evolve(legacy_dirty_state, profile));
    assert(pet_core_mood(legacy_dirty_state) == PetMood::Happy);
    pet_core_advance_minutes(legacy_dirty_state, 1U, profile, false);
    assert(legacy_dirty_state.needs.hygiene == 0U);
    assert(legacy_dirty_state.care_mistakes == 0U);
    assert(pet_core_can_evolve(legacy_dirty_state, profile));
    legacy_dirty_state.activity = PetActivity::Reserved;
    pet_core_sanitize(legacy_dirty_state, profile);
    assert(legacy_dirty_state.activity == PetActivity::Idle);
    assert(legacy_dirty_state.needs.hygiene == 100U);
    assert(legacy_dirty_state.waste_count == 0U);

    PetCoreState automatic_path = {};
    automatic_path.stage = PetLifeStage::Child;
    automatic_path.growth = profile.child_growth_limit;
    automatic_path.needs = {80U, 80U, 80U, 80U};
    automatic_path.foodie_score = 12U;
    automatic_path.affectionate_score = 6U;
    automatic_path.active_score = 3U;
    const PetPersonalityDecision automatic_decision =
        pet_core_personality_decision(automatic_path);
    assert(!automatic_decision.choice_required);
    assert(automatic_decision.automatic_branch ==
           PetPersonalityBranch::Foodie);
    assert(pet_core_choose_personality(
        automatic_path, automatic_decision.automatic_branch));
    assert(pet_core_evolve(automatic_path, profile));
    assert(automatic_path.stage == PetLifeStage::Youth);
    automatic_path.growth = profile.youth_growth_limit;
    assert(pet_core_can_evolve(automatic_path, profile));
    assert(pet_core_evolve(automatic_path, profile));
    assert(automatic_path.stage == PetLifeStage::Adult);
    assert(automatic_path.branch == PetPersonalityBranch::Foodie);
    assert(!pet_core_can_evolve(automatic_path, profile));
    assert(!pet_core_evolve(automatic_path, profile));

    PetCoreState youth_without_path = {};
    youth_without_path.stage = PetLifeStage::Youth;
    youth_without_path.growth = profile.youth_growth_limit;
    youth_without_path.needs = {80U, 80U, 80U, 80U};
    assert(!pet_core_can_evolve(youth_without_path, profile));

    PetCoreState close_paths = {};
    close_paths.stage = PetLifeStage::Child;
    close_paths.foodie_score = 6U;
    close_paths.affectionate_score = 6U;
    const PetPersonalityDecision close_decision =
        pet_core_personality_decision(close_paths);
    assert(close_decision.choice_required);
    assert(close_decision.automatic_branch ==
           PetPersonalityBranch::Undecided);
    assert(pet_core_choose_personality(
        close_paths, PetPersonalityBranch::Active));
    assert(!pet_core_choose_personality(
        close_paths, PetPersonalityBranch::Foodie));

    PetCoreState hungry = {};
    hungry.needs.food = 20U;
    assert(pet_core_mood(hungry) == PetMood::Hungry);
    assert(pet_dialogue_context_for_state(hungry) ==
           PetDialogueContext::Hungry);
    const PetDialogueEntry *first_line =
        pet_dialogue_pick(hungry, PetDialogueContext::Hungry, 0U);
    const PetDialogueEntry *second_line =
        pet_dialogue_pick(hungry, PetDialogueContext::Hungry, 0U);
    assert(first_line != nullptr);
    assert(second_line != nullptr);
    assert(first_line->id != second_line->id);
    assert(pet_dialogue_text_bytes() < 4096U);

    PetCoreState talking = {};
    const PetDialogueEntry *talk_lines[6] = {};
    for (uint32_t index = 0U; index < 6U; ++index) {
        talk_lines[index] = pet_dialogue_pick(
            talking, PetDialogueContext::Talk, index);
        assert(talk_lines[index] != nullptr);
        assert(talk_lines[index]->minimum_bond == 0U);
        assert(talk_lines[index]->maximum_bond == 34U);
        for (uint32_t earlier = 0U; earlier < index; ++earlier) {
            assert(talk_lines[index]->id != talk_lines[earlier]->id);
        }
    }
    talking.bond = 70U;
    std::memset(talking.recent_dialogue_ids,
                0,
                sizeof(talking.recent_dialogue_ids));
    const PetDialogueEntry *close_talk = pet_dialogue_pick(
        talking, PetDialogueContext::Talk, 0U);
    assert(close_talk != nullptr);
    assert(close_talk->minimum_bond == 70U);
    assert(close_talk->maximum_bond == 100U);

    PetCoreState idle_pet = {};
    PetIdleAction previous_idle = PetIdleAction::None;
    PetIdleAction second_previous_idle = PetIdleAction::None;
    for (uint32_t index = 0U; index < 12U; ++index) {
        const PetIdleAction selected = pet_idle_select(
            idle_pet, previous_idle, second_previous_idle, index);
        assert(selected != PetIdleAction::None);
        assert(selected != previous_idle);
        assert(selected != second_previous_idle);
        second_previous_idle = previous_idle;
        previous_idle = selected;
    }
    idle_pet.needs.food = 20U;
    const PetIdleAction hungry_idle = pet_idle_select(
        idle_pet, PetIdleAction::Blink, PetIdleAction::EarTwitch, 1U);
    assert(hungry_idle == PetIdleAction::Hungry);
    idle_pet.needs.food = 80U;
    idle_pet.needs.energy = 20U;
    const PetIdleAction tired_idle = pet_idle_select(
        idle_pet, PetIdleAction::Blink, PetIdleAction::EarTwitch, 1U);
    assert(tired_idle == PetIdleAction::Tired);
    assert(pet_idle_next_delay_ms(0U, true) == 4000U);
    assert(pet_idle_next_delay_ms(0U, false) == 12000U);
    assert(std::strcmp(
               pet_idle_message(PetIdleAction::Stretch,
                                PetLifeStage::Child),
               "LOOK HOW TALL I AM!") == 0);
    assert(std::strcmp(
               pet_idle_message(PetIdleAction::Stretch,
                                PetLifeStage::Adult),
               "ALL GROWN AND READY FOR TODAY!") == 0);

    PetAnimationQueue queue;
    queue.reset();
    PetAnimationNode pose = {};
    pose.duration_ms = 100U;
    pose.frame_count = 4U;
    pose.frame_delay_ms = 25U;
    pose.on_start = on_start;
    pose.on_complete = on_complete;
    assert(queue.enqueue(pose, 1000U));
    assert(queue.playing());
    assert(queue.current_frame(1075U) == 3U);

    PetAnimationNode wait = {};
    wait.type = PetAnimationNodeType::WaitInput;
    assert(queue.enqueue(wait, 1000U));
    queue.update(1100U);
    assert(queue.waiting_for_input());
    assert(s_started == 1);
    assert(s_completed == 1);
    queue.resume(1200U);
    assert(queue.empty());

    queue.reset();
    PetAnimationNode delay = {};
    delay.type = PetAnimationNodeType::Delay;
    delay.duration_ms = 1000U;
    for (uint8_t index = 0U; index < kPetAnimationQueueCapacity; ++index) {
        assert(queue.enqueue(delay, 2000U));
    }
    assert(!queue.enqueue(delay, 2000U));

    const PetSaveRecord first_save =
        pet_save_make_record(state, 1U, later.rtc_epoch_seconds);
    PetSaveRecord second_save =
        pet_save_make_record(state, 2U, next_day.rtc_epoch_seconds);
    assert(pet_save_validate(first_save));
    assert(pet_save_validate(second_save));
    const PetSaveRecord saves[] = {first_save, second_save};
    assert(pet_save_select_newest(saves, 2U) == 1);
    assert(pet_save_select_write_slot(saves, 2U) == 0U);
    second_save.state.bond = 99U;
    assert(!pet_save_validate(second_save));

    const uint8_t generic_payload[] = {1U, 2U, 3U, 4U};
    const PetSaveHeader generic_header = pet_save_make_header(
        generic_payload, sizeof(generic_payload), 7U, 15U, 100U);
    assert(pet_save_validate_payload(
        generic_header, generic_payload, sizeof(generic_payload), 7U));
    uint8_t corrupted_payload[] = {1U, 2U, 3U, 5U};
    assert(!pet_save_validate_payload(
        generic_header,
        corrupted_payload,
        sizeof(corrupted_payload),
        7U));
    assert(pet_save_sequence_newer(0U, UINT32_MAX));

    assert(sizeof(PetCoreState) <= 128U);
    assert(sizeof(PetAnimationQueue) <= 768U);

    PetCoreState accelerated = {};
    const PetCoreActionResult accelerated_feed = pet_core_apply_action(
        accelerated, PetCoreAction::Feed, pet_core_test_profile());
    assert(accelerated_feed.growth_delta == 20);
    assert(accelerated_feed.bond_delta == 5);
    assert(accelerated.growth == 30U);
    assert(accelerated.bond == 23U);

    accelerated.feed_count_today = UINT8_MAX;
    const uint16_t growth_at_limit = accelerated.growth;
    pet_core_apply_action(
        accelerated, PetCoreAction::Feed, pet_core_test_profile());
    assert(accelerated.feed_count_today == UINT8_MAX);
    assert(accelerated.growth == growth_at_limit);
    return 0;
}
