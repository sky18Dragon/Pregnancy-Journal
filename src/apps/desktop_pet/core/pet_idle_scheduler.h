#pragma once

#include <cstdint>

#include "pet_core.h"

enum class PetIdleAction : uint8_t {
    None,
    Blink,
    EarTwitch,
    LookAround,
    Stretch,
    Hungry,
    Tired,
};

// Selects one state-aware idle action while avoiding the last two actions.
// 选择匹配当前状态的待机动作，并避开最近两个动作。
PetIdleAction pet_idle_select(const PetCoreState &state,
                              PetIdleAction previous,
                              PetIdleAction second_previous,
                              uint32_t random_value);

// Returns the dialogue shown while one idle action is visible.
// 返回待机动作显示期间使用的对白。
const char *pet_idle_message(PetIdleAction action,
                             PetLifeStage stage);

// Returns the delay before the next autonomous action.
// 返回下一次自主动作开始前的等待时间。
uint32_t pet_idle_next_delay_ms(uint32_t random_value,
                                bool accelerated);

const char *pet_idle_action_name(PetIdleAction action);
