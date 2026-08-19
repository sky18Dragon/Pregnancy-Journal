#pragma once

#include <cstdint>

#ifndef STICKY_DESKTOP_PET_TEST_MODE
#define STICKY_DESKTOP_PET_TEST_MODE 1
#endif

enum class DesktopPetAction : uint8_t {
    None,
    Feed,
    Pet,
    Play,
    OpenTest,
    CloseTest,
    NextDay,
    AddGrowth,
    AddLove,
    Reset,
};

enum class DesktopPetPose : uint8_t {
    Idle,
    Feed,
    Pet,
    Play,
};

constexpr uint32_t kDesktopPetStateVersion = 2U;

struct DesktopPetState {
    uint32_t version = kDesktopPetStateVersion;
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

struct DesktopPetActionResult {
    bool changed = false;
    bool rewarded = false;
    uint16_t growth_delta = 0U;
    uint8_t love_delta = 0U;
    DesktopPetPose pose = DesktopPetPose::Idle;
    const char *message = "LET'S SPEND TODAY TOGETHER.";
};

constexpr uint16_t kDesktopPetHatchlingGrowthLimit = 30U;
constexpr uint32_t kDesktopPetTestDayLengthMs = 120000U;

// Applies one care or test action to the persistent pet state.
// 将一次照料或测试操作应用到可持久化的桌宠状态。
DesktopPetActionResult desktop_pet_state_apply(DesktopPetState &state,
                                               DesktopPetAction action);

// Starts a new simulated day and resets only the daily reward counters.
// 开始新的模拟日期，并只重置当天奖励计数。
void desktop_pet_state_advance_day(DesktopPetState &state);

const char *desktop_pet_action_name(DesktopPetAction action);
const char *desktop_pet_pose_name(DesktopPetPose pose);
