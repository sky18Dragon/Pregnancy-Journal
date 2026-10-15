#include "kick_app.h"

#include "app_log.h"

#include <cstring>
#include "canvas.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "kick_flow.h"
#include "kick_pages.h"
#include "kick_service.h"
#include "kick_comparison.h"
#include "kick_counter.h"
#include "domain_repository_stores.h"
#include "repository_status_page.h"
#include "prototype_ui.h"
#include "press_feedback.h"
#include "refresh_policy.h"
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
PageRefreshTracker s_refresh;
constexpr int kStatusPageId = 1000;
press_feedback::Pending s_press;

// Draws the tapped target pressed and shows it once, before a page change
// whose full refresh would otherwise leave the panel unchanged for ~2 s.
void show_press_feedback() {
  press_feedback::Rect rect = {};
  if (!s_press.take(rect)) return;
  press_feedback::draw(*s_canvas, rect);
  sticky_display_refresh_partial();
}
uint16_t s_rendered_elapsed_minutes = 0U;

// The RTC and the record capacity behind the flow's port.
class KickPort final : public KickFlowPort {
public:
  bool read_clock(KickClockTime &time) override {
    StickyRtcDateTime value = {};
    if (sticky_rtc_read(value) != ESP_OK)
      return false;
    time.date = {value.year, value.month, value.day};
    time.hour = value.hour;
    time.minute = value.minute;
    return true;
  }
  bool at_capacity() override {
    return s_store.service().count() >= KickService::kCapacity;
  }
};

KickPort s_port;
KickFlow s_flow(s_port);

uint16_t elapsed_minutes() { return s_flow.elapsed_minutes(esp_timer_get_time()); }

bool status_page_active() {
  return s_store.pending() || (s_storage_status != SdRepositoryAdapterStatus::Idle &&
                               s_storage_status != SdRepositoryAdapterStatus::Ok);
}

void render(bool full = false) {
  sticky_display_set_battery_overlay_enabled(false);
  KickDaySummary today = {};
  const bool status = status_page_active();
  if (s_refresh.page_changes(status ? kStatusPageId : static_cast<int>(s_flow.page())))
    show_press_feedback();
  else
    s_press.clear();
  if (status) {
    repository_status_ui::render(*s_canvas, ui_text("KICKS"), s_store.pending(),
                                  s_storage_saving, s_storage_status);
    s_refresh.needs_full(kStatusPageId, true);
    sticky_display_refresh_monochrome();
    return;
  }
  const KickPage page = s_flow.page();
  const PregnancyDate &date = s_flow.today();
  s_store.service().summarize(date, today);
  const KickService &service = s_store.service();
  if (page == KickPage::Counter) {
    s_rendered_elapsed_minutes = elapsed_minutes();
    kick_page_render_counter(*s_canvas, s_flow.period(), s_flow.counter().count(),
                             s_rendered_elapsed_minutes, s_flow.counter().mode());
  } else if (page == KickPage::ModeSelect) {
    kick_page_render_mode_select(*s_canvas);
  } else if (page == KickPage::Feel) {
    kick_page_render_feel(*s_canvas);
  } else if (page == KickPage::Result) {
    const KickSession &result = s_flow.result_session();
    KickAwarenessSummary awareness = {};
    kick_summarize_awareness(service, result.date, awareness);
    kick_page_render_result(*s_canvas, result,
                            kick_compare_session(service, result), awareness);
  } else if (page == KickPage::Compare) {
    KickComparison periods[3] = {};
    bool has_period[3] = {};
    kick_compare_day(service, date, periods, has_period);
    KickComparison ten = {};
    const bool has_ten = kick_compare_latest_count_to_ten(service, date, ten);
    KickAwarenessSummary awareness = {};
    const bool has_awareness =
        kick_summarize_awareness(service, date, awareness);
    kick_page_render_compare(*s_canvas, periods, has_period, has_ten, ten,
                             has_awareness, awareness);
  } else if (page == KickPage::Insights) {
    KickTrendSummary trend = {};
    s_store.service().summarize_recent(date, trend);
    kick_page_render_insights(*s_canvas, today, trend);
  } else {
    kick_page_render_today(*s_canvas, today, s_flow.save_failed());
  }
  // Page changes are full; the display layer owns the partial-refresh ceiling.
  if (s_refresh.needs_full(static_cast<int>(page), full))
    sticky_display_refresh_monochrome();
  else
    sticky_display_refresh_partial();
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
    s_flow.mark_save_failed();
}

// Carries out what the flow asked for after an action.
void apply_flow_result(const KickFlowResult &result) {
  if (result.open_launcher) {
    show_press_feedback();
    sticky_app_request_launcher();
  }
  if (result.capacity_reached)
    s_storage_status = SdRepositoryAdapterStatus::CapacityReached;
  if (result.save_session)
    save_session(result.session);
}

void begin_load() {
  if (!s_store.ready() && !s_retry_session)
    s_flow.abandon_session();
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
    s_flow.session_saved(s_pending_session);
    s_storage_saving = false;
    s_pending_session = {};
  }
  STICKY_LOGI(kTag, "repository=completed status=%u stack_free_bytes=%u",
              static_cast<unsigned>(s_storage_status),
              static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
  return true;
}

// Records the tapped target so render() can show it pressed before a page change.
void remember_press_target(int x, int y) {
  press_feedback::Rect target = {};
  const int w = s_canvas->width(), h = s_canvas->height();
  bool found = false;
  if (status_page_active()) {
    const auto retry = repository_status_ui::retry_rect(*s_canvas);
    const auto back = prototype_ui::header_action_rect(w, h);
    found = retry.contains(x, y) ? (target = retry, true) : back.contains(x, y) ? (target = back, true) : false;
  } else {
    KickTarget targets[16];
    const size_t count = kick_page_targets(s_flow.page(), w, h, targets, 16U);
    for (size_t i = 0U; i < count && !found; ++i) {
      if (targets[i].contains(x, y)) { target = targets[i].rect; found = true; }
    }
  }
  if (found) s_press.set(target);
}

void handle(int x, int y) {
  if (s_store.pending()) return;
  if (s_storage_status != SdRepositoryAdapterStatus::Idle &&
      s_storage_status != SdRepositoryAdapterStatus::Ok) {
    const auto back = prototype_ui::header_action_rect(s_canvas->width(), s_canvas->height());
    if (back.contains(x, y)) {
      s_flow.go_today();
      s_press.set(back);
      show_press_feedback();
      sticky_app_request_launcher();
      return;
    }
    if (repository_status_ui::retry_rect(*s_canvas).contains(x, y)) {
      // Reconcile the same stable ID before retrying an uncertain commit.
      s_retry_session = s_storage_saving && s_pending_session.id != 0U;
      if (!s_retry_session) s_flow.go_today();
      begin_load();
      render(true);
    }
    return;
  }
  const KickAction action = kick_page_action_at(
      s_flow.page(), s_canvas->width(), s_canvas->height(), x, y);
  if (action == KickAction::None)
    return;
  const KickFlowResult result =
      s_flow.handle_action(action, esp_timer_get_time());
  apply_flow_result(result);
  if (result.capacity_reached) {
    render(true);
    return;
  }
  // Counter taps stay partial; the tracker promotes page changes to full.
  if (result.render)
    render();
}

void task(void *) {
  begin_load();
  s_flow.read_clock();
  render(true);
  unsigned clock_polls = 0U;
  while (true) {
    if (poll_store()) render(true);
    if (!s_store.pending() && s_storage_status == SdRepositoryAdapterStatus::Ok && !s_store.ready()) {
      s_flow.go_today();
      begin_load(); render(true);
    }
    if (sticky_app_lifecycle_checkpoint(s_lifecycle)) {
      if (!s_store.pending() && !s_store.ready()) {
        begin_load();
      }
      s_flow.read_clock();
      render(true);
    }
    if (!s_store.pending() && s_storage_status == SdRepositoryAdapterStatus::Ok &&
        s_flow.counter_expired(esp_timer_get_time())) {
      apply_flow_result(s_flow.finish(esp_timer_get_time()));
      render();
    } else if (!s_store.pending() && s_storage_status == SdRepositoryAdapterStatus::Ok &&
               s_flow.page() == KickPage::Counter &&
               elapsed_minutes() != s_rendered_elapsed_minutes) {
      render();
    }
    if (++clock_polls >= 25U && s_flow.page() != KickPage::Counter && !s_store.pending()) {
      clock_polls = 0U;
      if (s_flow.refresh_clock()) render(true);
    }
    StickyTouchPress press = {};
    if (sticky_touch_take_press(press)) {
      int x = 0, y = 0;
      s_canvas->physical_to_logical(press.x, press.y, x, y);
      remember_press_target(x, y);
      handle(x, y);
      s_press.clear();
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
  return s_store.pending() || s_flow.page() == KickPage::Counter ? 0U : 60U * 1000U;
}
