#include "desktop_pet_state.h"

#include <algorithm>

namespace {

#if STICKY_DESKTOP_PET_TEST_MODE
constexpr uint16_t kGrowthMultiplier = 10U;
constexpr uint8_t kLoveMultiplier = 5U;
constexpr uint16_t kDailyGrowthCap = 100U;
constexpr uint8_t kDailyLoveCap = 40U;
#else
constexpr uint16_t kGrowthMultiplier = 1U;
constexpr uint8_t kLoveMultiplier = 1U;
constexpr uint16_t kDailyGrowthCap = 10U;
constexpr uint8_t kDailyLoveCap = 8U;
#endif

uint16_t award_growth(DesktopPetState &state, uint16_t base_amount)
{
    const uint16_t requested = base_amount * kGrowthMultiplier;
    const uint16_t daily_remaining =
        state.growth_earned_today >= kDailyGrowthCap
            ? 0U
            : kDailyGrowthCap - state.growth_earned_today;

    // TODO(stage-progression): Continue beyond this boundary when the Child
    // page and evolution transition are implemented.
    // TODO（成长阶段）：儿童期页面和进化转场完成后，从该边界继续增长。
    const uint16_t stage_remaining =
        state.growth >= kDesktopPetHatchlingGrowthLimit
            ? 0U
            : kDesktopPetHatchlingGrowthLimit - state.growth;
    const uint16_t awarded =
        std::min(requested, std::min(daily_remaining, stage_remaining));
    state.growth = static_cast<uint16_t>(state.growth + awarded);
    state.growth_earned_today =
        static_cast<uint16_t>(state.growth_earned_today + awarded);
    return awarded;
}

uint8_t award_love(DesktopPetState &state, uint8_t base_amount)
{
    const uint16_t requested =
        static_cast<uint16_t>(base_amount) * kLoveMultiplier;
    const uint16_t daily_remaining =
        state.love_earned_today >= kDailyLoveCap
            ? 0U
            : kDailyLoveCap - state.love_earned_today;
    const uint16_t total_remaining =
        state.love >= 100U ? 0U : 100U - state.love;
    const uint8_t awarded = static_cast<uint8_t>(
        std::min(requested, std::min(daily_remaining, total_remaining)));
    state.love = static_cast<uint8_t>(state.love + awarded);
    state.love_earned_today =
        static_cast<uint8_t>(state.love_earned_today + awarded);
    return awarded;
}

DesktopPetActionResult care_result(DesktopPetState &state,
                                   DesktopPetPose pose,
                                   uint16_t growth,
                                   uint8_t love,
                                   const char *rewarded_message,
                                   const char *companion_message)
{
    DesktopPetActionResult result = {};
    result.changed = true;
    result.pose = pose;
    result.growth_delta = award_growth(state, growth);
    result.love_delta = award_love(state, love);
    result.rewarded = result.growth_delta > 0U || result.love_delta > 0U;
    result.message = result.rewarded ? rewarded_message : companion_message;
    if (state.growth >= kDesktopPetHatchlingGrowthLimit) {
        result.message = "I'M READY TO GROW!";
    }
    return result;
}

}  // namespace

DesktopPetActionResult desktop_pet_state_apply(DesktopPetState &state,
                                               DesktopPetAction action)
{
    switch (action) {
    case DesktopPetAction::Feed: {
        const bool rewarded_use = state.feed_count_today < 2U;
        const uint16_t growth = state.feed_count_today == 0U ? 4U : 2U;
        ++state.feed_count_today;
        if (!rewarded_use) {
            return care_result(
                state, DesktopPetPose::Feed, 0U, 0U,
                "THANK YOU FOR THE SNACK!", "I'M FULL, BUT THANK YOU!");
        }
        state.foodie_score = static_cast<uint16_t>(state.foodie_score + 3U);
        return care_result(
            state, DesktopPetPose::Feed, growth, 1U,
            "YUM! THAT WAS DELICIOUS!", "I'M FULL, BUT THANK YOU!");
    }
    case DesktopPetAction::Pet: {
        const bool rewarded_use = state.pet_count_today < 3U;
        ++state.pet_count_today;
        if (!rewarded_use) {
            return care_result(
                state, DesktopPetPose::Pet, 0U, 0U,
                "THAT FEELS SO NICE!", "I LOVE YOUR GENTLE PATS.");
        }
        state.affectionate_score =
            static_cast<uint16_t>(state.affectionate_score + 2U);
        return care_result(
            state, DesktopPetPose::Pet, 1U, 2U,
            "THAT FEELS SO NICE!", "I LOVE YOUR GENTLE PATS.");
    }
    case DesktopPetAction::Play: {
        const bool rewarded_use = state.play_count_today < 1U;
        ++state.play_count_today;
        if (!rewarded_use) {
            return care_result(
                state, DesktopPetPose::Play, 0U, 0U,
                "LET'S CHASE IT!", "ONE MORE LITTLE HOP!");
        }
        state.active_score = static_cast<uint16_t>(state.active_score + 6U);
        return care_result(
            state, DesktopPetPose::Play, 3U, 3U,
            "LET'S CHASE IT!", "ONE MORE LITTLE HOP!");
    }
    case DesktopPetAction::NextDay:
        desktop_pet_state_advance_day(state);
        return {true, false, 0U, 0U, DesktopPetPose::Idle,
                "A NEW DAY TOGETHER!"};
    case DesktopPetAction::AddGrowth: {
        DesktopPetActionResult result = {};
        result.changed = true;
        result.growth_delta =
            static_cast<uint16_t>(std::min<uint16_t>(
                30U, kDesktopPetHatchlingGrowthLimit - state.growth));
        state.growth = static_cast<uint16_t>(state.growth + result.growth_delta);
        result.message = state.growth >= kDesktopPetHatchlingGrowthLimit
                             ? "I'M READY TO GROW!"
                             : "I FEEL A LITTLE BIGGER!";
        return result;
    }
    case DesktopPetAction::AddLove: {
        DesktopPetActionResult result = {};
        result.changed = true;
        result.love_delta = static_cast<uint8_t>(
            std::min<uint16_t>(20U, 100U - state.love));
        state.love = static_cast<uint8_t>(state.love + result.love_delta);
        result.message = "I FEEL SO LOVED!";
        return result;
    }
    case DesktopPetAction::Reset:
        state = {};
        return {true, false, 0U, 0U, DesktopPetPose::Idle,
                "LET'S SPEND TODAY TOGETHER."};
    case DesktopPetAction::OpenTest:
    case DesktopPetAction::CloseTest:
    case DesktopPetAction::None:
        return {};
    }
    return {};
}

void desktop_pet_state_advance_day(DesktopPetState &state)
{
    ++state.day;
    state.growth_earned_today = 0U;
    state.love_earned_today = 0U;
    state.feed_count_today = 0U;
    state.pet_count_today = 0U;
    state.play_count_today = 0U;
}

const char *desktop_pet_action_name(DesktopPetAction action)
{
    switch (action) {
    case DesktopPetAction::Feed:
        return "feed";
    case DesktopPetAction::Pet:
        return "pet";
    case DesktopPetAction::Play:
        return "play";
    case DesktopPetAction::OpenTest:
        return "open_test";
    case DesktopPetAction::CloseTest:
        return "close_test";
    case DesktopPetAction::NextDay:
        return "next_day";
    case DesktopPetAction::AddGrowth:
        return "add_growth";
    case DesktopPetAction::AddLove:
        return "add_love";
    case DesktopPetAction::Reset:
        return "reset";
    case DesktopPetAction::None:
    default:
        return "none";
    }
}

const char *desktop_pet_pose_name(DesktopPetPose pose)
{
    switch (pose) {
    case DesktopPetPose::Feed:
        return "feed";
    case DesktopPetPose::Pet:
        return "pet";
    case DesktopPetPose::Play:
        return "play";
    case DesktopPetPose::Idle:
    default:
        return "idle";
    }
}
