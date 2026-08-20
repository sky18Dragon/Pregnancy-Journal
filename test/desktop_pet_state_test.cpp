#include <cassert>
#include <cstring>

#include "desktop_pet_state.h"

int main()
{
    DesktopPetState state = {};
    assert(state.version == kDesktopPetStateVersion);
    assert(state.version == 5U);
    assert(state.pet.stage == PetLifeStage::Egg);
    assert(state.hatch_taps == 0U);
    assert(state.pet.growth == 10U);
    assert(state.pet.bond == 18U);
    assert(state.pet.day == 1U);
    assert(state.pet.needs.food == 80U);
    assert(std::strcmp(desktop_pet_state_mood_label(state), "HAPPY") == 0);
    assert(!desktop_pet_state_apply(state, DesktopPetAction::Feed).changed);
    desktop_pet_state_advance_day(state);
    assert(state.pet.day == 1U);

    const DesktopPetHatchResult first_tap =
        desktop_pet_state_tap_egg(state);
    assert(first_tap.changed && !first_tap.hatched);
    assert(first_tap.tap_count == 1U);
    assert(state.pet.stage == PetLifeStage::Egg);
    const DesktopPetHatchResult second_tap =
        desktop_pet_state_tap_egg(state);
    assert(second_tap.changed && !second_tap.hatched);
    assert(second_tap.tap_count == 2U);
    const DesktopPetHatchResult third_tap =
        desktop_pet_state_tap_egg(state);
    assert(third_tap.changed && third_tap.hatched);
    assert(third_tap.tap_count == kDesktopPetRequiredHatchTaps);
    assert(state.pet.stage == PetLifeStage::Hatchling);
    assert(!desktop_pet_state_tap_egg(state).changed);

    const DesktopPetActionResult add_growth =
        desktop_pet_state_apply(state, DesktopPetAction::AddGrowth);
    assert(std::strcmp(add_growth.message, "I'M READY TO GROW!") == 0);
    const DesktopPetActionResult add_growth_again =
        desktop_pet_state_apply(state, DesktopPetAction::AddGrowth);
    assert(std::strcmp(add_growth_again.message,
                       "GROWTH LIMIT REACHED.") == 0);
    assert(desktop_pet_state_evolve_if_ready(state) ==
           DesktopPetEvolutionOutcome::Evolved);
    assert(state.pet.stage == PetLifeStage::Child);
    assert(desktop_pet_state_growth_limit(state) ==
           kDesktopPetChildGrowthLimit);
    assert(std::strcmp(desktop_pet_state_stage_label(state), "CHILD") == 0);
    const DesktopPetActionResult child_growth =
        desktop_pet_state_apply(state, DesktopPetAction::AddGrowth);
    assert(child_growth.growth_delta == 30U);
    assert(state.pet.growth == 60U);

    DesktopPetState waiting_to_grow = {};
    waiting_to_grow.pet.stage = PetLifeStage::Hatchling;
    waiting_to_grow.pet.needs.food = 20U;
    desktop_pet_state_apply(waiting_to_grow, DesktopPetAction::AddGrowth);
    assert(desktop_pet_state_evolve_if_ready(waiting_to_grow) ==
           DesktopPetEvolutionOutcome::None);
    desktop_pet_state_apply(waiting_to_grow, DesktopPetAction::Feed);
    assert(desktop_pet_state_evolve_if_ready(waiting_to_grow) ==
           DesktopPetEvolutionOutcome::Evolved);
    assert(waiting_to_grow.pet.stage == PetLifeStage::Child);

    DesktopPetState automatic_youth = {};
    automatic_youth.pet.stage = PetLifeStage::Child;
    automatic_youth.pet.growth = kDesktopPetChildGrowthLimit;
    automatic_youth.pet.needs = {80U, 80U, 80U, 80U};
    automatic_youth.pet.foodie_score = 12U;
    automatic_youth.pet.affectionate_score = 6U;
    automatic_youth.pet.active_score = 3U;
    assert(desktop_pet_state_evolve_if_ready(automatic_youth) ==
           DesktopPetEvolutionOutcome::Evolved);
    assert(automatic_youth.pet.stage == PetLifeStage::Youth);
    assert(automatic_youth.pet.branch == PetPersonalityBranch::Foodie);
    assert(desktop_pet_state_growth_limit(automatic_youth) ==
           kDesktopPetYouthGrowthLimit);
    assert(std::strcmp(desktop_pet_state_stage_label(automatic_youth),
                       "FOODIE YOUTH") == 0);
    automatic_youth.pet.growth = kDesktopPetYouthGrowthLimit;
    assert(desktop_pet_state_evolve_if_ready(automatic_youth) ==
           DesktopPetEvolutionOutcome::Evolved);
    assert(automatic_youth.pet.stage == PetLifeStage::Adult);
    assert(automatic_youth.pet.branch == PetPersonalityBranch::Foodie);
    assert(desktop_pet_state_growth_limit(automatic_youth) ==
           kDesktopPetYouthGrowthLimit);
    assert(std::strcmp(desktop_pet_state_stage_label(automatic_youth),
                       "FOODIE ADULT") == 0);
    assert(desktop_pet_state_evolve_if_ready(automatic_youth) ==
           DesktopPetEvolutionOutcome::None);

    const uint16_t adult_growth = automatic_youth.pet.growth;
    const DesktopPetActionResult adult_feed = desktop_pet_state_apply(
        automatic_youth, DesktopPetAction::Feed);
    assert(adult_feed.changed);
    assert(adult_feed.growth_delta == 0U);
    assert(automatic_youth.pet.growth == adult_growth);
    assert(std::strcmp(adult_feed.message,
                       "I KNOW ALL THE BEST FLAVORS!") == 0);
    const DesktopPetActionResult adult_talk = desktop_pet_state_apply(
        automatic_youth, DesktopPetAction::Talk);
    assert(std::strcmp(adult_talk.message,
                       "I LEARNED A NEW RECIPE FOR US.") == 0);

    DesktopPetState waiting_youth = {};
    waiting_youth.pet.stage = PetLifeStage::Youth;
    waiting_youth.pet.branch = PetPersonalityBranch::Active;
    waiting_youth.pet.growth = kDesktopPetYouthGrowthLimit;
    waiting_youth.pet.needs = {20U, 80U, 80U, 80U};
    assert(desktop_pet_state_evolve_if_ready(waiting_youth) ==
           DesktopPetEvolutionOutcome::None);
    desktop_pet_state_apply(waiting_youth, DesktopPetAction::Feed);
    assert(desktop_pet_state_evolve_if_ready(waiting_youth) ==
           DesktopPetEvolutionOutcome::Evolved);
    assert(waiting_youth.pet.stage == PetLifeStage::Adult);
    assert(std::strcmp(desktop_pet_state_stage_label(waiting_youth),
                       "ACTIVE ADULT") == 0);

    DesktopPetState chosen_youth = {};
    chosen_youth.pet.stage = PetLifeStage::Child;
    chosen_youth.pet.growth = kDesktopPetChildGrowthLimit;
    chosen_youth.pet.needs = {80U, 80U, 80U, 80U};
    chosen_youth.pet.foodie_score = 6U;
    chosen_youth.pet.affectionate_score = 6U;
    assert(desktop_pet_state_evolve_if_ready(chosen_youth) ==
           DesktopPetEvolutionOutcome::ChoiceRequired);
    assert(chosen_youth.pet.stage == PetLifeStage::Child);
    assert(desktop_pet_state_choose_youth_branch(
        chosen_youth, PetPersonalityBranch::Affectionate));
    assert(chosen_youth.pet.stage == PetLifeStage::Youth);
    assert(std::strcmp(desktop_pet_state_stage_label(chosen_youth),
                       "HEART YOUTH") == 0);
    assert(!desktop_pet_state_choose_youth_branch(
        chosen_youth, PetPersonalityBranch::Active));
    state = {};
    state.pet.stage = PetLifeStage::Hatchling;
    state.hatch_taps = kDesktopPetRequiredHatchTaps;

    const DesktopPetActionResult feed =
        desktop_pet_state_apply(state, DesktopPetAction::Feed);
    assert(feed.changed);
    assert(feed.rewarded);
    assert(feed.pose == DesktopPetPose::Feed);
    assert(feed.growth_delta == 20U);
    assert(feed.love_delta == 5U);
    assert(state.pet.growth == kDesktopPetHatchlingGrowthLimit);
    assert(state.pet.bond == 23U);
    assert(state.pet.needs.food == 100U);
    assert(state.pet.foodie_score == 3U);
    assert(std::strcmp(feed.message, "I'M READY TO GROW!") == 0);

    DesktopPetState child_state = {};
    child_state.pet.stage = PetLifeStage::Child;
    child_state.pet.growth = kDesktopPetHatchlingGrowthLimit;
    const DesktopPetActionResult child_feed =
        desktop_pet_state_apply(child_state, DesktopPetAction::Feed);
    assert(std::strcmp(child_feed.message,
                       "ENERGY FOR ADVENTURES!") == 0);

    const DesktopPetActionResult second_feed =
        desktop_pet_state_apply(state, DesktopPetAction::Feed);
    assert(second_feed.rewarded);
    assert(second_feed.growth_delta == 0U);
    assert(second_feed.love_delta == 5U);
    assert(state.pet.foodie_score == 6U);
    assert(std::strcmp(second_feed.message,
                       "YUM! THAT WAS DELICIOUS!") == 0);

    const DesktopPetActionResult third_feed =
        desktop_pet_state_apply(state, DesktopPetAction::Feed);
    assert(!third_feed.rewarded);
    assert(third_feed.growth_delta == 0U);
    assert(third_feed.love_delta == 0U);

    const DesktopPetActionResult pet =
        desktop_pet_state_apply(state, DesktopPetAction::Pet);
    assert(pet.rewarded);
    assert(pet.pose == DesktopPetPose::Pet);
    assert(pet.love_delta == 10U);
    assert(state.pet.affectionate_score == 2U);
    assert(std::strcmp(pet.message, "THAT FEELS SO NICE!") == 0);

    const DesktopPetActionResult play =
        desktop_pet_state_apply(state, DesktopPetAction::Play);
    assert(play.rewarded);
    assert(play.pose == DesktopPetPose::Play);
    assert(state.pet.active_score == 6U);

    DesktopPetState talk_state = {};
    talk_state.pet.stage = PetLifeStage::Hatchling;
    const uint16_t growth_before_talk = talk_state.pet.growth;
    const uint8_t love_before_talk = talk_state.pet.bond;
    const char *talk_lines[6] = {};
    for (size_t index = 0U; index < 6U; ++index) {
        const DesktopPetActionResult talk = desktop_pet_state_apply(
            talk_state, DesktopPetAction::Talk);
        assert(talk.changed);
        assert(!talk.rewarded);
        assert(talk.pose == DesktopPetPose::Idle);
        assert(talk.message != nullptr);
        talk_lines[index] = talk.message;
        for (size_t earlier = 0U; earlier < index; ++earlier) {
            assert(std::strcmp(talk_lines[index], talk_lines[earlier]) != 0);
        }
    }
    assert(talk_state.pet.growth == growth_before_talk);
    assert(talk_state.pet.bond == love_before_talk);

    talk_state.pet.bond = 50U;
    std::memset(talk_state.pet.recent_dialogue_ids,
                0,
                sizeof(talk_state.pet.recent_dialogue_ids));
    const DesktopPetActionResult familiar_talk = desktop_pet_state_apply(
        talk_state, DesktopPetAction::Talk);
    assert(std::strcmp(familiar_talk.message,
                       "I'M GLAD YOU CAME BACK.") == 0 ||
           std::strcmp(familiar_talk.message,
                       "TELL ME ABOUT YOUR DAY.") == 0 ||
           std::strcmp(familiar_talk.message,
                       "SHALL WE HOP SOMEWHERE?") == 0 ||
           std::strcmp(familiar_talk.message,
                       "YOU MAKE THIS ROOM FEEL WARM.") == 0 ||
           std::strcmp(familiar_talk.message,
                       "I SAVED THIS SPOT FOR YOU.") == 0 ||
           std::strcmp(familiar_talk.message,
                       "LET'S SPEND MORE TIME TOGETHER.") == 0);

    talk_state.pet.bond = 80U;
    std::memset(talk_state.pet.recent_dialogue_ids,
                0,
                sizeof(talk_state.pet.recent_dialogue_ids));
    const DesktopPetActionResult close_talk = desktop_pet_state_apply(
        talk_state, DesktopPetAction::Talk);
    assert(std::strcmp(close_talk.message,
                       "YOU'RE MY FAVORITE PERSON.") == 0 ||
           std::strcmp(close_talk.message,
                       "I TRUST YOU WITH MY CARROTS.") == 0 ||
           std::strcmp(close_talk.message,
                       "EVERY DAY IS BETTER WITH YOU.") == 0 ||
           std::strcmp(close_talk.message,
                       "I KNEW YOU WOULD COME BACK.") == 0 ||
           std::strcmp(close_talk.message,
                       "LET'S STAY TOGETHER A WHILE.") == 0 ||
           std::strcmp(close_talk.message,
                       "HOME IS WHERE YOU ARE.") == 0);

    desktop_pet_state_advance_day(state);
    assert(state.pet.day == 2U);
    assert(state.pet.needs.food == 76U);
    assert(state.pet.feed_count_today == 0U);
    assert(state.pet.pet_count_today == 0U);
    assert(state.pet.play_count_today == 0U);
    assert(state.pet.growth_earned_today == 0U);
    assert(state.pet.bond_earned_today == 0U);

    desktop_pet_state_advance_day(state);
    desktop_pet_state_advance_day(state);
    assert(state.pet.needs.food == 36U);
    desktop_pet_state_advance_day(state);
    assert(state.pet.needs.food == 16U);
    assert(std::strcmp(desktop_pet_state_mood_label(state), "HUNGRY") == 0);
    desktop_pet_state_apply(state, DesktopPetAction::Feed);
    assert(state.pet.needs.food == 46U);
    assert(std::strcmp(desktop_pet_state_mood_label(state), "HUNGRY") != 0);

    const DesktopPetActionResult reset =
        desktop_pet_state_apply(state, DesktopPetAction::Reset);
    assert(reset.changed);
    assert(state.pet.growth == 10U);
    assert(state.pet.bond == 18U);
    assert(state.pet.day == 1U);
    assert(state.pet.needs.food == 80U);
    assert(state.pet.stage == PetLifeStage::Egg);
    assert(state.hatch_taps == 0U);
    return 0;
}
