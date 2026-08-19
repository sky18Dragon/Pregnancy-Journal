#include <cassert>
#include <cstring>

#include "desktop_pet_state.h"

int main()
{
    DesktopPetState state = {};
    assert(state.growth == 10U);
    assert(state.love == 18U);
    assert(state.day == 1U);

    const DesktopPetActionResult feed =
        desktop_pet_state_apply(state, DesktopPetAction::Feed);
    assert(feed.changed);
    assert(feed.rewarded);
    assert(feed.pose == DesktopPetPose::Feed);
    assert(feed.growth_delta == 20U);
    assert(feed.love_delta == 5U);
    assert(state.growth == kDesktopPetHatchlingGrowthLimit);
    assert(state.love == 23U);
    assert(state.foodie_score == 3U);
    assert(std::strcmp(feed.message, "I'M READY TO GROW!") == 0);

    const DesktopPetActionResult second_feed =
        desktop_pet_state_apply(state, DesktopPetAction::Feed);
    assert(second_feed.rewarded);
    assert(second_feed.growth_delta == 0U);
    assert(second_feed.love_delta == 5U);
    assert(state.foodie_score == 6U);

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
    assert(state.affectionate_score == 2U);

    const DesktopPetActionResult play =
        desktop_pet_state_apply(state, DesktopPetAction::Play);
    assert(play.rewarded);
    assert(play.pose == DesktopPetPose::Play);
    assert(state.active_score == 6U);

    desktop_pet_state_advance_day(state);
    assert(state.day == 2U);
    assert(state.feed_count_today == 0U);
    assert(state.pet_count_today == 0U);
    assert(state.play_count_today == 0U);
    assert(state.growth_earned_today == 0U);
    assert(state.love_earned_today == 0U);

    const DesktopPetActionResult reset =
        desktop_pet_state_apply(state, DesktopPetAction::Reset);
    assert(reset.changed);
    assert(state.growth == 10U);
    assert(state.love == 18U);
    assert(state.day == 1U);
    return 0;
}

