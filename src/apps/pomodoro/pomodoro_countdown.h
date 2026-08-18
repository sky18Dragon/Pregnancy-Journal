#pragma once

#include <cstdint>

// Returns true whenever the visible countdown second has changed.
// 当屏幕上应该显示的倒计时秒数发生变化时返回true。
constexpr bool pomodoro_countdown_should_render_frame(
    uint32_t displayed_seconds,
    uint32_t remaining_seconds)
{
    return displayed_seconds != remaining_seconds;
}
