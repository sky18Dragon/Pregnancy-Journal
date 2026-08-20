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

constexpr uint32_t kDesktopPetOutingTestMinimumMs = 20000U;
constexpr uint32_t kDesktopPetOutingTestMaximumMs = 40000U;
constexpr uint32_t kDesktopPetOutingProductionMinimumMs = 3600000U;
constexpr uint32_t kDesktopPetOutingProductionMaximumMs = 25200000U;

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
