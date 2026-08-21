#include "desktop_pet_outing.h"

namespace {

constexpr uint32_t kPackingHoldMs = 1500U;
constexpr uint32_t kLeavingHoldMs = 1800U;
constexpr uint32_t kReturningHoldMs = 1700U;
constexpr uint32_t kReunionHoldMs = 2300U;
constexpr uint32_t kSecondsPerDay = 86400U;
constexpr uint32_t kProductionEarliestDepartureSecond = 9U * 3600U;
constexpr uint32_t kProductionLatestDepartureSecond = 16U * 3600U;
constexpr uint32_t kProductionDepartureLeadSeconds = 10U * 60U;
constexpr uint32_t kTestMinimumDepartureLeadSeconds = 15U;
constexpr uint32_t kTestMaximumDepartureLeadSeconds = 30U;

bool deadline_reached(uint32_t now_ms, uint32_t deadline_ms)
{
    return static_cast<int32_t>(now_ms - deadline_ms) >= 0;
}

uint32_t inclusive_random(uint32_t minimum,
                          uint32_t maximum,
                          uint32_t random_value)
{
    return minimum + random_value % (maximum - minimum + 1U);
}

uint32_t outing_duration_seconds(uint32_t random_value,
                                 bool accelerated)
{
    const uint32_t minimum = accelerated
                                 ? kDesktopPetOutingTestMinimumMs / 1000U
                                 : kDesktopPetOutingProductionMinimumMs / 1000U;
    const uint32_t maximum = accelerated
                                 ? kDesktopPetOutingTestMaximumMs / 1000U
                                 : kDesktopPetOutingProductionMaximumMs / 1000U;
    return inclusive_random(minimum, maximum, random_value);
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
    return desktop_pet_outing_start_for_duration(
        session,
        now_ms,
        desktop_pet_outing_duration_ms(random_value, accelerated));
}

bool desktop_pet_outing_start_for_duration(
    DesktopPetOutingSession &session,
    uint32_t now_ms,
    uint32_t away_duration_ms)
{
    if (desktop_pet_outing_active(session) || away_duration_ms == 0U) {
        return false;
    }
    session.phase = DesktopPetOutingPhase::Packing;
    session.away_duration_ms = away_duration_ms;
    session.phase_deadline_ms = now_ms + kPackingHoldMs;
    return true;
}

bool desktop_pet_outing_resume_away(DesktopPetOutingSession &session,
                                    uint32_t now_ms,
                                    uint32_t remaining_duration_ms)
{
    if (desktop_pet_outing_active(session) ||
        remaining_duration_ms == 0U) {
        return false;
    }
    session.phase = DesktopPetOutingPhase::Away;
    session.away_duration_ms = remaining_duration_ms;
    session.phase_deadline_ms = now_ms + remaining_duration_ms;
    return true;
}

bool desktop_pet_outing_resume_returning(
    DesktopPetOutingSession &session,
    uint32_t now_ms)
{
    if (desktop_pet_outing_active(session)) {
        return false;
    }
    session.phase = DesktopPetOutingPhase::Returning;
    session.away_duration_ms = 0U;
    session.phase_deadline_ms = now_ms + kReturningHoldMs;
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

bool desktop_pet_outing_plan_day(DesktopPetOutingPlan &plan,
                                 uint32_t now_epoch_seconds,
                                 uint32_t decision_random_value,
                                 uint32_t departure_random_value,
                                 uint32_t duration_random_value,
                                 bool accelerated)
{
    if (now_epoch_seconds == 0U) {
        return false;
    }

    const uint32_t day_key = now_epoch_seconds / kSecondsPerDay;
    if (plan.decision_day_key >= day_key) {
        return false;
    }

    plan.decision_day_key = day_key;
    plan.departure_epoch_seconds = 0U;
    plan.return_epoch_seconds = 0U;

    const bool goes_out = accelerated ||
        decision_random_value % kDesktopPetOutingProductionChanceDenominator <
            kDesktopPetOutingProductionChanceNumerator;
    if (!goes_out) {
        return true;
    }

    uint32_t earliest_departure = 0U;
    uint32_t latest_departure = 0U;
    if (accelerated) {
        earliest_departure = now_epoch_seconds +
                             kTestMinimumDepartureLeadSeconds;
        latest_departure = now_epoch_seconds +
                           kTestMaximumDepartureLeadSeconds;
    } else {
        const uint32_t day_start = day_key * kSecondsPerDay;
        earliest_departure = day_start +
                             kProductionEarliestDepartureSecond;
        const uint32_t departure_after_start = now_epoch_seconds +
                                               kProductionDepartureLeadSeconds;
        if (departure_after_start > earliest_departure) {
            earliest_departure = departure_after_start;
        }
        latest_departure = day_start +
                           kProductionLatestDepartureSecond;
    }
    if (earliest_departure > latest_departure) {
        return true;
    }

    plan.departure_epoch_seconds = inclusive_random(
        earliest_departure, latest_departure, departure_random_value);
    plan.return_epoch_seconds = plan.departure_epoch_seconds +
        outing_duration_seconds(duration_random_value, accelerated);
    return true;
}

DesktopPetOutingPlanStatus desktop_pet_outing_plan_status(
    const DesktopPetOutingPlan &plan,
    uint32_t now_epoch_seconds)
{
    if (plan.decision_day_key == 0U) {
        return DesktopPetOutingPlanStatus::Unplanned;
    }
    if (plan.departure_epoch_seconds == 0U ||
        plan.return_epoch_seconds == 0U) {
        return DesktopPetOutingPlanStatus::StayingHome;
    }
    if (now_epoch_seconds < plan.departure_epoch_seconds) {
        return DesktopPetOutingPlanStatus::Scheduled;
    }
    if (now_epoch_seconds < plan.return_epoch_seconds) {
        return DesktopPetOutingPlanStatus::Away;
    }
    return DesktopPetOutingPlanStatus::Completed;
}

uint32_t desktop_pet_outing_plan_remaining_seconds(
    const DesktopPetOutingPlan &plan,
    uint32_t now_epoch_seconds)
{
    return plan.return_epoch_seconds > now_epoch_seconds
               ? plan.return_epoch_seconds - now_epoch_seconds
               : 0U;
}

void desktop_pet_outing_complete_plan(DesktopPetOutingPlan &plan)
{
    plan.departure_epoch_seconds = 0U;
    plan.return_epoch_seconds = 0U;
}

void desktop_pet_outing_sanitize_plan(DesktopPetOutingPlan &plan)
{
    const bool valid_empty_plan = plan.departure_epoch_seconds == 0U &&
                                  plan.return_epoch_seconds == 0U;
    const bool valid_timed_plan = plan.decision_day_key != 0U &&
        plan.departure_epoch_seconds != 0U &&
        plan.return_epoch_seconds > plan.departure_epoch_seconds;
    if (plan.decision_day_key == 0U ||
        (!valid_empty_plan && !valid_timed_plan)) {
        plan = {};
    }
}

const char *desktop_pet_outing_plan_status_name(
    DesktopPetOutingPlanStatus status)
{
    switch (status) {
    case DesktopPetOutingPlanStatus::StayingHome:
        return "staying_home";
    case DesktopPetOutingPlanStatus::Scheduled:
        return "scheduled";
    case DesktopPetOutingPlanStatus::Away:
        return "away";
    case DesktopPetOutingPlanStatus::Completed:
        return "completed";
    case DesktopPetOutingPlanStatus::Unplanned:
    default:
        return "unplanned";
    }
}
