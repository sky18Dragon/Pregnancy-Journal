#include "battery_status_overlay.h"

#include <cstdio>
#include <cstring>

#include "app_log.h"
#include "battery_status_overlay_theme.h"
#include "board_charger.h"
#include "canvas.h"
#include "esp_timer.h"
#include "font.h"
#include "prototype_ui.h"
#include "status_glyphs.h"
#include "sticky_battery.h"

static_assert(kBatteryStatusCompactWidth == kBatteryIconWidth &&
                  kBatteryStatusCompactHeight == kBatteryIconHeight,
              "pages lay out around the compact indicator using these constants");

namespace {

constexpr char kTag[] = "battery_overlay";
constexpr int64_t kReadIntervalUs = 60LL * 1000000LL;
constexpr int64_t kRetryIntervalUs = 10LL * 1000000LL;
constexpr int kAreaWidth = 98;
constexpr int kAreaHeight = 28;
constexpr int kAreaTop = 4;
constexpr int kNormalRightMargin = 6;

bool s_valid = false;
int s_percent = 0;
int s_last_logged_percent = -1;
bool s_last_logged_external_power = false;
bool s_has_logged_external_power = false;
bool s_log_tag_registered = false;
int64_t s_next_read_us = 0;

void update_reading_if_due() {
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
  s_next_read_us = now_us + (s_valid ? kReadIntervalUs : kRetryIntervalUs);

  const bool external_power = board_charger_external_power_present();
  if ((s_valid && s_percent != s_last_logged_percent) ||
      !s_has_logged_external_power ||
      external_power != s_last_logged_external_power) {
    STICKY_LOGI(
        kTag,
        "battery=display valid=%d percent=%d external_power=%d next_read_s=%u",
        s_valid, s_percent, external_power,
        static_cast<unsigned>((s_valid ? kReadIntervalUs : kRetryIntervalUs) /
                              1000000LL));
    s_last_logged_percent = s_percent;
    s_last_logged_external_power = external_power;
    s_has_logged_external_power = true;
  }
}

int overlay_area_x(const Canvas &canvas) {
  return static_cast<int>(canvas.width()) - kAreaWidth - kNormalRightMargin;
}

void clear_overlay_area(Canvas &canvas, GrayLevel background) {
  canvas.fill_rect(overlay_area_x(canvas), kAreaTop, kAreaWidth, kAreaHeight,
                   background);
}

// In the sleep frame the status takes the page's header action button slot.
prototype_ui::Rect sleep_slot(const Canvas &canvas) {
  return prototype_ui::header_action_rect(static_cast<int>(canvas.width()),
                                          static_cast<int>(canvas.height()));
}

BatteryStatusOverlayTheme slot_theme(const Canvas &canvas, const prototype_ui::Rect &slot) {
  return battery_status_overlay_theme(canvas, slot.x, slot.y, slot.width, slot.height);
}

} // namespace

void battery_status_overlay_clear(Canvas &canvas, bool sleep_layout) {
  if (sleep_layout) {
    const prototype_ui::Rect slot = sleep_slot(canvas);
    canvas.fill_rect(slot.x, slot.y, slot.width, slot.height, slot_theme(canvas, slot).background);
    return;
  }
  const int area_x = overlay_area_x(canvas);
  const BatteryStatusOverlayTheme theme = battery_status_overlay_theme(
      canvas, area_x, kAreaTop, kAreaWidth, kAreaHeight);
  clear_overlay_area(canvas, theme.background);
}

void battery_status_overlay_draw(Canvas &canvas, bool sleep_layout) {
  if (!s_log_tag_registered) {
    app_log_register_tag(kTag);
    s_log_tag_registered = true;
  }
  update_reading_if_due();

  if (sleep_layout) {
    // Icon only, no number: the fill shows the charge. It replaces the header button, which cannot
    // be pressed while asleep, so it never overlaps the page's own controls.
    const prototype_ui::Rect slot = sleep_slot(canvas);
    const BatteryStatusOverlayTheme slot_colors = slot_theme(canvas, slot);
    sleep_status_draw(canvas, slot.x, slot.y, slot.width, slot.height, s_valid ? s_percent : -1,
                      board_charger_external_power_present(), slot_colors.foreground,
                      slot_colors.background);
    return;
  }

  const int area_x = overlay_area_x(canvas);
  const BatteryStatusOverlayTheme theme = battery_status_overlay_theme(
      canvas, area_x, kAreaTop, kAreaWidth, kAreaHeight);
  clear_overlay_area(canvas, theme.background);

  constexpr int kBatteryWidth = 34;
  constexpr int kBatteryHeight = 16;
  const int battery_x = area_x + 1;
  const int battery_y = kAreaTop + 6;
  canvas.draw_rect(battery_x, battery_y, kBatteryWidth, kBatteryHeight,
                   theme.foreground);
  canvas.fill_rect(battery_x + kBatteryWidth, battery_y + 5, 4, 6,
                   theme.foreground);

  if (s_valid && s_percent > 0) {
    constexpr int kInnerWidth = kBatteryWidth - 4;
    const int fill_width = (kInnerWidth * s_percent + 99) / 100;
    canvas.fill_rect(battery_x + 2, battery_y + 2, fill_width,
                     kBatteryHeight - 4, theme.foreground);
  }
  if (board_charger_external_power_present()) {
    battery_bolt_draw(canvas, battery_x + 13, battery_y,
                       s_valid && s_percent >= 50 ? theme.background
                                                  : theme.foreground);
  }

  char label[6] = "--%";
  if (s_valid) {
    std::snprintf(label, sizeof(label), "%d%%", s_percent);
  }
  constexpr int kLabelScale = 2;
  constexpr int kLabelAreaX = 43;
  constexpr int kLabelAreaWidth = kAreaWidth - kLabelAreaX;
  const int label_width = font_text_width(label, kLabelScale);
  canvas.draw_text(area_x + kLabelAreaX + (kLabelAreaWidth - label_width) / 2,
                   kAreaTop + 7, label, kLabelScale, theme.foreground);
}

void battery_status_overlay_draw_compact(Canvas &canvas, int x, int y) {
  update_reading_if_due();
  // No number: the fill alone shows what is left (see status_glyphs.h).
  battery_icon_draw(canvas, x, y, s_valid ? s_percent : -1, board_charger_external_power_present(),
                    GrayLevel::Black, GrayLevel::White);
}
