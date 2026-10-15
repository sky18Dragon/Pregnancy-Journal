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

    portrait = {480, 800, 240, 5, 250, 405, 500U, true};
    assert(sticky_app_gesture_classify(portrait) ==
           StickyAppGestureAction::CloseLauncher);
    landscape = {800, 480, 400, 5, 410, 245, 420U, true};
    assert(sticky_app_gesture_classify(landscape) ==
           StickyAppGestureAction::CloseLauncher);

    portrait = {480, 800, 240, 570, 250, 760, 360U, true};
    assert(sticky_app_gesture_classify(portrait) ==
           StickyAppGestureAction::None);

    portrait = {480, 800, 240, 400, 250, 470, 360U, true};
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

    // Bottom-edge horizontal swipes switch apps (launcher closed only).
    landscape = {800, 480, 700, 470, 300, 474, 400U, false};
    assert(sticky_app_gesture_classify(landscape) == StickyAppGestureAction::NextApp);
    landscape = {800, 480, 100, 470, 560, 466, 400U, false};
    assert(sticky_app_gesture_classify(landscape) == StickyAppGestureAction::PreviousApp);
    portrait = {480, 800, 400, 785, 100, 790, 400U, false};
    assert(sticky_app_gesture_classify(portrait) == StickyAppGestureAction::NextApp);
    portrait = {480, 800, 60, 785, 420, 780, 400U, false};
    assert(sticky_app_gesture_classify(portrait) == StickyAppGestureAction::PreviousApp);
    // Not from the middle of the screen, too short, too slow/fast, too diagonal.
    landscape = {800, 480, 700, 300, 300, 304, 400U, false};
    assert(sticky_app_gesture_classify(landscape) == StickyAppGestureAction::None);
    landscape = {800, 480, 500, 470, 400, 470, 400U, false};
    assert(sticky_app_gesture_classify(landscape) == StickyAppGestureAction::None);
    landscape = {800, 480, 700, 470, 300, 470, 1700U, false};
    assert(sticky_app_gesture_classify(landscape) == StickyAppGestureAction::None);
    landscape = {800, 480, 700, 470, 300, 470, 30U, false};
    assert(sticky_app_gesture_classify(landscape) == StickyAppGestureAction::None);
    landscape = {800, 480, 700, 470, 300, 300, 400U, false};
    assert(sticky_app_gesture_classify(landscape) == StickyAppGestureAction::None);
    // With the launcher open a horizontal swipe does nothing.
    landscape = {800, 480, 700, 470, 300, 474, 400U, true};
    assert(sticky_app_gesture_classify(landscape) == StickyAppGestureAction::None);
    return 0;
}
