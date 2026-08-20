#pragma once

#include <cstdint>

#include "desktop_pet_state.h"
#include "sticky_buzzer_pattern.h"

struct DesktopPetSoundCue {
    StickyBuzzerPattern pattern = StickyBuzzerPattern::None;
    uint8_t variant = 0U;

    bool audible() const
    {
        return pattern != StickyBuzzerPattern::None;
    }
};

// Selects a sound from the pet performance that is currently visible.
// 根据当前可见的宠物表演选择对应声音。
DesktopPetSoundCue desktop_pet_sound_for_performance(
    const DesktopPetState &state,
    DesktopPetPerformance performance,
    const char *message);

// Selects a subtle sound for one autonomous idle animation frame.
// 为一帧自主待机动画选择轻量声音。
DesktopPetSoundCue desktop_pet_sound_for_idle(
    const DesktopPetState &state,
    DesktopPetIdleFrame frame);
