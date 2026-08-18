#include "pomodoro_pages.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "canvas.h"

namespace {

constexpr float kPi = 3.14159265358979323846F;
constexpr int kScreenWidth = 480;
constexpr int kScreenHeight = 800;
constexpr int kActionButtonX = 40;
constexpr int kActionButtonWidth = 400;
constexpr int kActionButtonHeight = 64;
constexpr int kPresetHighlightInsetX = 12;
constexpr int kPresetHighlightTopExtension = 2;

struct Rect {
    int x;
    int y;
    int width;
    int height;

    bool contains(int point_x, int point_y) const
    {
        return point_x >= x && point_y >= y &&
               point_x < x + width && point_y < y + height;
    }
};

constexpr Rect kSetupPresetRects[] = {
    {25, 515, 135, 38},
    {172, 515, 135, 38},
    {320, 515, 135, 38},
    {25, 559, 135, 38},
    {172, 559, 135, 38},
    {320, 559, 135, 38},
};
constexpr Rect kCustomTimeRect = {
    kActionButtonX, 615, kActionButtonWidth, kActionButtonHeight};
constexpr Rect kStartRect = {
    kActionButtonX, 696, kActionButtonWidth, kActionButtonHeight};

constexpr Rect kTimeFieldRects[] = {
    {90, 205, 90, 135},
    {195, 205, 90, 135},
    {300, 205, 90, 135},
};
constexpr Rect kKeypadRects[] = {
    {40, 450, 125, 46},
    {178, 450, 125, 46},
    {315, 450, 125, 46},
    {40, 500, 125, 46},
    {178, 500, 125, 46},
    {315, 500, 125, 46},
    {40, 550, 125, 46},
    {178, 550, 125, 46},
    {315, 550, 125, 46},
    {40, 600, 125, 46},
    {178, 600, 125, 46},
    {315, 600, 125, 46},
};
constexpr Rect kUseCustomRect = {
    kActionButtonX, 665, kActionButtonWidth, kActionButtonHeight};
constexpr Rect kBackRect = {140, 738, 200, 50};

// Full-width action buttons share one size across every Pomodoro page.
// 所有番茄钟页面的通栏操作按钮统一使用同一尺寸。
constexpr Rect kPrimaryTimerRect = {
    kActionButtonX, 600, kActionButtonWidth, kActionButtonHeight};
constexpr Rect kSecondaryTimerRect = {80, 680, 320, 60};
constexpr Rect kKeepSessionRect = {
    kActionButtonX, 620, kActionButtonWidth, kActionButtonHeight};
constexpr Rect kEndNowRect = {
    kActionButtonX, 704, kActionButtonWidth, kActionButtonHeight};
constexpr Rect kEndAlarmRect = {
    kActionButtonX, 696, kActionButtonWidth, kActionButtonHeight};

int text_width(const char *text, int scale)
{
    const size_t length = std::strlen(text);
    if (length == 0U) {
        return 0;
    }
    return (static_cast<int>(length) * 6 - 1) * scale;
}

void draw_centered_text(Canvas &canvas,
                        int y,
                        const char *text,
                        int scale,
                        GrayLevel color = GrayLevel::Black)
{
    canvas.draw_text((static_cast<int>(canvas.width()) -
                      text_width(text, scale)) /
                         2,
                     y,
                     text,
                     scale,
                     color);
}

void draw_centered_text_in_rect(Canvas &canvas,
                                const Rect &rect,
                                const char *text,
                                int scale,
                                GrayLevel color)
{
    const int x = rect.x + (rect.width - text_width(text, scale)) / 2;
    const int y = rect.y + (rect.height - 7 * scale) / 2;
    canvas.draw_text(x, y, text, scale, color);
}

void begin_page(Canvas &canvas)
{
    // The standalone app always uses the calibrated upright portrait surface.
    // 独立APP固定使用已标定的正向竖屏坐标。
    canvas.set_rotation(CanvasRotation::Deg90CounterClockwise);
    canvas.clear(GrayLevel::White);
}

void draw_tomato_mark(Canvas &canvas)
{
    canvas.fill_circle(240, 35, 14, GrayLevel::Black);
    canvas.draw_line(240, 17, 240, 25, GrayLevel::Black);
    canvas.draw_line(240, 22, 233, 18, GrayLevel::Black);
    canvas.draw_line(240, 22, 247, 18, GrayLevel::Black);
    canvas.fill_circle(245, 31, 2, GrayLevel::White);
    canvas.fill_circle(247, 38, 2, GrayLevel::White);
}

void draw_button(Canvas &canvas,
                 const Rect &rect,
                 const char *label,
                 bool filled,
                 int scale = 3)
{
    if (filled) {
        canvas.fill_rect(rect.x,
                         rect.y,
                         rect.width,
                         rect.height,
                         GrayLevel::Black);
        draw_centered_text_in_rect(
            canvas, rect, label, scale, GrayLevel::White);
        return;
    }

    canvas.draw_rect(rect.x,
                     rect.y,
                     rect.width,
                     rect.height,
                     GrayLevel::Black);
    canvas.draw_rect(rect.x + 1,
                     rect.y + 1,
                     rect.width - 2,
                     rect.height - 2,
                     GrayLevel::Black);
    draw_centered_text_in_rect(canvas, rect, label, scale, GrayLevel::Black);
}

void point_on_circle(int center_x,
                     int center_y,
                     int radius,
                     float degrees,
                     int &x,
                     int &y)
{
    const float radians = degrees * kPi / 180.0F;
    x = center_x + static_cast<int>(std::lround(std::cos(radians) * radius));
    y = center_y + static_cast<int>(std::lround(std::sin(radians) * radius));
}

void draw_arc(Canvas &canvas,
              int center_x,
              int center_y,
              int radius,
              float start_degrees,
              float end_degrees,
              bool strong)
{
    // Draws either a solid black arc or a sparse monochrome arc.
    // 绘制黑色实线圆弧或稀疏黑白圆弧。
    const int step = strong ? 1 : 4;
    const int dot_radius = strong ? 5 : 1;
    for (int degree = static_cast<int>(std::ceil(start_degrees));
         degree <= static_cast<int>(std::floor(end_degrees));
         degree += step) {
        int x = 0;
        int y = 0;
        point_on_circle(center_x,
                        center_y,
                        radius,
                        static_cast<float>(degree),
                        x,
                        y);
        canvas.fill_circle(x, y, dot_radius, GrayLevel::Black);
    }
}

void draw_split_ring(Canvas &canvas, int center_y, int radius)
{
    draw_arc(canvas, 240, center_y, radius, -88.0F, 88.0F, true);
    draw_arc(canvas, 240, center_y, radius, 92.0F, 268.0F, false);
}

void draw_segmented_ring(Canvas &canvas,
                         int center_y,
                         int radius,
                         uint8_t active_segments)
{
    for (uint8_t index = 0; index < 12; ++index) {
        const float start = -88.0F + static_cast<float>(index) * 30.0F;
        const float end = start + 23.0F;
        draw_arc(canvas,
                 240,
                 center_y,
                 radius,
                 start,
                 end,
                 index < active_segments);
    }
}

void format_duration(uint32_t seconds, char *buffer, size_t buffer_size)
{
    const uint32_t hours = seconds / 3600U;
    const uint32_t minutes = (seconds % 3600U) / 60U;
    const uint32_t remainder = seconds % 60U;
    if (hours > 0U) {
        std::snprintf(buffer,
                      buffer_size,
                      "%02lu:%02lu:%02lu",
                      static_cast<unsigned long>(hours),
                      static_cast<unsigned long>(minutes),
                      static_cast<unsigned long>(remainder));
    } else {
        std::snprintf(buffer,
                      buffer_size,
                      "%02lu:%02lu",
                      static_cast<unsigned long>(minutes),
                      static_cast<unsigned long>(remainder));
    }
}

uint8_t ring_segments(uint32_t remaining_seconds, uint32_t total_seconds)
{
    if (remaining_seconds == 0U || total_seconds == 0U) {
        return 0;
    }
    const uint32_t scaled = remaining_seconds * 12U + total_seconds - 1U;
    return static_cast<uint8_t>(std::min<uint32_t>(12U, scaled / total_seconds));
}

void draw_timer_content(Canvas &canvas,
                        const char *title,
                        uint32_t remaining_seconds,
                        uint32_t total_seconds)
{
    draw_tomato_mark(canvas);
    draw_centered_text(canvas, 70, title, 4);
    draw_segmented_ring(canvas,
                        330,
                        205,
                        ring_segments(remaining_seconds, total_seconds));

    char time_text[16] = {};
    format_duration(remaining_seconds, time_text, sizeof(time_text));
    const int scale = std::strlen(time_text) > 5U ? 5 : 10;
    constexpr int kCaptionScale = 3;
    constexpr int kTimeCaptionGap = 18;
    // Centers the time and its caption as one visual block inside the ring.
    // 将时间和说明文字作为一个整体放在圆环正中央。
    const int content_height = 7 * scale + kTimeCaptionGap +
                               7 * kCaptionScale;
    const int time_y = 330 - content_height / 2;
    draw_centered_text(canvas, time_y, time_text, scale);
    draw_centered_text(canvas,
                       time_y + 7 * scale + kTimeCaptionGap,
                       "TIME LEFT",
                       kCaptionScale);
}

}  // namespace

void pomodoro_page_render_setup(Canvas &canvas, uint32_t selected_seconds)
{
    begin_page(canvas);
    draw_tomato_mark(canvas);
    draw_centered_text(canvas, 70, "CHOOSE A FOCUS TIME", 3);
    draw_split_ring(canvas, 300, 180);

    char time_text[16] = {};
    format_duration(selected_seconds, time_text, sizeof(time_text));
    const int scale = std::strlen(time_text) > 5U ? 6 : 11;
    constexpr int kSelectedScale = 2;
    constexpr int kTimeSelectedGap = 10;
    // Centers the selected duration and caption as one visual block.
    // 将选中时间和说明文字作为一个整体居中显示。
    const int content_height = 7 * scale + kTimeSelectedGap +
                               7 * kSelectedScale;
    const int time_y = 300 - content_height / 2;
    draw_centered_text(canvas, time_y, time_text, scale);
    draw_centered_text(canvas,
                       time_y + 7 * scale + kTimeSelectedGap,
                       "SELECTED",
                       kSelectedScale);

    constexpr const char *kPresetTop[] = {"10", "30", "1", "3", "5", "15"};
    constexpr const char *kPresetBottom[] = {
        "SEC", "SEC", "MIN", "MIN", "MIN", "MIN",
    };
    constexpr uint32_t kPresetSeconds[] = {10, 30, 60, 180, 300, 900};
    for (int index = 0; index < 6; ++index) {
        const Rect &rect = kSetupPresetRects[index];
        if (selected_seconds == kPresetSeconds[index]) {
            canvas.fill_rect(rect.x + kPresetHighlightInsetX,
                             rect.y - kPresetHighlightTopExtension,
                             rect.width - 2 * kPresetHighlightInsetX,
                             rect.height + kPresetHighlightTopExtension,
                             GrayLevel::Black);
        }
        const GrayLevel color = selected_seconds == kPresetSeconds[index]
                                    ? GrayLevel::White
                                    : GrayLevel::Black;
        const int number_scale = 3;
        const int unit_scale = 1;
        constexpr int kNumberUnitGap = 4;
        const int number_width = text_width(kPresetTop[index], number_scale);
        const int unit_width = text_width(kPresetBottom[index], unit_scale);
        const int label_width = number_width + kNumberUnitGap + unit_width;
        const int label_x = rect.x + (rect.width - label_width) / 2;
        const int number_y = rect.y + 7;
        // Places the smaller unit at the lower-right corner of the number.
        // 将较小的单位放在数字右下角，并将两者作为整体居中。
        canvas.draw_text(label_x,
                         number_y,
                         kPresetTop[index],
                         number_scale,
                         color);
        canvas.draw_text(label_x + number_width + kNumberUnitGap,
                         number_y + 14,
                         kPresetBottom[index],
                         unit_scale,
                         color);
    }

    draw_button(canvas, kCustomTimeRect, "CUSTOM TIME +", false, 4);
    draw_button(canvas, kStartRect, "START FOCUS", true, 4);
}

void pomodoro_page_render_custom_time(Canvas &canvas,
                                      uint8_t hours,
                                      uint8_t minutes,
                                      uint8_t seconds,
                                      PomodoroTimeField active_field)
{
    begin_page(canvas);
    draw_tomato_mark(canvas);
    draw_centered_text(canvas, 65, "CUSTOM TIME", 3);
    draw_segmented_ring(canvas, 270, 165, 6);

    char time_text[16] = {};
    std::snprintf(time_text,
                  sizeof(time_text),
                  "%02u:%02u:%02u",
                  static_cast<unsigned>(hours),
                  static_cast<unsigned>(minutes),
                  static_cast<unsigned>(seconds));
    draw_centered_text(canvas, 235, time_text, 5);
    canvas.draw_text(123, 300, "HR", 2);
    canvas.draw_text(222, 300, "MIN", 2);
    canvas.draw_text(324, 300, "SEC", 2);

    const int active_index = active_field == PomodoroTimeField::Hours
                                 ? 0
                                 : active_field == PomodoroTimeField::Minutes ? 1 : 2;
    canvas.fill_rect(kTimeFieldRects[active_index].x + 14,
                     335,
                     kTimeFieldRects[active_index].width - 28,
                     5,
                     GrayLevel::Black);

    constexpr const char *kKeyLabels[] = {
        "1", "2", "3", "4", "5", "6",
        "7", "8", "9", "CLEAR", "0", "DELETE",
    };
    for (int index = 0; index < 12; ++index) {
        const Rect &rect = kKeypadRects[index];
        canvas.draw_line(rect.x,
                         rect.y + rect.height - 1,
                         rect.x + rect.width - 1,
                         rect.y + rect.height - 1,
                         GrayLevel::Black);
        draw_centered_text_in_rect(canvas,
                                   rect,
                                   kKeyLabels[index],
                                   index == 9 || index == 11 ? 2 : 4,
                                   GrayLevel::Black);
    }

    draw_button(canvas, kUseCustomRect, "USE THIS TIME", true, 4);
    draw_centered_text_in_rect(
        canvas, kBackRect, "BACK", 3, GrayLevel::Black);
}

void pomodoro_page_render_timer(Canvas &canvas,
                                PomodoroPage page,
                                uint32_t remaining_seconds,
                                uint32_t total_seconds)
{
    begin_page(canvas);
    const bool paused = page == PomodoroPage::Paused;
    draw_timer_content(canvas,
                       paused ? "PAUSED" : "FOCUSING",
                       remaining_seconds,
                       total_seconds);
    draw_button(canvas,
                kPrimaryTimerRect,
                paused ? "RESUME" : "PAUSE",
                true,
                4);
    draw_centered_text_in_rect(
        canvas, kSecondaryTimerRect, "END SESSION", 3, GrayLevel::Black);
}

void pomodoro_page_render_end_confirmation(Canvas &canvas,
                                           uint32_t remaining_seconds,
                                           uint32_t total_seconds)
{
    begin_page(canvas);
    draw_timer_content(
        canvas, "END SESSION?", remaining_seconds, total_seconds);
    draw_centered_text(canvas, 570, "YOUR PROGRESS WILL END.", 2);
    draw_button(canvas, kKeepSessionRect, "KEEP SESSION", true, 4);
    draw_button(canvas, kEndNowRect, "END NOW", false, 4);
}

void pomodoro_page_render_alarm(Canvas &canvas, uint32_t focused_seconds)
{
    begin_page(canvas);
    draw_tomato_mark(canvas);
    draw_centered_text(canvas, 78, "TIME'S UP", 4);
    draw_segmented_ring(canvas, 315, 168, 0);
    draw_centered_text(canvas, 280, "00:00", 8);
    draw_centered_text(canvas, 362, "ALARM SOUNDING", 3);

    char focused_text[32] = {};
    if (focused_seconds % 60U == 0U) {
        std::snprintf(focused_text,
                      sizeof(focused_text),
                      "%lu MIN FOCUSED",
                      static_cast<unsigned long>(focused_seconds / 60U));
    } else {
        std::snprintf(focused_text,
                      sizeof(focused_text),
                      "%lu SEC FOCUSED",
                      static_cast<unsigned long>(focused_seconds));
    }
    draw_centered_text(canvas, 565, focused_text, 2);
    draw_button(canvas, kEndAlarmRect, "END", true, 4);
}

PomodoroAction pomodoro_page_action_at(PomodoroPage page, int x, int y)
{
    if (x < 0 || y < 0 || x >= kScreenWidth || y >= kScreenHeight) {
        return PomodoroAction::None;
    }

    if (page == PomodoroPage::Setup) {
        constexpr PomodoroAction kPresetActions[] = {
            PomodoroAction::Preset10Seconds,
            PomodoroAction::Preset30Seconds,
            PomodoroAction::Preset1Minute,
            PomodoroAction::Preset3Minutes,
            PomodoroAction::Preset5Minutes,
            PomodoroAction::Preset15Minutes,
        };
        for (int index = 0; index < 6; ++index) {
            if (kSetupPresetRects[index].contains(x, y)) {
                return kPresetActions[index];
            }
        }
        if (kCustomTimeRect.contains(x, y)) {
            return PomodoroAction::OpenCustomTime;
        }
        if (kStartRect.contains(x, y)) {
            return PomodoroAction::StartFocus;
        }
    } else if (page == PomodoroPage::CustomTime) {
        if (kTimeFieldRects[0].contains(x, y)) {
            return PomodoroAction::SelectHours;
        }
        if (kTimeFieldRects[1].contains(x, y)) {
            return PomodoroAction::SelectMinutes;
        }
        if (kTimeFieldRects[2].contains(x, y)) {
            return PomodoroAction::SelectSeconds;
        }

        constexpr PomodoroAction kKeyActions[] = {
            PomodoroAction::Digit1,
            PomodoroAction::Digit2,
            PomodoroAction::Digit3,
            PomodoroAction::Digit4,
            PomodoroAction::Digit5,
            PomodoroAction::Digit6,
            PomodoroAction::Digit7,
            PomodoroAction::Digit8,
            PomodoroAction::Digit9,
            PomodoroAction::Clear,
            PomodoroAction::Digit0,
            PomodoroAction::Delete,
        };
        for (int index = 0; index < 12; ++index) {
            if (kKeypadRects[index].contains(x, y)) {
                return kKeyActions[index];
            }
        }
        if (kUseCustomRect.contains(x, y)) {
            return PomodoroAction::UseCustomTime;
        }
        if (kBackRect.contains(x, y)) {
            return PomodoroAction::Back;
        }
    } else if (page == PomodoroPage::Running) {
        if (kPrimaryTimerRect.contains(x, y)) {
            return PomodoroAction::Pause;
        }
        if (kSecondaryTimerRect.contains(x, y)) {
            return PomodoroAction::EndSession;
        }
    } else if (page == PomodoroPage::Paused) {
        if (kPrimaryTimerRect.contains(x, y)) {
            return PomodoroAction::Resume;
        }
        if (kSecondaryTimerRect.contains(x, y)) {
            return PomodoroAction::EndSession;
        }
    } else if (page == PomodoroPage::EndConfirmation) {
        if (kKeepSessionRect.contains(x, y)) {
            return PomodoroAction::KeepSession;
        }
        if (kEndNowRect.contains(x, y)) {
            return PomodoroAction::EndNow;
        }
    } else if (page == PomodoroPage::Alarm &&
               kEndAlarmRect.contains(x, y)) {
        return PomodoroAction::EndAlarm;
    }

    return PomodoroAction::None;
}

const char *pomodoro_page_name(PomodoroPage page)
{
    switch (page) {
    case PomodoroPage::Setup:
        return "setup";
    case PomodoroPage::CustomTime:
        return "custom_time";
    case PomodoroPage::Running:
        return "running";
    case PomodoroPage::Paused:
        return "paused";
    case PomodoroPage::EndConfirmation:
        return "end_confirmation";
    case PomodoroPage::Alarm:
        return "alarm";
    default:
        return "unknown";
    }
}

const char *pomodoro_action_name(PomodoroAction action)
{
    switch (action) {
    case PomodoroAction::Preset10Seconds:
        return "preset_10_seconds";
    case PomodoroAction::Preset30Seconds:
        return "preset_30_seconds";
    case PomodoroAction::Preset1Minute:
        return "preset_1_minute";
    case PomodoroAction::Preset3Minutes:
        return "preset_3_minutes";
    case PomodoroAction::Preset5Minutes:
        return "preset_5_minutes";
    case PomodoroAction::Preset15Minutes:
        return "preset_15_minutes";
    case PomodoroAction::OpenCustomTime:
        return "open_custom_time";
    case PomodoroAction::StartFocus:
        return "start_focus";
    case PomodoroAction::SelectHours:
        return "select_hours";
    case PomodoroAction::SelectMinutes:
        return "select_minutes";
    case PomodoroAction::SelectSeconds:
        return "select_seconds";
    case PomodoroAction::Digit0:
    case PomodoroAction::Digit1:
    case PomodoroAction::Digit2:
    case PomodoroAction::Digit3:
    case PomodoroAction::Digit4:
    case PomodoroAction::Digit5:
    case PomodoroAction::Digit6:
    case PomodoroAction::Digit7:
    case PomodoroAction::Digit8:
    case PomodoroAction::Digit9:
        return "digit";
    case PomodoroAction::Clear:
        return "clear";
    case PomodoroAction::Delete:
        return "delete";
    case PomodoroAction::UseCustomTime:
        return "use_custom_time";
    case PomodoroAction::Back:
        return "back";
    case PomodoroAction::Pause:
        return "pause";
    case PomodoroAction::Resume:
        return "resume";
    case PomodoroAction::EndSession:
        return "end_session";
    case PomodoroAction::KeepSession:
        return "keep_session";
    case PomodoroAction::EndNow:
        return "end_now";
    case PomodoroAction::EndAlarm:
        return "end_alarm";
    case PomodoroAction::None:
    default:
        return "none";
    }
}
