#include <cassert>
#include <cstdint>
#include <limits>

#include "sd_card_state.h"

using sticky_storage::SdCardAccess;
using sticky_storage::SdCardState;
using sticky_storage::SdCardStateMachine;

int main()
{
    SdCardStateMachine invalidated;
    assert(invalidated.begin_mount(true));
    assert(invalidated.complete_mount(SdCardAccess::ReadWrite));
    const auto old_generation = invalidated.generation();
    invalidated.invalidate_requests();
    assert(invalidated.can_write());
    assert(!invalidated.accepts(old_generation, true));
    assert(invalidated.accepts(invalidated.generation(), true));
    SdCardStateMachine card;
    assert(card.state() == SdCardState::Absent);
    assert(card.generation() == 0U);
    assert(!card.mounted() && !card.can_read() && !card.can_write());
    assert(!card.begin_mount(false));

    assert(card.begin_mount(true));
    assert(card.state() == SdCardState::Mounting);
    assert(card.generation() == 1U);
    assert(!card.accepts(1U, false));
    assert(!card.begin_mount(true));
    assert(card.complete_mount(SdCardAccess::ReadWrite));
    assert(card.state() == SdCardState::ReadyReadWrite && card.mounted());
    assert(card.accepts(1U, false));
    assert(card.accepts(1U, true));
    assert(!card.accepts(0U, false));
    assert(!card.accepts(2U, false));

    assert(card.mark_full());
    assert(card.state() == SdCardState::Full);
    assert(card.can_read() && !card.can_write());
    assert(card.accepts(1U, false) && !card.accepts(1U, true));
    assert(card.mark_space_available());
    assert(card.can_write());
    assert(card.mark_read_only());
    assert(card.state() == SdCardState::ReadyReadOnly);
    assert(card.can_read() && !card.can_write());
    assert(!card.mark_full());

    assert(card.begin_eject());
    assert(card.state() == SdCardState::Ejecting);
    assert(!card.can_read() && !card.can_write());
    assert(card.fail_eject());
    assert(card.state() == SdCardState::Fault && card.mounted());
    assert(!card.can_read() && !card.can_write());
    assert(card.begin_eject());
    assert(card.complete_eject());
    assert(card.state() == SdCardState::Unmounted && !card.mounted());

    assert(card.begin_mount(true));
    assert(card.generation() == 2U);
    assert(card.complete_mount(SdCardAccess::ReadOnly));
    assert(card.can_read() && !card.can_write());
    assert(card.accepts(2U, false) && !card.accepts(2U, true));
    card.card_removed();
    assert(card.state() == SdCardState::Absent && !card.mounted());
    assert(!card.accepts(2U, false));

    assert(card.begin_mount(true));
    assert(card.generation() == 3U);
    assert(card.fail_mount(true));
    assert(card.state() == SdCardState::Fault && !card.mounted());
    assert(card.clear_fault(true));
    assert(card.state() == SdCardState::Unmounted);
    assert(card.begin_mount(true));
    assert(card.generation() == 4U);
    assert(card.fail_mount(false));
    assert(card.state() == SdCardState::Absent);

    SdCardStateMachine wrapping(std::numeric_limits<uint32_t>::max());
    assert(wrapping.begin_mount(true));
    assert(wrapping.generation() == 1U);
}
