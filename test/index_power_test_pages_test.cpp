#include <cassert>

#include "index_power_test_pages.h"

int main()
{
    using namespace sticky_storage;
    IndexPowerTestView view = {};
    view.phase = IndexPowerTestPhase::Ready;
    assert(index_power_test_page_action_at(view, 800, 480, 400, 428) ==
           IndexPowerTestAction::Continue);
    assert(index_power_test_page_action_at(view, 480, 800, 240, 748) ==
           IndexPowerTestAction::Continue);
    view.phase = IndexPowerTestPhase::Confirm;
    assert(index_power_test_page_action_at(view, 800, 480, 100, 428) ==
           IndexPowerTestAction::Cancel);
    assert(index_power_test_page_action_at(view, 800, 480, 600, 428) ==
           IndexPowerTestAction::Start);
    assert(index_power_test_page_action_at(view, 480, 800, 100, 748) ==
           IndexPowerTestAction::Cancel);
    assert(index_power_test_page_action_at(view, 480, 800, 350, 748) ==
           IndexPowerTestAction::Start);
    assert(index_power_test_page_action_at(view, 800, 480, 400, 200) ==
           IndexPowerTestAction::None);
    view.partial_mode = true;
    view.phase = IndexPowerTestPhase::Ready;
    assert(index_power_test_page_action_at(view, 800, 480, 400, 428) ==
           IndexPowerTestAction::Continue);
    view.phase = IndexPowerTestPhase::AwaitingReset;
    assert(index_power_test_page_action_at(view, 800, 480, 400, 428) ==
           IndexPowerTestAction::None);
    return 0;
}
