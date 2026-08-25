#include "desktop_pet_power_policy.h"

bool desktop_pet_power_autonomous_wake_allowed(
    const DesktopPetState &state)
{
    return state.pet.stage != PetLifeStage::Egg;
}

uint32_t desktop_pet_power_next_event_epoch(
    const DesktopPetState &state,
    uint32_t current_epoch_seconds)
{
    if (current_epoch_seconds == 0U ||
        !desktop_pet_power_autonomous_wake_allowed(state)) {
        return 0U;
    }

    const DesktopPetOutingPlanStatus status =
        desktop_pet_outing_plan_status(
            state.outing_plan, current_epoch_seconds);
    if (status == DesktopPetOutingPlanStatus::Scheduled) {
        return state.outing_plan.departure_epoch_seconds;
    }
    if (status == DesktopPetOutingPlanStatus::Away) {
        return state.outing_plan.return_epoch_seconds;
    }
    return 0U;
}
