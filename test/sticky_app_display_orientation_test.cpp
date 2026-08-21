#include <cassert>

#include "sticky_app_display_orientation.h"

namespace {

void expect_rotation(StickyAppId app,
                     StickyImuOrientation orientation,
                     CanvasRotation expected)
{
    CanvasRotation actual = CanvasRotation::Deg0;
    assert(sticky_app_display_rotation(app, orientation, actual));
    assert(actual == expected);
}

}  // namespace

int main()
{
    expect_rotation(StickyAppId::Pomodoro,
                    StickyImuOrientation::Landscape0,
                    CanvasRotation::Deg90CounterClockwise);
    expect_rotation(StickyAppId::Pomodoro,
                    StickyImuOrientation::Landscape180,
                    CanvasRotation::Deg90Clockwise);
    expect_rotation(StickyAppId::StatusBoard,
                    StickyImuOrientation::Portrait0,
                    CanvasRotation::Deg180);
    expect_rotation(StickyAppId::StatusBoard,
                    StickyImuOrientation::Portrait180,
                    CanvasRotation::Deg0);

    CanvasRotation unchanged = CanvasRotation::Deg180;
    assert(!sticky_app_display_rotation(StickyAppId::Pomodoro,
                                        StickyImuOrientation::Portrait0,
                                        unchanged));
    assert(unchanged == CanvasRotation::Deg180);
    assert(!sticky_app_display_rotation(StickyAppId::DesktopPet,
                                        StickyImuOrientation::Landscape0,
                                        unchanged));
    assert(!sticky_app_display_rotation(StickyAppId::BookOfAnswers,
                                        StickyImuOrientation::Landscape0,
                                        unchanged));
    return 0;
}
