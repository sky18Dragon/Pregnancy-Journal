#include <cassert>
#include <cstdint>

#include "desktop_pet_outing.h"

int main()
{
    assert(desktop_pet_outing_duration_ms(0U, true) == 20000U);
    assert(desktop_pet_outing_duration_ms(20000U, true) == 40000U);
    assert(desktop_pet_outing_duration_ms(0U, false) == 3600000U);
    assert(desktop_pet_outing_duration_ms(21600000U, false) ==
           25200000U);

    DesktopPetOutingSession session = {};
    assert(!desktop_pet_outing_active(session));
    assert(desktop_pet_outing_start(session, 1000U, 11U, true));
    assert(session.phase == DesktopPetOutingPhase::Packing);
    assert(session.away_duration_ms == 20011U);
    assert(!desktop_pet_outing_start(session, 1000U, 0U, true));
    assert(!desktop_pet_outing_call_home(session, 1500U));
    assert(!desktop_pet_outing_update(session, 2499U));
    assert(desktop_pet_outing_update(session, 2500U));
    assert(session.phase == DesktopPetOutingPhase::Leaving);
    assert(desktop_pet_outing_update(session, 4300U));
    assert(session.phase == DesktopPetOutingPhase::Away);

    DesktopPetOutingSession called_home = session;
    assert(desktop_pet_outing_call_home(called_home, 5000U));
    assert(called_home.phase == DesktopPetOutingPhase::Returning);
    assert(!desktop_pet_outing_call_home(called_home, 5001U));
    assert(desktop_pet_outing_update(called_home, 6700U));
    assert(called_home.phase == DesktopPetOutingPhase::Reunion);
    assert(desktop_pet_outing_update(called_home, 9000U));
    assert(called_home.phase == DesktopPetOutingPhase::Home);
    assert(!desktop_pet_outing_active(called_home));

    assert(desktop_pet_outing_update(session, 24311U));
    assert(session.phase == DesktopPetOutingPhase::Returning);

    DesktopPetOutingPlan production_home = {};
    const uint32_t day_start = 20000U * 86400U;
    assert(desktop_pet_outing_plan_day(
        production_home, day_start + 8U * 3600U,
        4U, 0U, 0U, false));
    assert(desktop_pet_outing_plan_status(
               production_home, day_start + 8U * 3600U) ==
           DesktopPetOutingPlanStatus::StayingHome);
    assert(!desktop_pet_outing_plan_day(
        production_home, day_start + 9U * 3600U,
        0U, 0U, 0U, false));

    DesktopPetOutingPlan production_trip = {};
    assert(desktop_pet_outing_plan_day(
        production_trip, day_start + 8U * 3600U,
        0U, 0U, 0U, false));
    assert(production_trip.departure_epoch_seconds ==
           day_start + 9U * 3600U);
    assert(production_trip.return_epoch_seconds ==
           day_start + 10U * 3600U);
    assert(desktop_pet_outing_plan_status(
               production_trip, day_start + 8U * 3600U) ==
           DesktopPetOutingPlanStatus::Scheduled);
    assert(desktop_pet_outing_plan_status(
               production_trip, day_start + 9U * 3600U) ==
           DesktopPetOutingPlanStatus::Away);
    assert(desktop_pet_outing_plan_remaining_seconds(
               production_trip, day_start + 9U * 3600U) == 3600U);
    assert(desktop_pet_outing_plan_status(
               production_trip, day_start + 10U * 3600U) ==
           DesktopPetOutingPlanStatus::Completed);

    DesktopPetOutingPlan late_day = {};
    assert(desktop_pet_outing_plan_day(
        late_day, day_start + 16U * 3600U,
        0U, 0U, 0U, false));
    assert(desktop_pet_outing_plan_status(
               late_day, day_start + 16U * 3600U) ==
           DesktopPetOutingPlanStatus::StayingHome);

    DesktopPetOutingPlan test_trip = {};
    assert(desktop_pet_outing_plan_day(
        test_trip, day_start + 18U * 3600U,
        4U, 0U, 0U, true));
    assert(test_trip.departure_epoch_seconds ==
           day_start + 18U * 3600U + 15U);
    assert(test_trip.return_epoch_seconds ==
           day_start + 18U * 3600U + 35U);

    DesktopPetOutingSession resumed = {};
    assert(desktop_pet_outing_resume_away(resumed, 1000U, 25000U));
    assert(resumed.phase == DesktopPetOutingPhase::Away);
    assert(resumed.phase_deadline_ms == 26000U);

    DesktopPetOutingSession resumed_returning = {};
    assert(desktop_pet_outing_resume_returning(
        resumed_returning, 1000U));
    assert(resumed_returning.phase == DesktopPetOutingPhase::Returning);
    assert(resumed_returning.phase_deadline_ms == 2700U);
    assert(!desktop_pet_outing_resume_returning(
        resumed_returning, 1001U));
    assert(desktop_pet_outing_update(resumed_returning, 2700U));
    assert(resumed_returning.phase == DesktopPetOutingPhase::Reunion);

    desktop_pet_outing_complete_plan(test_trip);
    assert(desktop_pet_outing_plan_status(
               test_trip, day_start + 18U * 3600U + 16U) ==
           DesktopPetOutingPlanStatus::StayingHome);

    DesktopPetOutingPlan invalid = {};
    invalid.decision_day_key = 20U;
    invalid.departure_epoch_seconds = 300U;
    invalid.return_epoch_seconds = 200U;
    desktop_pet_outing_sanitize_plan(invalid);
    assert(invalid.decision_day_key == 0U);
    return 0;
}
