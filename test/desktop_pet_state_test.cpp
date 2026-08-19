#include <cassert>
#include <cstring>

#include "desktop_pet_state.h"

int main()
{
    DesktopPetState state = {};
    assert(state.version == kDesktopPetStateVersion);
    assert(state.version == 3U);
    assert(state.pet.growth == 10U);
    assert(state.pet.bond == 18U);
    assert(state.pet.day == 1U);
    assert(state.pet.needs.food == 80U);
    assert(std::strcmp(desktop_pet_state_mood_label(state), "HAPPY") == 0);

    const DesktopPetActionResult add_growth =
        desktop_pet_state_apply(state, DesktopPetAction::AddGrowth);
    assert(std::strcmp(add_growth.message, "I'M READY TO GROW!") == 0);
    const DesktopPetActionResult add_growth_again =
        desktop_pet_state_apply(state, DesktopPetAction::AddGrowth);
    assert(std::strcmp(add_growth_again.message,
                       "GROWTH LIMIT REACHED.") == 0);
    state = {};

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
    return 0;
}
