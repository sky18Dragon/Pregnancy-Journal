#include "kick_app.h"

#include "app_log.h"

#include <cstring>
#include "canvas.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "kick_pages.h"
#include "kick_service.h"
#include "kick_comparison.h"
#include "kick_counter.h"
#include "domain_repository_stores.h"
#include "repository_status_page.h"
#include "prototype_ui.h"
#include "ui_language.h"
#include "sticky_app.h"
#include "sticky_app_lifecycle.h"
#include "sticky_display.h"
#include "sticky_rtc.h"
#include "sticky_touch.h"

namespace {
constexpr char kTag[] = "KICKS";
// Includes repository completion collection plus libc/display driver frames.
constexpr uint32_t kTaskStackBytes = 8192U;
// Only the kicks task accesses retry comparison scratch; keep two 1 KiB
// payloads out of poll_store's frame, including its normal load path.
pregnancy_repository::EncodedPayload s_retry_left = {}, s_retry_right = {};
Canvas *s_canvas = nullptr;
TaskHandle_t s_task = nullptr;
StickyAppLifecycle s_lifecycle = {};
pregnancy_repository::KickRepositoryStore &s_store =
    pregnancy_repository::kick_repository_store();
using sticky_storage::SdRepositoryAdapterStatus;
SdRepositoryAdapterStatus s_storage_status = SdRepositoryAdapterStatus::Idle;
bool s_storage_saving = false;
KickSession s_pending_session = {};
bool s_retry_session = false;
uint32_t s_session_generation = 0U;
uint8_t s_partial_refreshes = 0U;
PregnancyDate s_today = {};
uint8_t s_hour = 0U;
uint8_t s_minute = 0U;
KickPeriod s_period = KickPeriod::Morning;
KickPage s_page = KickPage::Today;
bool s_save_failed = false;
KickCounter s_counter;
uint16_t s_rendered_elapsed_minutes = 0U;
// The record shown on the Result page after a durable save.
KickSession s_result_session = {};

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

uint16_t elapsed_minutes() { return s_counter.elapsed_minutes(esp_timer_get_time()); }

void render(bool full = false) {
  sticky_display_set_battery_overlay_enabled(false);
  KickDaySummary today = {};
  if (s_store.pending() || (s_storage_status != SdRepositoryAdapterStatus::Idle &&
                            s_storage_status != SdRepositoryAdapterStatus::Ok)) {
    repository_status_ui::render(*s_canvas, ui_text("KICKS"), s_store.pending(),
                                  s_storage_saving, s_storage_status);
    sticky_display_refresh_monochrome();
    return;
  }
  s_store.service().summarize(s_today, today);
  const KickService &service = s_store.service();
  if (s_page == KickPage::Counter) {
    s_rendered_elapsed_minutes = elapsed_minutes();
    kick_page_render_counter(*s_canvas, s_period, s_counter.count(),
                             s_rendered_elapsed_minutes, s_counter.mode());
  } else if (s_page == KickPage::ModeSelect) {
    kick_page_render_mode_select(*s_canvas);
  } else if (s_page == KickPage::Feel) {
    kick_page_render_feel(*s_canvas);
  } else if (s_page == KickPage::Result) {
    KickAwarenessSummary awareness = {};
    kick_summarize_awareness(service, s_result_session.date, awareness);
    kick_page_render_result(*s_canvas, s_result_session,
                            kick_compare_session(service, s_result_session),
                            awareness);
  } else if (s_page == KickPage::Compare) {
    KickComparison periods[3] = {};
    bool has_period[3] = {};
    kick_compare_day(service, s_today, periods, has_period);
    KickComparison ten = {};
    const bool has_ten = kick_compare_latest_count_to_ten(service, s_today, ten);
    KickAwarenessSummary awareness = {};
    const bool has_awareness =
        kick_summarize_awareness(service, s_today, awareness);
    kick_page_render_compare(*s_canvas, periods, has_period, has_ten, ten,
                             has_awareness, awareness);
  } else if (s_page == KickPage::Insights) {
    KickTrendSummary trend = {};
    s_store.service().summarize_recent(s_today, trend);
    kick_page_render_insights(*s_canvas, today, trend);
  } else {
    kick_page_render_today(*s_canvas, today, s_save_failed);
  }
  if (full || s_partial_refreshes >= 8U) {
    s_partial_refreshes = 0U;
    sticky_display_refresh_monochrome();
  } else {
    ++s_partial_refreshes;
    sticky_display_refresh_partial();
  }
}

void save_session(const KickSession &session) {
  s_pending_session = session;
  s_pending_session.id = s_store.service().next_id();
  s_session_generation = sticky_storage::sd_storage_service_status().mount_generation;
  s_storage_saving = true;
  s_storage_status = s_store.begin_upsert(s_pending_session);
  // Keep the draft counter until the durable completion is collected.
  if (s_storage_status == SdRepositoryAdapterStatus::Pending)
    s_storage_status = SdRepositoryAdapterStatus::Idle;
  else
    s_save_failed = true;
}

void finish() {
  if (s_counter.count() == 0U) {
    s_counter.cancel();
    s_page = KickPage::Today;
    return;
  }
  save_session(s_counter.snapshot(esp_timer_get_time()));
}

void save_feel(KickFeel feel) {
  if (!read_clock()) {
    s_save_failed = true;
    s_page = KickPage::Today;
    return;
  }
  KickSession session = {};
  session.date = s_today;
  session.hour = s_hour;
  session.minute = s_minute;
  session.period = inferred_period();
  session.mode = KickMode::Awareness;
  session.feel = feel;
  save_session(session);
}

bool begin_counter(KickMode mode, int64_t now) {
  if (!read_clock()) {
    s_save_failed = true;
    s_page = KickPage::Today;
    return false;
  }
  s_period = inferred_period();
  s_counter.cancel();
  s_counter.begin(s_today, s_hour, s_minute, s_period, now, mode);
  s_save_failed = false;
  s_page = KickPage::Counter;
  return true;
}

void begin_load() {
  if (!s_store.ready() && !s_retry_session) {
    s_counter.cancel();
    s_page = KickPage::Today;
  }
  s_storage_saving = false;
  s_storage_status = s_store.begin_load();
  if (s_storage_status == SdRepositoryAdapterStatus::Pending)
    s_storage_status = SdRepositoryAdapterStatus::Idle;
}

bool poll_store() {
  if (!s_store.pending()) return false;
  s_storage_status = s_store.poll();
  if (s_storage_status == SdRepositoryAdapterStatus::Pending) {
    s_storage_status = SdRepositoryAdapterStatus::Idle;
    return false;
  }
  if (s_storage_status == SdRepositoryAdapterStatus::Ok && s_retry_session) {
    s_retry_session = false;
    const auto current = sticky_storage::sd_storage_service_status();
    const auto *existing = s_store.service().find(s_pending_session.id);
    if (current.mount_generation != s_session_generation) {
      s_storage_status = SdRepositoryAdapterStatus::StaleGeneration;
    } else if (existing == nullptr) {
      s_storage_saving = true;
      s_storage_status = s_store.begin_upsert(s_pending_session);
      if (s_storage_status == SdRepositoryAdapterStatus::Pending)
        s_storage_status = SdRepositoryAdapterStatus::Idle;
      return true;
    } else {
      auto &left = s_retry_left;
      auto &right = s_retry_right;
      const auto ok = pregnancy_repository::RecordCodecStatus::Ok;
      if (pregnancy_repository::encode_kick(*existing, left) != ok ||
          pregnancy_repository::encode_kick(s_pending_session, right) != ok ||
          left.size != right.size || std::memcmp(left.bytes.data(), right.bytes.data(), left.size) != 0)
        s_storage_status = SdRepositoryAdapterStatus::RevisionConflict;
      else s_storage_saving = true;
    }
  }
  if (s_storage_status == SdRepositoryAdapterStatus::Ok && s_storage_saving) {
    // Show the saved record next to its own baseline.
    s_result_session = s_pending_session;
    s_page = KickPage::Result;
    s_counter.cancel();
    s_save_failed = false;
    s_storage_saving = false;
    s_pending_session = {};
  }
  STICKY_LOGI(kTag, "repository=completed status=%u stack_free_bytes=%u",
              static_cast<unsigned>(s_storage_status),
              static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
  return true;
}

void handle(int x, int y) {
  if (s_store.pending()) return;
  if (s_storage_status != SdRepositoryAdapterStatus::Idle &&
      s_storage_status != SdRepositoryAdapterStatus::Ok) {
    const auto back = prototype_ui::header_action_rect(s_canvas->width(), s_canvas->height());
    if (back.contains(x, y)) {
      s_page = KickPage::Today;
      sticky_app_request_launcher();
      return;
    }
    if (repository_status_ui::retry_rect(*s_canvas).contains(x, y)) {
      // Reconcile the same stable ID before retrying an uncertain commit.
      s_retry_session = s_storage_saving && s_pending_session.id != 0U;
      if (!s_retry_session) s_page = KickPage::Today;
      begin_load();
      render(true);
    }
    return;
  }
  const int64_t now = esp_timer_get_time();
  const KickPage page_before = s_page;
  const KickAction action = kick_page_action_at(
      s_page, s_canvas->width(), s_canvas->height(), x, y);
  if (action == KickAction::None)
    return;

  switch (action) {
  case KickAction::Back:
    if (s_page == KickPage::Today) {
      sticky_app_request_launcher();
      return;
    }
    if (s_page == KickPage::Counter) s_counter.cancel();
    s_page = KickPage::Today;
    break;
  case KickAction::Done:
    s_page = KickPage::Today;
    break;
  case KickAction::Start:
    if (s_store.service().count() >= KickService::kCapacity) {
      s_storage_status = SdRepositoryAdapterStatus::CapacityReached;
      render(true);
      return;
    }
    s_save_failed = false;
    s_page = KickPage::ModeSelect;
    break;
  case KickAction::StartTimed:
    begin_counter(KickMode::Timed, now);
    break;
  case KickAction::StartCountToTen:
    begin_counter(KickMode::CountToTen, now);
    break;
  case KickAction::StartAwareness:
    s_page = KickPage::Feel;
    break;
  case KickAction::FeelUsual:
    save_feel(KickFeel::Usual);
    break;
  case KickAction::FeelWeaker:
    save_feel(KickFeel::Weaker);
    break;
  case KickAction::FeelFewer:
    save_feel(KickFeel::Fewer);
    break;
  case KickAction::FeelChanged:
    save_feel(KickFeel::Changed);
    break;
  case KickAction::ShowInsights:
    s_page = KickPage::Insights;
    break;
  case KickAction::ShowCompare:
    read_clock();
    s_page = KickPage::Compare;
    break;
  case KickAction::Count:
    // The tenth press of a count-to-ten session saves it immediately.
    if (s_counter.add(now) && s_counter.target_reached()) finish();
    break;
  case KickAction::Undo:
    s_counter.undo();
    break;
  case KickAction::Finish:
    finish();
    break;
  case KickAction::None:
    return;
  }
  // Page changes get a full refresh; counter taps stay partial.
  render(s_page != page_before);
}

void task(void *) {
  begin_load();
  read_clock();
  render(true);
  unsigned clock_polls = 0U;
  while (true) {
    if (poll_store()) render(true);
    if (!s_store.pending() && s_storage_status == SdRepositoryAdapterStatus::Ok && !s_store.ready()) {
      s_page = KickPage::Today;
      begin_load(); render(true);
    }
    if (sticky_app_lifecycle_checkpoint(s_lifecycle)) {
      if (!s_store.pending() && !s_store.ready()) {
        begin_load();
      }
      read_clock();
      render(true);
    }
    if (!s_store.pending() && s_storage_status == SdRepositoryAdapterStatus::Ok &&
        s_page == KickPage::Counter &&
        s_counter.expired(esp_timer_get_time())) {
      finish();
      render();
    } else if (!s_store.pending() && s_storage_status == SdRepositoryAdapterStatus::Ok &&
               s_page == KickPage::Counter &&
               elapsed_minutes() != s_rendered_elapsed_minutes) {
      render();
    }
    if (++clock_polls >= 25U && s_page != KickPage::Counter && !s_store.pending()) {
      clock_polls = 0U;
      const auto before = s_today;
      if (read_clock() && (before.year != s_today.year || before.month != s_today.month || before.day != s_today.day)) render(true);
    }
    StickyTouchPress press = {};
    if (sticky_touch_take_press(press)) {
      int x = 0, y = 0;
      s_canvas->physical_to_logical(press.x, press.y, x, y);
      handle(x, y);
      sticky_touch_clear_press();
    }
    // Loading reads one record per completion; poll faster while pending so
    // a full collection does not take seconds longer than the card needs.
    vTaskDelay(pdMS_TO_TICKS(s_store.pending() ? 5 : 40));
  }
}
} // namespace

esp_err_t kick_app_start(Canvas &canvas) {
  app_log_register_tag(kTag);
  if (s_task != nullptr)
    return ESP_OK;
  s_canvas = &canvas;
  return xTaskCreate(task, "kicks", kTaskStackBytes, nullptr, 3U, &s_task) == pdPASS
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
  return s_store.pending() || s_page == KickPage::Counter ? 0U : 60U * 1000U;
}
