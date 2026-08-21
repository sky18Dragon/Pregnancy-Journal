#include "battery_status_overlay.h"

#include <cstdio>

#include "app_log.h"
#include "board_charger.h"
#include "canvas.h"
#include "esp_timer.h"
#include "sticky_battery.h"

namespace {

constexpr char kTag[] = "battery_overlay";
constexpr int64_t kReadIntervalUs = 60LL * 1000000LL;
constexpr int64_t kRetryIntervalUs = 10LL * 1000000LL;

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
    canvas.draw_line(x + 3, y, x, y + 5, color);
    canvas.draw_line(x, y + 5, x + 4, y + 5, color);
    canvas.draw_line(x + 4, y + 5, x + 1, y + 10, color);
}

}  // namespace

void battery_status_overlay_draw(Canvas &canvas)
{
    if (!s_log_tag_registered) {
        app_log_register_tag(kTag);
        s_log_tag_registered = true;
    }
    update_reading_if_due();

    constexpr int kAreaWidth = 68;
    constexpr int kAreaHeight = 20;
    const int area_x = static_cast<int>(canvas.width()) - kAreaWidth - 6;
    constexpr int area_y = 4;
    canvas.fill_rect(area_x,
                     area_y,
                     kAreaWidth,
                     kAreaHeight,
                     GrayLevel::White);

    constexpr int kBatteryWidth = 28;
    constexpr int kBatteryHeight = 12;
    const int battery_x = area_x + 1;
    const int battery_y = area_y + 4;
    canvas.draw_rect(battery_x,
                     battery_y,
                     kBatteryWidth,
                     kBatteryHeight,
                     GrayLevel::Black);
    canvas.fill_rect(battery_x + kBatteryWidth,
                     battery_y + 4,
                     3,
                     4,
                     GrayLevel::Black);

    if (s_valid && s_percent > 0) {
        constexpr int kInnerWidth = kBatteryWidth - 4;
        const int fill_width =
            (kInnerWidth * s_percent + 99) / 100;
        canvas.fill_rect(battery_x + 2,
                         battery_y + 2,
                         fill_width,
                         kBatteryHeight - 4,
                         GrayLevel::Black);
    }
    if (board_charger_external_power_present()) {
        draw_charging_bolt(canvas,
                           battery_x + 11,
                           battery_y + 1,
                           s_valid && s_percent >= 50
                               ? GrayLevel::White
                               : GrayLevel::Black);
    }

    char label[6] = "--%";
    if (s_valid) {
        std::snprintf(label, sizeof(label), "%d%%", s_percent);
    }
    canvas.draw_text(area_x + 37,
                     area_y + 7,
                     label,
                     1,
                     GrayLevel::Black);
}
