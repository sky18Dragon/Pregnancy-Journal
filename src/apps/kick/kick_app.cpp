#include "kick_app.h"

#include "app_log.h"
#include "canvas.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "kick_pages.h"
#include "kick_service.h"
#include "kick_storage.h"
#include "sticky_app.h"
#include "sticky_app_lifecycle.h"
#include "sticky_display.h"
#include "sticky_rtc.h"
#include "sticky_touch.h"

namespace {
constexpr char kTag[] = "KICKS";
constexpr int64_t kDebounceUs = 400LL * 1000LL;
constexpr int64_t kMaximumSessionUs = 60LL * 60LL * 1000000LL;
Canvas *s_canvas = nullptr;
TaskHandle_t s_task = nullptr;
StickyAppLifecycle s_lifecycle = {};
KickService s_service;
PregnancyDate s_today = {};
uint8_t s_hour = 0U;
uint8_t s_minute = 0U;
KickPeriod s_period = KickPeriod::Morning;
KickPage s_page = KickPage::Today;
bool s_save_failed = false;
uint16_t s_count = 0U;
uint16_t s_rendered_elapsed_minutes = 0U;
int64_t s_started_us = 0;
int64_t s_last_kick_us = 0;

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

uint16_t elapsed_minutes() {
  if (s_page != KickPage::Counter || s_started_us == 0)
    return 0U;
  const int64_t elapsed = esp_timer_get_time() - s_started_us;
  const int64_t rounded = (elapsed + 59999999LL) / 60000000LL;
  return static_cast<uint16_t>(rounded > 60LL ? 60LL : rounded);
}

void render(bool full = false) {
  sticky_display_set_battery_overlay_enabled(false);
  KickDaySummary today = {};
  s_service.summarize(s_today, today);
  if (s_page == KickPage::Counter) {
    s_rendered_elapsed_minutes = elapsed_minutes();
    kick_page_render_counter(*s_canvas, s_period, s_count,
                             s_rendered_elapsed_minutes);
  } else if (s_page == KickPage::Insights) {
    KickTrendSummary trend = {};
    s_service.summarize_recent(s_today, trend);
    kick_page_render_insights(*s_canvas, today, trend);
  } else {
    kick_page_render_today(*s_canvas, today, s_save_failed);
  }
  if (full)
    sticky_display_refresh_monochrome();
  else
    sticky_display_refresh_partial();
}

void finish() {
  if (s_count == 0U) {
    s_page = KickPage::Today;
    return;
  }
  KickSession session = {};
  session.date = s_today;
  session.hour = s_hour;
  session.minute = s_minute;
  session.period = s_period;
  session.count = s_count;
  session.duration_minutes = elapsed_minutes();
  s_save_failed = !s_service.upsert(session) ||
                  kick_storage_save(s_service) != ESP_OK;
  s_page = KickPage::Today;
}

void handle(int x, int y) {
  const int64_t now = esp_timer_get_time();
  const KickAction action = kick_page_action_at(
      s_page, s_canvas->width(), s_canvas->height(), x, y);
  if (action == KickAction::None)
    return;

  if (action == KickAction::Back) {
    if (s_page == KickPage::Today) {
      sticky_app_request_launcher();
      return;
    }
    s_page = KickPage::Today;
  } else if (action == KickAction::Start) {
    read_clock();
    s_period = inferred_period();
    s_count = 0U;
    s_started_us = now;
    s_last_kick_us = 0;
    s_save_failed = false;
    s_page = KickPage::Counter;
  } else if (action == KickAction::ShowInsights) {
    s_page = KickPage::Insights;
  } else if (action == KickAction::Count) {
    if (s_count < 500U &&
        (s_last_kick_us == 0 || now - s_last_kick_us >= kDebounceUs)) {
      ++s_count;
      s_last_kick_us = now;
    }
  } else if (action == KickAction::Undo) {
    if (s_count > 0U)
      --s_count;
    s_last_kick_us = 0;
  } else if (action == KickAction::Finish) {
    finish();
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
      s_page = KickPage::Today;
      render(true);
    }
    if (s_page == KickPage::Counter &&
        esp_timer_get_time() - s_started_us >= kMaximumSessionUs) {
      finish();
      render();
    } else if (s_page == KickPage::Counter &&
               elapsed_minutes() != s_rendered_elapsed_minutes) {
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
  return s_page == KickPage::Counter ? 0U : 60U * 1000U;
}
