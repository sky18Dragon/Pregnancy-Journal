#include <cassert>

#include "desktop_pet_sound_cues.h"

int main()
{
    DesktopPetState state = {};
    state.pet.stage = PetLifeStage::Hatchling;

    DesktopPetSoundCue cue = desktop_pet_sound_for_performance(
        state, DesktopPetPerformance::Eating, "YUM!");
    assert(cue.pattern == StickyBuzzerPattern::Eat);

    cue = desktop_pet_sound_for_performance(
        state, DesktopPetPerformance::ReceivingPet, "HELLO.");
    assert(cue.pattern == StickyBuzzerPattern::CooShy);
    state.pet.bond = 50U;
    cue = desktop_pet_sound_for_performance(
        state, DesktopPetPerformance::ReceivingPet, "HELLO.");
    assert(cue.pattern == StickyBuzzerPattern::CooWarm);

    state.pet.needs.energy = 80U;
    cue = desktop_pet_sound_for_performance(
        state, DesktopPetPerformance::Playing, "LET'S PLAY!");
    assert(cue.pattern == StickyBuzzerPattern::Play);
    state.pet.needs.energy = 8U;
    cue = desktop_pet_sound_for_performance(
        state, DesktopPetPerformance::Playing, "ONE MORE HOP!");
    assert(cue.pattern == StickyBuzzerPattern::PlayTired);
    state.pet.needs.energy = 80U;

    state.pet.bond = 20U;
    cue = desktop_pet_sound_for_performance(
        state, DesktopPetPerformance::Speaking, "WHO ARE YOU?");
    assert(cue.pattern == StickyBuzzerPattern::VoiceShy);
    state.pet.bond = 50U;
    cue = desktop_pet_sound_for_performance(
        state, DesktopPetPerformance::Speaking, "WELCOME BACK.");
    assert(cue.pattern == StickyBuzzerPattern::VoiceWarm);
    state.pet.bond = 80U;
    cue = desktop_pet_sound_for_performance(
        state, DesktopPetPerformance::Speaking, "STAY WITH ME.");
    assert(cue.pattern == StickyBuzzerPattern::VoiceClose);

    state.pet.needs.food = 20U;
    cue = desktop_pet_sound_for_performance(
        state, DesktopPetPerformance::Speaking, "CARROT, PLEASE.");
    assert(cue.pattern == StickyBuzzerPattern::VoiceHungry);
    state.pet.needs.food = 80U;
    state.pet.needs.energy = 20U;
    cue = desktop_pet_sound_for_performance(
        state, DesktopPetPerformance::Speaking, "I'M TIRED.");
    assert(cue.pattern == StickyBuzzerPattern::VoiceTired);

    state.pet.needs.energy = 80U;
    state.pet.needs.joy = 35U;
    cue = desktop_pet_sound_for_performance(
        state, DesktopPetPerformance::Speaking, "EARS ARE HEAVY TODAY.");
    assert(cue.pattern == StickyBuzzerPattern::VoiceSad);

    state.pet.needs.joy = 80U;
    state.pet.bond = 80U;
    state.pet.stage = PetLifeStage::Adult;
    state.pet.branch = PetPersonalityBranch::Foodie;
    cue = desktop_pet_sound_for_performance(
        state, DesktopPetPerformance::Speaking, "I MADE A SNACK.");
    assert(cue.pattern == StickyBuzzerPattern::VoiceFoodie);
    state.pet.branch = PetPersonalityBranch::Affectionate;
    cue = desktop_pet_sound_for_performance(
        state, DesktopPetPerformance::Speaking, "STAY CLOSE.");
    assert(cue.pattern == StickyBuzzerPattern::VoiceAffectionate);
    state.pet.branch = PetPersonalityBranch::Active;
    cue = desktop_pet_sound_for_performance(
        state, DesktopPetPerformance::Speaking, "LET'S GO!");
    assert(cue.pattern == StickyBuzzerPattern::VoiceActive);

    cue = desktop_pet_sound_for_idle(
        state, DesktopPetIdleFrame::Blink);
    assert(!cue.audible());
    cue = desktop_pet_sound_for_idle(
        state, DesktopPetIdleFrame::Stretch);
    assert(!cue.audible());
    cue = desktop_pet_sound_for_idle(
        state, DesktopPetIdleFrame::Hungry);
    assert(cue.pattern == StickyBuzzerPattern::VoiceHungry);
    cue = desktop_pet_sound_for_idle(
        state, DesktopPetIdleFrame::Tired);
    assert(!cue.audible());

    DesktopPetNeedSoundState need_sound_state = {};
    state.pet.needs.food = 30U;
    cue = desktop_pet_sound_for_hunger_once(state, need_sound_state);
    assert(cue.pattern == StickyBuzzerPattern::VoiceHungry);
    cue = desktop_pet_sound_for_hunger_once(state, need_sound_state);
    assert(!cue.audible());
    state.pet.needs.food = 31U;
    desktop_pet_sound_rearm_need_alerts(state, need_sound_state);
    state.pet.needs.food = 30U;
    cue = desktop_pet_sound_for_hunger_once(state, need_sound_state);
    assert(cue.pattern == StickyBuzzerPattern::VoiceHungry);

    state.pet.needs.energy = 0U;
    cue = desktop_pet_sound_for_empty_energy_once(
        state, need_sound_state);
    assert(cue.pattern == StickyBuzzerPattern::VoiceTired);
    cue = desktop_pet_sound_for_empty_energy_once(
        state, need_sound_state);
    assert(!cue.audible());
    state.pet.needs.energy = 1U;
    desktop_pet_sound_rearm_need_alerts(state, need_sound_state);
    state.pet.needs.energy = 0U;
    cue = desktop_pet_sound_for_empty_energy_once(
        state, need_sound_state);
    assert(cue.pattern == StickyBuzzerPattern::VoiceTired);

    cue = desktop_pet_sound_for_performance(
        state, DesktopPetPerformance::FallingAsleep, "GOOD NIGHT.");
    assert(cue.pattern == StickyBuzzerPattern::Sleep);
    cue = desktop_pet_sound_for_performance(
        state, DesktopPetPerformance::Waking, "GOOD MORNING.");
    assert(cue.pattern == StickyBuzzerPattern::Wake);
    return 0;
}
