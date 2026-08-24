#include <cassert>

#include "sticky_app_gesture.h"

int main()
{
    StickyAppGestureSample portrait = {
        480, 800, 240, 785, 250, 560, 420U, false};
    assert(sticky_app_gesture_classify(portrait) ==
           StickyAppGestureAction::OpenLauncher);

    StickyAppGestureSample landscape = {
        800, 480, 400, 470, 410, 350, 300U, false};
    assert(sticky_app_gesture_classify(landscape) ==
           StickyAppGestureAction::OpenLauncher);

    portrait.start_y = 690;
    portrait.end_y = 480;
    assert(sticky_app_gesture_classify(portrait) ==
           StickyAppGestureAction::None);
    portrait = {480, 800, 240, 785, 430, 560, 420U, false};
    assert(sticky_app_gesture_classify(portrait) ==
           StickyAppGestureAction::None);

    portrait = {480, 800, 240, 400, 250, 590, 360U, true};
    assert(sticky_app_gesture_classify(portrait) ==
           StickyAppGestureAction::CloseLauncher);
    landscape = {800, 480, 400, 220, 410, 340, 300U, true};
    assert(sticky_app_gesture_classify(landscape) ==
           StickyAppGestureAction::CloseLauncher);

    portrait.end_y = 470;
    assert(sticky_app_gesture_classify(portrait) ==
           StickyAppGestureAction::None);
    portrait = {480, 800, 240, 400, 250, 590, 1700U, true};
    assert(sticky_app_gesture_classify(portrait) ==
           StickyAppGestureAction::None);

    portrait = {480, 800, 240, 400, 250, 190, 360U, true};
    assert(sticky_app_gesture_classify(portrait) ==
           StickyAppGestureAction::None);
    portrait = {480, 800, 240, 785, 250, 560, 420U, true};
    assert(sticky_app_gesture_classify(portrait) ==
           StickyAppGestureAction::None);
    return 0;
}
