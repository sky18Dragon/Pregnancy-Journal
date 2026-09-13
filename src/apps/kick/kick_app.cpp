#include "kick_app.h"

#include <cstdio>

#include "app_log.h"
#include "canvas.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "kick_service.h"
#include "kick_storage.h"
#include "sticky_app_lifecycle.h"
#include "sticky_display.h"
#include "sticky_rtc.h"
#include "sticky_touch.h"
#include "ui_language.h"

namespace {
constexpr char kTag[] = "KICKS";
constexpr int64_t kDebounceUs = 5LL * 1000000LL;
constexpr int64_t kMaximumSessionUs = 60LL * 60LL * 1000000LL;
Canvas *s_canvas = nullptr;
TaskHandle_t s_task = nullptr;
StickyAppLifecycle s_lifecycle = {};
KickService s_service;
PregnancyDate s_today = {};
uint8_t s_hour = 0U;
uint8_t s_minute = 0U;
KickPeriod s_period = KickPeriod::Morning;
bool s_counting = false;
uint16_t s_count = 0U;
int64_t s_started_us = 0;
int64_t s_last_kick_us = 0;

void box(int x, int y, int width, int height, bool filled = false) {
  if (filled)
    s_canvas->fill_rect(x, y, width, height, GrayLevel::Black);
  else
    s_canvas->draw_rect(x, y, width, height, GrayLevel::Black);
}

void button(int x, int y, int width, int height, const char *label, bool filled,
            uint8_t scale = 2U) {
  box(x, y, width, height, filled);
  const GrayLevel color = filled ? GrayLevel::White : GrayLevel::Black;
  s_canvas->draw_text(x + (width - ui_text_width(label, scale)) / 2,
                      y + (height - 7 * scale) / 2, ui_text(label), scale,
                      color);
}

bool read_clock() {
  StickyRtcDateTime value = {};
  if (sticky_rtc_read(value) != ESP_OK)
    return false;
  s_today = {value.year, value.month, value.day};
  s_hour = value.hour;
  s_minute = value.minute;
  return true;
}

KickPeriod inferred_period() {
  if (s_hour < 12U)
    return KickPeriod::Morning;
  if (s_hour < 18U)
    return KickPeriod::Afternoon;
  return KickPeriod::Evening;
}

void render_dashboard() {
  s_canvas->set_rotation(CanvasRotation::Deg0);
  s_canvas->clear(GrayLevel::White);
  s_canvas->draw_text(32, 24, ui_text("KICK COUNTER"), 5, GrayLevel::Black);
  s_canvas->draw_text(32, 76, ui_text("TODAY'S THREE CHECK-INS"), 2,
                      GrayLevel::DarkGray);
  KickDaySummary summary = {};
  const bool has_data = s_service.summarize(s_today, summary);
  const uint16_t values[] = {summary.morning, summary.afternoon,
                             summary.evening};
  const char *labels[] = {"MORNING", "AFTERNOON", "EVENING"};
  for (size_t i = 0U; i < 3U; ++i) {
    const int x = 32 + static_cast<int>(i) * 248;
    box(x, 116, 224, 126, false);
    s_canvas->draw_text(x + 16, 134, ui_text(labels[i]), 2,
                        GrayLevel::DarkGray);
    char count[12] = {};
    std::snprintf(count, sizeof(count), "%u", values[i]);
    s_canvas->draw_text(x + 16, 172, count, 6, GrayLevel::Black);
  }
  box(32, 266, 736, 94, false);
  if (!has_data) {
    s_canvas->draw_text(52, 296, ui_text("NO KICK SESSIONS TODAY"), 3,
                        GrayLevel::Black);
  } else {
    char result[80] = {};
    std::snprintf(result, sizeof(result),
                  ui_language_is_chinese() ? "采样 %u 次   12小时估算 %u 次"
                                           : "SAMPLED %u   EST. 12H %u",
                  summary.sampled_total, summary.estimated_12h);
    s_canvas->draw_text(52, 288, result, 3, GrayLevel::Black);
    s_canvas->draw_text(52, 328,
                        ui_text(summary.attention ? "CONTACT YOUR CARE TEAM"
                                : summary.complete
                                    ? "PATTERN LOOKS STEADY"
                                    : "COMPLETE THREE CHECK-INS"),
                        2, GrayLevel::Black);
  }
  button(32, 392, 352, 64, "START SESSION", true, 3);
  button(416, 392, 352, 64, "RESET TODAY", false, 3);
}

void render_counter() {
  s_canvas->clear(GrayLevel::White);
  s_canvas->draw_text(32, 24, ui_text("KICK COUNTER"), 4, GrayLevel::Black);
  s_canvas->draw_text(32, 68, ui_text(kick_period_name(s_period)), 2,
                      GrayLevel::DarkGray);
  s_canvas->draw_circle(400, 220, 126, GrayLevel::Black);
  char count[16] = {};
  std::snprintf(count, sizeof(count), "%u", s_count);
  s_canvas->draw_text(400 - ui_text_width(count, 10U) / 2, 178, count, 10,
                      GrayLevel::Black);
  s_canvas->draw_text(400 - ui_text_width("TAP FOR EACH KICK", 2U) / 2, 272,
                      ui_text("TAP FOR EACH KICK"), 2, GrayLevel::Black);
  button(32, 392, 224, 64, "CANCEL", false, 3);
  button(544, 392, 224, 64, "FINISH", true, 3);
}

void render(bool full = false) {
  if (s_counting)
    render_counter();
  else
    render_dashboard();
  if (full)
    sticky_display_refresh_monochrome();
  else
    sticky_display_refresh_partial();
}

void finish() {
  const int64_t elapsed = esp_timer_get_time() - s_started_us;
  KickSession session = {};
  session.date = s_today;
  session.hour = s_hour;
  session.minute = s_minute;
  session.period = s_period;
  session.count = s_count;
  session.duration_minutes =
      static_cast<uint16_t>((elapsed + 59999999LL) / 60000000LL);
  if (session.duration_minutes > 60U)
    session.duration_minutes = 60U;
  if (s_service.upsert(session))
    kick_storage_save(s_service);
  s_counting = false;
}

void handle(int x, int y) {
  const int64_t now = esp_timer_get_time();
  if (s_counting) {
    if (y >= 392 && x < 256)
      s_counting = false;
    else if (y >= 392 && x >= 544)
      finish();
    else if (x >= 250 && x < 550 && y >= 82 && y < 356 &&
             (s_last_kick_us == 0 || now - s_last_kick_us >= kDebounceUs)) {
      ++s_count;
      s_last_kick_us = now;
    }
    render();
    return;
  }
  if (y < 392)
    return;
  if (x < 384) {
    read_clock();
    s_period = inferred_period();
    s_count = 0U;
    s_started_us = now;
    s_last_kick_us = 0;
    s_counting = true;
  } else {
    s_service.remove_day(s_today);
    kick_storage_save(s_service);
  }
  render();
}

void task(void *) {
  kick_storage_load(s_service);
  read_clock();
  render(true);
  while (true) {
    if (sticky_app_lifecycle_checkpoint(s_lifecycle)) {
      kick_storage_load(s_service);
      read_clock();
      render(true);
    }
    if (s_counting &&
        esp_timer_get_time() - s_started_us >= kMaximumSessionUs) {
      finish();
      render();
    }
    StickyTouchPress press = {};
    if (sticky_touch_take_press(press)) {
      int x = 0, y = 0;
      s_canvas->physical_to_logical(press.x, press.y, x, y);
      handle(x, y);
      sticky_touch_clear_press();
    }
    vTaskDelay(pdMS_TO_TICKS(40));
  }
}
} // namespace

esp_err_t kick_app_start(Canvas &canvas) {
  app_log_register_tag(kTag);
  if (s_task != nullptr)
    return ESP_OK;
  s_canvas = &canvas;
  return xTaskCreate(task, "kicks", 5120U, nullptr, 3U, &s_task) == pdPASS
             ? ESP_OK
             : ESP_ERR_NO_MEM;
}
esp_err_t kick_app_pause() {
  return sticky_app_lifecycle_pause(s_lifecycle, s_task);
}
esp_err_t kick_app_resume() {
  if (s_task == nullptr)
    return ESP_ERR_INVALID_STATE;
  sticky_app_lifecycle_resume(s_lifecycle);
  return ESP_OK;
}
esp_err_t kick_app_prepare_power_sleep(uint32_t &current, uint32_t &next) {
  current = 0U;
  next = 0U;
  return kick_app_pause();
}
uint32_t kick_app_power_sleep_timeout_ms() {
  return s_counting ? 0U : 60U * 1000U;
}
