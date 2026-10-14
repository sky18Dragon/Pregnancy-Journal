#include <cassert>

#include "object_power_test_pages.h"

int main()
{
    using namespace sticky_storage;
    ObjectPowerTestView view = {};
    view.phase = ObjectPowerTestPhase::Ready;
    assert(object_power_test_page_action_at(
               view, 800, 480, 400, 428) ==
           ObjectPowerTestAction::Continue);
    assert(object_power_test_page_action_at(
               view, 480, 800, 240, 748) ==
           ObjectPowerTestAction::Continue);

    view.phase = ObjectPowerTestPhase::Confirm;
    assert(object_power_test_page_action_at(
               view, 800, 480, 100, 428) ==
           ObjectPowerTestAction::Cancel);
    assert(object_power_test_page_action_at(
               view, 800, 480, 600, 428) ==
           ObjectPowerTestAction::Start);
    assert(object_power_test_page_action_at(
               view, 480, 800, 100, 748) ==
           ObjectPowerTestAction::Cancel);
    assert(object_power_test_page_action_at(
               view, 480, 800, 350, 748) ==
           ObjectPowerTestAction::Start);
    assert(object_power_test_page_action_at(
               view, 800, 480, 400, 200) ==
           ObjectPowerTestAction::None);

    view.partial_mode = true;
    view.phase = ObjectPowerTestPhase::Ready;
    assert(object_power_test_page_action_at(
               view, 800, 480, 400, 428) ==
           ObjectPowerTestAction::Continue);
    view.phase = ObjectPowerTestPhase::AwaitingReset;
    assert(object_power_test_page_action_at(
               view, 800, 480, 400, 428) ==
           ObjectPowerTestAction::None);
    return 0;
}
