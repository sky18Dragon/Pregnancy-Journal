#include "home_pages.h"

#include <cstdio>

#include "canvas.h"
#include "font.h"
#include "ui_language.h"

namespace {

void draw_centered(Canvas &canvas,
                   int y,
                   const char *text,
                   uint8_t scale,
                   GrayLevel color = GrayLevel::Black)
{
    canvas.draw_text((static_cast<int>(canvas.width()) -
                      ui_text_width(text, scale)) /
                         2,
                     y,
                     text,
                     scale,
                     color);
}

void draw_beveled_box(Canvas &canvas,
                      int x,
                      int y,
                      int width,
                      int height)
{
    constexpr int kCut = 8;
    canvas.draw_line(x + kCut, y, x + width - kCut - 1, y);
    canvas.draw_line(x + width - kCut - 1, y,
                     x + width - 1, y + kCut);
    canvas.draw_line(x + width - 1, y + kCut,
                     x + width - 1, y + height - kCut - 1);
    canvas.draw_line(x + width - 1, y + height - kCut - 1,
                     x + width - kCut - 1, y + height - 1);
    canvas.draw_line(x + width - kCut - 1, y + height - 1,
                     x + kCut, y + height - 1);
    canvas.draw_line(x + kCut, y + height - 1,
                     x, y + height - kCut - 1);
    canvas.draw_line(x, y + height - kCut - 1, x, y + kCut);
    canvas.draw_line(x, y + kCut, x + kCut, y);
}

}  // namespace

void home_page_render(Canvas &canvas, const HomePageData &data)
{
    canvas.set_rotation(CanvasRotation::Deg0);
    canvas.clear(GrayLevel::White);

    canvas.draw_text(32, 28, "STICKY CORE", 4);
    canvas.draw_text(32, 62, "FRAMEWORK", 2);
    canvas.draw_line(32, 92, 767, 92, GrayLevel::Black);

    char time[8] = "--:--";
    char date[16] = "---- -- --";
    if (data.rtc_valid) {
        std::snprintf(time,
                      sizeof(time),
                      "%02u:%02u",
                      static_cast<unsigned>(data.hour),
                      static_cast<unsigned>(data.minute));
        std::snprintf(date,
                      sizeof(date),
                      "%04u-%02u-%02u",
                      static_cast<unsigned>(data.year),
                      static_cast<unsigned>(data.month),
                      static_cast<unsigned>(data.day));
    }
    draw_centered(canvas, 122, time, 10);
    draw_centered(canvas, 220, date, 4);

    constexpr int kStatusY = 288;
    constexpr int kStatusHeight = 62;
    draw_beveled_box(canvas, 98, kStatusY, 280, kStatusHeight);
    draw_beveled_box(canvas, 422, kStatusY, 280, kStatusHeight);

    char battery[32] = {};
    if (ui_language_is_chinese()) {
        std::snprintf(battery,
                      sizeof(battery),
                      data.battery_valid ? "电量  %d%%" : "电量  --",
                      data.battery_percent);
    } else {
        std::snprintf(battery,
                      sizeof(battery),
                      data.battery_valid ? "BATTERY  %d%%" : "BATTERY  --",
                      data.battery_percent);
    }
    canvas.draw_text(124, kStatusY + 20, battery, 3);
    canvas.draw_text(478,
                     kStatusY + 20,
                     data.rtc_valid ? "RTC  READY" : "RTC  UNAVAILABLE",
                     3);

    draw_centered(canvas, 402, "SWIPE UP FOR APPS", 3);
}
