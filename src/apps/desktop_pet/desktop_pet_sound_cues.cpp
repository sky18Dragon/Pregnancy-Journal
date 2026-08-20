#include "desktop_pet_sound_cues.h"

#include <cstddef>

namespace {

// Converts displayed dialogue into one of three repeatable pitch variants.
// 把当前显示的对白转换成三种可重复的音高变化之一。
uint8_t message_variant(const char *message)
{
    uint32_t hash = 2166136261U;
    if (message != nullptr) {
        for (size_t index = 0U; message[index] != '\0'; ++index) {
            hash ^= static_cast<uint8_t>(message[index]);
            hash *= 16777619U;
        }
    }
    return static_cast<uint8_t>(hash % 3U);
}

// Selects the rabbit voice from its immediate mood and companion identity.
// 根据兔子的即时心情和伙伴特征选择说话音型。
StickyBuzzerPattern speaking_pattern(const DesktopPetState &state)
{
    switch (pet_core_mood(state.pet)) {
    case PetMood::Hungry:
        return StickyBuzzerPattern::VoiceHungry;
    case PetMood::Tired:
        return StickyBuzzerPattern::VoiceTired;
    case PetMood::Sad:
    case PetMood::Upset:
    case PetMood::Miserable:
        return StickyBuzzerPattern::VoiceSad;
    default:
        break;
    }

    if (state.pet.stage == PetLifeStage::Youth ||
        state.pet.stage == PetLifeStage::Adult) {
        switch (state.pet.branch) {
        case PetPersonalityBranch::Foodie:
            return StickyBuzzerPattern::VoiceFoodie;
        case PetPersonalityBranch::Affectionate:
            return StickyBuzzerPattern::VoiceAffectionate;
        case PetPersonalityBranch::Active:
            return StickyBuzzerPattern::VoiceActive;
        case PetPersonalityBranch::Undecided:
        default:
            break;
        }
    }

    if (state.pet.bond < 35U) {
        return StickyBuzzerPattern::VoiceShy;
    }
    if (state.pet.bond < 70U) {
        return StickyBuzzerPattern::VoiceWarm;
    }
    return StickyBuzzerPattern::VoiceClose;
}

}  // namespace

DesktopPetSoundCue desktop_pet_sound_for_performance(
    const DesktopPetState &state,
    DesktopPetPerformance performance,
    const char *message)
{
    if (desktop_pet_state_requires_sleep(state) &&
        performance != DesktopPetPerformance::FallingAsleep &&
        performance != DesktopPetPerformance::Waking) {
        return {StickyBuzzerPattern::VoiceTired, 0U};
    }
    switch (performance) {
    case DesktopPetPerformance::Eating:
        return {StickyBuzzerPattern::Eat, 0U};
    case DesktopPetPerformance::ReceivingPet:
        return {state.pet.bond < 35U ? StickyBuzzerPattern::CooShy
                                    : StickyBuzzerPattern::CooWarm,
                message_variant(message)};
    case DesktopPetPerformance::Playing:
        return {desktop_pet_state_is_low_energy(state)
                    ? StickyBuzzerPattern::PlayTired
                    : StickyBuzzerPattern::Play,
                message_variant(message)};
    case DesktopPetPerformance::Speaking:
        return {speaking_pattern(state), message_variant(message)};
    case DesktopPetPerformance::FallingAsleep:
        return {StickyBuzzerPattern::Sleep, 0U};
    case DesktopPetPerformance::Waking:
        return {StickyBuzzerPattern::Wake, 0U};
    case DesktopPetPerformance::None:
    default:
        return {};
    }
}

DesktopPetSoundCue desktop_pet_sound_for_idle(
    const DesktopPetState &state,
    DesktopPetIdleFrame frame)
{
    switch (frame) {
    case DesktopPetIdleFrame::Hungry:
        return {StickyBuzzerPattern::VoiceHungry,
                static_cast<uint8_t>(state.pet.day % 3U)};
    case DesktopPetIdleFrame::Tired:
    case DesktopPetIdleFrame::Normal:
    case DesktopPetIdleFrame::Blink:
    case DesktopPetIdleFrame::EarTwitch:
    case DesktopPetIdleFrame::LookAround:
    case DesktopPetIdleFrame::Stretch:
    default:
        return {};
    }
}

void desktop_pet_sound_rearm_need_alerts(
    const DesktopPetState &state,
    DesktopPetNeedSoundState &sound_state)
{
    if (state.pet.needs.food > 30U) {
        sound_state.hunger_announced = false;
    }
    if (state.pet.needs.energy > 0U) {
        sound_state.empty_energy_announced = false;
    }
}

DesktopPetSoundCue desktop_pet_sound_for_hunger_once(
    const DesktopPetState &state,
    DesktopPetNeedSoundState &sound_state)
{
    desktop_pet_sound_rearm_need_alerts(state, sound_state);
    if (state.pet.needs.food > 30U || sound_state.hunger_announced) {
        return {};
    }
    sound_state.hunger_announced = true;
    return {StickyBuzzerPattern::VoiceHungry,
            static_cast<uint8_t>(state.pet.day % 3U)};
}

DesktopPetSoundCue desktop_pet_sound_for_empty_energy_once(
    const DesktopPetState &state,
    DesktopPetNeedSoundState &sound_state)
{
    desktop_pet_sound_rearm_need_alerts(state, sound_state);
    if (state.pet.stage == PetLifeStage::Egg ||
        state.pet.activity == PetActivity::Sleeping ||
        state.pet.needs.energy > 0U ||
        sound_state.empty_energy_announced) {
        return {};
    }
    sound_state.empty_energy_announced = true;
    return {StickyBuzzerPattern::VoiceTired,
            static_cast<uint8_t>(state.pet.day % 3U)};
}
