#include <cassert>

#include "desktop_pet_power_policy.h"

int main()
{
    constexpr uint32_t kNow = 1000U;
    DesktopPetState state = {};
    state.outing_plan.decision_day_key = 1U;
    state.outing_plan.departure_epoch_seconds = kNow + 100U;
    state.outing_plan.return_epoch_seconds = kNow + 200U;

    // A stale outing plan must never wake an unhatched pet.
    // 遗留的外出计划不能唤醒尚未孵化的宠物。
    assert(state.pet.stage == PetLifeStage::Egg);
    assert(!desktop_pet_power_autonomous_wake_allowed(state));
    assert(desktop_pet_power_next_event_epoch(state, kNow) == 0U);

    state.pet.stage = PetLifeStage::Hatchling;
    assert(desktop_pet_power_autonomous_wake_allowed(state));
    assert(desktop_pet_power_next_event_epoch(state, 0U) == 0U);
    assert(desktop_pet_power_next_event_epoch(state, kNow) ==
           kNow + 100U);
    assert(desktop_pet_power_next_event_epoch(state, kNow + 100U) ==
           kNow + 200U);
    assert(desktop_pet_power_next_event_epoch(state, kNow + 200U) ==
           0U);
    return 0;
}
