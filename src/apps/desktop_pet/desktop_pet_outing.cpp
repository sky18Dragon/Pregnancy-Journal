#include "desktop_pet_outing.h"

namespace {

constexpr uint32_t kPackingHoldMs = 1500U;
constexpr uint32_t kLeavingHoldMs = 1800U;
constexpr uint32_t kReturningHoldMs = 1700U;
constexpr uint32_t kReunionHoldMs = 2300U;

bool deadline_reached(uint32_t now_ms, uint32_t deadline_ms)
{
    return static_cast<int32_t>(now_ms - deadline_ms) >= 0;
}

}  // namespace

uint32_t desktop_pet_outing_duration_ms(uint32_t random_value,
                                        bool accelerated)
{
    const uint32_t minimum = accelerated
                                 ? kDesktopPetOutingTestMinimumMs
                                 : kDesktopPetOutingProductionMinimumMs;
    const uint32_t maximum = accelerated
                                 ? kDesktopPetOutingTestMaximumMs
                                 : kDesktopPetOutingProductionMaximumMs;
    return minimum + random_value % (maximum - minimum + 1U);
}

bool desktop_pet_outing_start(DesktopPetOutingSession &session,
                              uint32_t now_ms,
                              uint32_t random_value,
                              bool accelerated)
{
    if (desktop_pet_outing_active(session)) {
        return false;
    }
    session.phase = DesktopPetOutingPhase::Packing;
    session.away_duration_ms = desktop_pet_outing_duration_ms(
        random_value, accelerated);
    session.phase_deadline_ms = now_ms + kPackingHoldMs;
    return true;
}

bool desktop_pet_outing_update(DesktopPetOutingSession &session,
                               uint32_t now_ms)
{
    if (session.phase == DesktopPetOutingPhase::Home ||
        !deadline_reached(now_ms, session.phase_deadline_ms)) {
        return false;
    }

    switch (session.phase) {
    case DesktopPetOutingPhase::Packing:
        session.phase = DesktopPetOutingPhase::Leaving;
        session.phase_deadline_ms = now_ms + kLeavingHoldMs;
        break;
    case DesktopPetOutingPhase::Leaving:
        session.phase = DesktopPetOutingPhase::Away;
        session.phase_deadline_ms = now_ms + session.away_duration_ms;
        break;
    case DesktopPetOutingPhase::Away:
        session.phase = DesktopPetOutingPhase::Returning;
        session.phase_deadline_ms = now_ms + kReturningHoldMs;
        break;
    case DesktopPetOutingPhase::Returning:
        session.phase = DesktopPetOutingPhase::Reunion;
        session.phase_deadline_ms = now_ms + kReunionHoldMs;
        break;
    case DesktopPetOutingPhase::Reunion:
        session.phase = DesktopPetOutingPhase::Home;
        session.away_duration_ms = 0U;
        session.phase_deadline_ms = 0U;
        break;
    case DesktopPetOutingPhase::Home:
    default:
        return false;
    }
    return true;
}

bool desktop_pet_outing_call_home(DesktopPetOutingSession &session,
                                  uint32_t now_ms)
{
    if (session.phase != DesktopPetOutingPhase::Away) {
        return false;
    }
    session.phase = DesktopPetOutingPhase::Returning;
    session.phase_deadline_ms = now_ms + kReturningHoldMs;
    return true;
}

bool desktop_pet_outing_active(const DesktopPetOutingSession &session)
{
    return session.phase != DesktopPetOutingPhase::Home;
}

const char *desktop_pet_outing_phase_name(DesktopPetOutingPhase phase)
{
    switch (phase) {
    case DesktopPetOutingPhase::Packing:
        return "packing";
    case DesktopPetOutingPhase::Leaving:
        return "leaving";
    case DesktopPetOutingPhase::Away:
        return "away";
    case DesktopPetOutingPhase::Returning:
        return "returning";
    case DesktopPetOutingPhase::Reunion:
        return "reunion";
    case DesktopPetOutingPhase::Home:
    default:
        return "home";
    }
}
