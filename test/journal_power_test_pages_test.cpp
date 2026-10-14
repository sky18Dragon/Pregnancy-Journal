#include <cassert>
#include <cstdint>

#include "canvas.h"
#include "journal_power_test_pages.h"

int main()
{
    using namespace sticky_storage;
    uint8_t framebuffer[800U * 480U / 2U] = {};
    Canvas canvas(800U, 480U, framebuffer, sizeof(framebuffer));
    JournalPowerTestView view = {};
    view.phase = JournalPowerTestPhase::Ready;
    view.record_count = 3U;
    view.last_transaction_id = 3U;
    view.target_transaction_id = 4U;
    journal_power_test_page_render(canvas, view);
    assert(journal_power_test_page_action_at(view, 800, 480, 400, 430) ==
           JournalPowerTestAction::Continue);

    view.phase = JournalPowerTestPhase::Confirm;
    journal_power_test_page_render(canvas, view);
    assert(journal_power_test_page_action_at(view, 800, 480, 100, 430) ==
           JournalPowerTestAction::Cancel);
    assert(journal_power_test_page_action_at(view, 800, 480, 600, 430) ==
           JournalPowerTestAction::Prepare);

    view.phase = JournalPowerTestPhase::Armed;
    assert(journal_power_test_page_action_at(view, 800, 480, 400, 430) ==
           JournalPowerTestAction::Start);

    canvas.set_rotation(CanvasRotation::Deg90Clockwise);
    view.phase = JournalPowerTestPhase::Ready;
    journal_power_test_page_render(canvas, view);
    assert(journal_power_test_page_action_at(view, 480, 800, 240, 748) ==
           JournalPowerTestAction::Continue);
    view.phase = JournalPowerTestPhase::Confirm;
    assert(journal_power_test_page_action_at(view, 480, 800, 100, 748) ==
           JournalPowerTestAction::Cancel);
    assert(journal_power_test_page_action_at(view, 480, 800, 360, 748) ==
           JournalPowerTestAction::Prepare);
    view.phase = JournalPowerTestPhase::Running;
    assert(journal_power_test_page_action_at(view, 480, 800, 240, 748) ==
           JournalPowerTestAction::None);
    view.partial_mode = true;
    view.phase = JournalPowerTestPhase::Ready;
    assert(journal_power_test_page_action_at(view, 800, 480, 400, 430) ==
           JournalPowerTestAction::Continue);
    view.phase = JournalPowerTestPhase::AwaitingReset;
    assert(journal_power_test_page_action_at(view, 800, 480, 400, 430) ==
           JournalPowerTestAction::None);
    return 0;
}
