#pragma once

#include <cstdint>

enum class DesktopPetOutingPhase : uint8_t {
    Home,
    Packing,
    Leaving,
    Away,
    Returning,
    Reunion,
};

struct DesktopPetOutingSession {
    DesktopPetOutingPhase phase = DesktopPetOutingPhase::Home;
    uint32_t away_duration_ms = 0U;
    uint32_t phase_deadline_ms = 0U;
};

struct DesktopPetOutingPlan {
    uint32_t decision_day_key = 0U;
    uint32_t departure_epoch_seconds = 0U;
    uint32_t return_epoch_seconds = 0U;
};

enum class DesktopPetOutingPlanStatus : uint8_t {
    Unplanned,
    StayingHome,
    Scheduled,
    Away,
    Completed,
};

constexpr uint32_t kDesktopPetOutingTestMinimumMs = 20000U;
constexpr uint32_t kDesktopPetOutingTestMaximumMs = 40000U;
constexpr uint32_t kDesktopPetOutingProductionMinimumMs = 3600000U;
constexpr uint32_t kDesktopPetOutingProductionMaximumMs = 25200000U;
constexpr uint32_t kDesktopPetOutingProductionChanceNumerator = 2U;
constexpr uint32_t kDesktopPetOutingProductionChanceDenominator = 5U;

// Selects one inclusive outing duration from the active balance profile.
// 根据当前平衡配置，在包含边界的范围内选择一次外出时长。
uint32_t desktop_pet_outing_duration_ms(uint32_t random_value,
                                        bool accelerated);

// Starts one visual outing sequence and records its selected away duration.
// 开始一次外出视觉流程，并记录本次选中的外出时长。
bool desktop_pet_outing_start(DesktopPetOutingSession &session,
                              uint32_t now_ms,
                              uint32_t random_value,
                              bool accelerated);

// Starts an outing whose duration was already selected by the RTC plan.
// 使用RTC日程中已经确定的时长开始一次外出。
bool desktop_pet_outing_start_for_duration(
    DesktopPetOutingSession &session,
    uint32_t now_ms,
    uint32_t away_duration_ms);

// Restores the away page with the remaining duration after a restart.
// 设备重启后，按照剩余时长恢复正在外出的页面。
bool desktop_pet_outing_resume_away(DesktopPetOutingSession &session,
                                    uint32_t now_ms,
                                    uint32_t remaining_duration_ms);

// Advances one due outing phase without blocking touch processing.
// 在不阻塞触摸处理的情况下推进一个到期的外出阶段。
bool desktop_pet_outing_update(DesktopPetOutingSession &session,
                               uint32_t now_ms);

// Moves an away rabbit into the returning phase immediately.
// 让正在外出的兔子立即进入回家阶段。
bool desktop_pet_outing_call_home(DesktopPetOutingSession &session,
                                  uint32_t now_ms);

bool desktop_pet_outing_active(const DesktopPetOutingSession &session);
const char *desktop_pet_outing_phase_name(DesktopPetOutingPhase phase);

// Makes one persistent outing decision for a new RTC calendar day.
// 为新的RTC自然日生成并保存一次外出决定。
bool desktop_pet_outing_plan_day(DesktopPetOutingPlan &plan,
                                 uint32_t now_epoch_seconds,
                                 uint32_t decision_random_value,
                                 uint32_t departure_random_value,
                                 uint32_t duration_random_value,
                                 bool accelerated);

DesktopPetOutingPlanStatus desktop_pet_outing_plan_status(
    const DesktopPetOutingPlan &plan,
    uint32_t now_epoch_seconds);

uint32_t desktop_pet_outing_plan_remaining_seconds(
    const DesktopPetOutingPlan &plan,
    uint32_t now_epoch_seconds);

// Marks today's trip complete while retaining the once-per-day decision.
// 标记今天的外出已经完成，同时保留每天只决定一次的记录。
void desktop_pet_outing_complete_plan(DesktopPetOutingPlan &plan);

void desktop_pet_outing_sanitize_plan(DesktopPetOutingPlan &plan);
const char *desktop_pet_outing_plan_status_name(
    DesktopPetOutingPlanStatus status);
