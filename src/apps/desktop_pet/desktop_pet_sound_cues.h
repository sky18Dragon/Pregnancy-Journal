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

struct DesktopPetNeedSoundState {
    bool hunger_announced = false;
    bool empty_energy_announced = false;
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

// Rearms one-shot need alerts after the corresponding need recovers.
// 对应需求恢复后，重新启用下一次单次状态提醒。
void desktop_pet_sound_rearm_need_alerts(
    const DesktopPetState &state,
    DesktopPetNeedSoundState &sound_state);

// Emits the hunger voice only once during one continuous hunger episode.
// 在一次连续饥饿期间只返回一次饥饿声音。
DesktopPetSoundCue desktop_pet_sound_for_hunger_once(
    const DesktopPetState &state,
    DesktopPetNeedSoundState &sound_state);

// Emits the tired voice only once while energy remains at zero.
// 在能量持续为零期间只返回一次疲劳声音。
DesktopPetSoundCue desktop_pet_sound_for_empty_energy_once(
    const DesktopPetState &state,
    DesktopPetNeedSoundState &sound_state);
