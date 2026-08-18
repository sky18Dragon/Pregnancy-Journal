#include <cassert>

#include "pomodoro_countdown.h"

int main()
{
    assert(pomodoro_countdown_should_render_frame(10U, 9U));
    assert(pomodoro_countdown_should_render_frame(30U, 29U));
    assert(pomodoro_countdown_should_render_frame(180U, 179U));
    assert(pomodoro_countdown_should_render_frame(300U, 299U));
    assert(pomodoro_countdown_should_render_frame(900U, 899U));
    assert(pomodoro_countdown_should_render_frame(3600U, 3599U));
    assert(!pomodoro_countdown_should_render_frame(899U, 899U));
    return 0;
}
