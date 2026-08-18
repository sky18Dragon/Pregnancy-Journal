#pragma once

#include <cstdint>

class Canvas;

enum class PomodoroPage {
    Setup,
    CustomTime,
    Running,
    Paused,
    EndConfirmation,
    Alarm,
};

enum class PomodoroTimeField {
    Hours,
    Minutes,
    Seconds,
};

enum class PomodoroAction {
    None,
    Preset10Seconds,
    Preset30Seconds,
    Preset1Minute,
    Preset3Minutes,
    Preset5Minutes,
    Preset15Minutes,
    OpenCustomTime,
    StartFocus,
    SelectHours,
    SelectMinutes,
    SelectSeconds,
    Digit0,
    Digit1,
    Digit2,
    Digit3,
    Digit4,
    Digit5,
    Digit6,
    Digit7,
    Digit8,
    Digit9,
    Clear,
    Delete,
    UseCustomTime,
    Back,
    Pause,
    Resume,
    EndSession,
    KeepSession,
    EndNow,
    EndAlarm,
};

// Draws the time-selection home page with the selected duration.
// 绘制时间选择主页，并显示当前选中的时长。
void pomodoro_page_render_setup(Canvas &canvas, uint32_t selected_seconds);

// Draws the fixed custom-time keypad and highlights the active field.
// 绘制固定自定义时间键盘，并标记当前输入位置。
void pomodoro_page_render_custom_time(Canvas &canvas,
                                      uint8_t hours,
                                      uint8_t minutes,
                                      uint8_t seconds,
                                      PomodoroTimeField active_field);

// Draws either the running or paused timer using a 12-segment progress ring.
// 使用12段圆环绘制运行中或暂停状态。
void pomodoro_page_render_timer(Canvas &canvas,
                                PomodoroPage page,
                                uint32_t remaining_seconds,
                                uint32_t total_seconds);

// Draws the early-end confirmation page with frozen progress.
// 绘制冻结当前进度的提前结束确认页。
void pomodoro_page_render_end_confirmation(Canvas &canvas,
                                           uint32_t remaining_seconds,
                                           uint32_t total_seconds);

// Draws the active alarm page shown after the timer reaches zero.
// 绘制倒计时归零后的持续响铃页面。
void pomodoro_page_render_alarm(Canvas &canvas, uint32_t focused_seconds);

// Maps one logical portrait touch coordinate to the visible page action.
// 将竖屏逻辑触摸坐标映射为页面上的操作。
PomodoroAction pomodoro_page_action_at(PomodoroPage page, int x, int y);

const char *pomodoro_page_name(PomodoroPage page);
const char *pomodoro_action_name(PomodoroAction action);
