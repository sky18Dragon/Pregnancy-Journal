#include "app_pages.h"

#include <cstdio>
#include <cstring>

#include "canvas.h"

namespace {

constexpr int kBorderMargin = 12;

CanvasRotation rotation_for_orientation(StickyImuOrientation orientation)
{
    switch (orientation) {
    case StickyImuOrientation::Portrait0:
        return CanvasRotation::Deg90CounterClockwise;
    case StickyImuOrientation::Landscape180:
        return CanvasRotation::Deg180;
    case StickyImuOrientation::Portrait180:
        return CanvasRotation::Deg90Clockwise;
    case StickyImuOrientation::Landscape0:
    case StickyImuOrientation::FaceUp:
    case StickyImuOrientation::FaceDown:
    case StickyImuOrientation::Unknown:
    default:
        return CanvasRotation::Deg0;
    }
}

const char *page_orientation_name(StickyImuOrientation orientation)
{
    switch (orientation) {
    case StickyImuOrientation::Landscape0:
        return "LANDSCAPE 0";
    case StickyImuOrientation::Landscape180:
        return "LANDSCAPE 180";
    case StickyImuOrientation::Portrait0:
        return "PORTRAIT 0";
    case StickyImuOrientation::Portrait180:
        return "PORTRAIT 180";
    case StickyImuOrientation::FaceUp:
        return "FACE UP";
    case StickyImuOrientation::FaceDown:
        return "FACE DOWN";
    case StickyImuOrientation::Unknown:
    default:
        return "UNKNOWN";
    }
}

int text_width(const char *text, int scale)
{
    return static_cast<int>(std::strlen(text)) * 6 * scale;
}

void draw_centered_text(Canvas &canvas,
                        int y,
                        const char *text,
                        int scale,
                        GrayLevel color = GrayLevel::Black)
{
    const int x = (static_cast<int>(canvas.width()) -
                   text_width(text, scale)) /
                  2;
    canvas.draw_text(x, y, text, scale, color);
}

void begin_page(Canvas &canvas, StickyImuOrientation orientation)
{
    // Applies the matching logical coordinate rotation before every full redraw.
    // 每次整页重绘前都切换到与放稳姿态匹配的逻辑坐标。
    canvas.set_rotation(rotation_for_orientation(orientation));
    canvas.clear(GrayLevel::White);
    canvas.draw_rect(kBorderMargin,
                     kBorderMargin,
                     canvas.width() - 2 * kBorderMargin,
                     canvas.height() - 2 * kBorderMargin,
                     GrayLevel::Black);
}

}  // namespace

void app_page_render_base(Canvas &canvas, StickyImuOrientation orientation)
{
    begin_page(canvas, orientation);
    draw_centered_text(canvas, canvas.height() / 2 - 72, "STICKY HOME", 5);
    draw_centered_text(canvas,
                       canvas.height() / 2 + 4,
                       page_orientation_name(orientation),
                       3);
    draw_centered_text(canvas, canvas.height() - 62, "READY", 2);
}

void app_page_render_pomodoro_confirmation(
    Canvas &canvas,
    StickyImuOrientation orientation)
{
    begin_page(canvas, orientation);
    draw_centered_text(canvas, 80, "POMODORO", 5);
    draw_centered_text(canvas, 210, "15:00", 10);

    const int box_x = 48;
    const int box_y = 470;
    const int box_width = canvas.width() - box_x * 2;
    canvas.fill_rect(box_x, box_y, box_width, 112, GrayLevel::Black);
    draw_centered_text(canvas, box_y + 24, "TAP TO START", 4, GrayLevel::White);
    draw_centered_text(canvas, 625, "10 SECOND WINDOW", 2);
    draw_centered_text(canvas, 690, "NO TAP: HOME", 2);
}

void app_page_render_pomodoro_running(
    Canvas &canvas,
    StickyImuOrientation orientation,
    uint32_t remaining_seconds)
{
    begin_page(canvas, orientation);

    char time_text[16] = {};
    const uint32_t minutes = remaining_seconds / 60U;
    const uint32_t seconds = remaining_seconds % 60U;
    std::snprintf(time_text,
                  sizeof(time_text),
                  "%02lu:%02lu",
                  static_cast<unsigned long>(minutes),
                  static_cast<unsigned long>(seconds));

    const bool portrait_page = canvas.height() > canvas.width();
    const int title_y = portrait_page ? 95 : 70;
    const int timer_y = portrait_page ? 280 : 190;
    const int status_y = portrait_page ? 520 : 340;
    const int status_text_y = portrait_page ? 548 : 366;
    draw_centered_text(canvas, title_y, "FOCUS SESSION", 4);
    draw_centered_text(canvas, timer_y, time_text, 10);
    canvas.fill_rect(48,
                     status_y,
                     canvas.width() - 96,
                     portrait_page ? 100 : 88,
                     GrayLevel::Black);
    draw_centered_text(
        canvas, status_text_y, "POMODORO RUNNING", 3, GrayLevel::White);
}

void app_page_render_pomodoro_done(
    Canvas &canvas,
    StickyImuOrientation orientation)
{
    begin_page(canvas, orientation);
    const bool portrait_page = canvas.height() > canvas.width();
    draw_centered_text(canvas, portrait_page ? 210 : 100, "POMODORO", 5);
    draw_centered_text(canvas, portrait_page ? 330 : 220, "COMPLETE", 6);
    draw_centered_text(
        canvas, portrait_page ? 560 : 360, "FOCUS SESSION DONE", 2);
}
