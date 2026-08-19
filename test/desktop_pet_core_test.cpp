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

    PetCoreState ready = {};
    ready.growth = profile.hatchling_growth_limit;
    ready.needs = {80U, 80U, 80U, 80U};
    assert(pet_core_can_evolve(ready, profile));
    assert(pet_core_evolve(ready, profile));
    assert(ready.stage == PetLifeStage::Child);

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
