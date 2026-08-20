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
    return 0;
}
