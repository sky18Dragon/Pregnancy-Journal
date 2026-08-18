#include <cassert>

#include "pomodoro_render_policy.h"

int main()
{
    assert(pomodoro_render_clears_pending_touch(
        PomodoroRenderReason::InteractiveChange));
    assert(!pomodoro_render_clears_pending_touch(
        PomodoroRenderReason::CountdownTick));
    return 0;
}
