#pragma once

enum class PomodoroRenderReason {
    InteractiveChange,
    CountdownTick,
};

// Returns whether a render should discard a press captured during refresh.
// 返回一次画面刷新结束后是否应丢弃刷新期间记录的触摸事件。
constexpr bool pomodoro_render_clears_pending_touch(
    PomodoroRenderReason reason)
{
    return reason == PomodoroRenderReason::InteractiveChange;
}
