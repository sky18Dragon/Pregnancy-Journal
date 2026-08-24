#include "battery_status_overlay.h"

#include <cstdio>
#include <cstring>

#include "app_log.h"
#include "battery_status_overlay_theme.h"
#include "board_charger.h"
#include "canvas.h"
#include "esp_timer.h"
#include "sticky_battery.h"

namespace {

constexpr char kTag[] = "battery_overlay";
constexpr int64_t kReadIntervalUs = 60LL * 1000000LL;
constexpr int64_t kRetryIntervalUs = 10LL * 1000000LL;
constexpr int kAreaWidth = 98;
constexpr int kAreaHeight = 28;
constexpr int kAreaTop = 4;
constexpr int kNormalRightMargin = 6;
constexpr int kSleepRightMargin = 55;

bool s_valid = false;
int s_percent = 0;
int s_last_logged_percent = -1;
bool s_last_logged_external_power = false;
bool s_has_logged_external_power = false;
bool s_log_tag_registered = false;
int64_t s_next_read_us = 0;

void update_reading_if_due()
{
    const int64_t now_us = esp_timer_get_time();
    if (now_us < s_next_read_us) {
        return;
    }

    StickyBatteryReading reading = {};
    const esp_err_t result = sticky_battery_read(reading);
    s_valid = result == ESP_OK;
    if (s_valid) {
        s_percent = reading.percent;
    }
    s_next_read_us = now_us +
                     (s_valid ? kReadIntervalUs : kRetryIntervalUs);

    const bool external_power = board_charger_external_power_present();
    if ((s_valid && s_percent != s_last_logged_percent) ||
        !s_has_logged_external_power ||
        external_power != s_last_logged_external_power) {
        STICKY_LOGI(kTag,
                    "battery=display valid=%d percent=%d external_power=%d next_read_s=%u",
                    s_valid,
                    s_percent,
                    external_power,
                    static_cast<unsigned>(
                        (s_valid ? kReadIntervalUs : kRetryIntervalUs) /
                        1000000LL));
        s_last_logged_percent = s_percent;
        s_last_logged_external_power = external_power;
        s_has_logged_external_power = true;
    }
}

void draw_charging_bolt(Canvas &canvas, int x, int y, GrayLevel color)
{
    // Draws a two-pixel-wide bolt that remains legible over the larger fill bar.
    // 绘制两像素宽的闪电，使其在放大的电量填充条上仍然清晰。
    canvas.draw_line(x + 5, y, x, y + 7, color);
    canvas.draw_line(x + 6, y, x + 1, y + 7, color);
    canvas.draw_line(x, y + 7, x + 6, y + 7, color);
    canvas.draw_line(x + 1, y + 8, x + 7, y + 8, color);
    canvas.draw_line(x + 6, y + 7, x + 1, y + 15, color);
    canvas.draw_line(x + 7, y + 8, x + 2, y + 15, color);
}

int overlay_area_x(const Canvas &canvas, bool sleep_layout)
{
    const int right_margin = sleep_layout ? kSleepRightMargin
                                          : kNormalRightMargin;
    return static_cast<int>(canvas.width()) -
           kAreaWidth - right_margin;
}

void clear_overlay_area(Canvas &canvas,
                        bool sleep_layout,
                        GrayLevel background)
{
    canvas.fill_rect(overlay_area_x(canvas, sleep_layout),
                     kAreaTop,
                     kAreaWidth,
                     kAreaHeight,
                     background);
}

}  // namespace

void battery_status_overlay_clear(Canvas &canvas, bool sleep_layout)
{
    const int area_x = overlay_area_x(canvas, sleep_layout);
    const BatteryStatusOverlayTheme theme =
        battery_status_overlay_theme(
            canvas, area_x, kAreaTop, kAreaWidth, kAreaHeight);
    clear_overlay_area(canvas, sleep_layout, theme.background);
}

void battery_status_overlay_draw(Canvas &canvas, bool sleep_layout)
{
    if (!s_log_tag_registered) {
        app_log_register_tag(kTag);
        s_log_tag_registered = true;
    }
    update_reading_if_due();

    const int area_x = overlay_area_x(canvas, sleep_layout);
    const BatteryStatusOverlayTheme theme =
        battery_status_overlay_theme(
            canvas, area_x, kAreaTop, kAreaWidth, kAreaHeight);
    clear_overlay_area(canvas, sleep_layout, theme.background);

    constexpr int kBatteryWidth = 34;
    constexpr int kBatteryHeight = 16;
    const int battery_x = area_x + 1;
    const int battery_y = kAreaTop + 6;
    canvas.draw_rect(battery_x,
                     battery_y,
                     kBatteryWidth,
                     kBatteryHeight,
                     theme.foreground);
    canvas.fill_rect(battery_x + kBatteryWidth,
                     battery_y + 5,
                     4,
                     6,
                     theme.foreground);

    if (s_valid && s_percent > 0) {
        constexpr int kInnerWidth = kBatteryWidth - 4;
        const int fill_width =
            (kInnerWidth * s_percent + 99) / 100;
        canvas.fill_rect(battery_x + 2,
                         battery_y + 2,
                         fill_width,
                         kBatteryHeight - 4,
                         theme.foreground);
    }
    if (board_charger_external_power_present()) {
        draw_charging_bolt(canvas,
                           battery_x + 13,
                           battery_y,
                           s_valid && s_percent >= 50
                               ? theme.background
                               : theme.foreground);
    }

    char label[6] = "--%";
    if (s_valid) {
        std::snprintf(label, sizeof(label), "%d%%", s_percent);
    }
    constexpr int kLabelScale = 2;
    constexpr int kLabelAreaX = 43;
    constexpr int kLabelAreaWidth = kAreaWidth - kLabelAreaX;
    const int label_width = static_cast<int>(std::strlen(label)) *
                            6 * kLabelScale;
    canvas.draw_text(area_x + kLabelAreaX +
                         (kLabelAreaWidth - label_width) / 2,
                     kAreaTop + 7,
                     label,
                     kLabelScale,
                     theme.foreground);
}
