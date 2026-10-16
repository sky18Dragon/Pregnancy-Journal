#include "weight_app.h"

#include <atomic>

#include "app_log.h"
#include "canvas.h"
#include "domain_repository_stores.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "repository_status_page.h"
#include "sticky_app.h"
#include "sticky_app_lifecycle.h"
#include "press_feedback.h"
#include "refresh_policy.h"
#include "sticky_display.h"
#include "sticky_rtc.h"
#include "sticky_touch.h"
#include "weight_flow.h"
#include "weight_pages.h"

namespace {
constexpr char kTag[] = "WEIGHT";
Canvas *s_canvas = nullptr;
TaskHandle_t s_task = nullptr;
StickyAppLifecycle s_lifecycle = {};
auto &s_store = pregnancy_repository::weight_repository_store();
using sticky_storage::SdRepositoryAdapterStatus;
SdRepositoryAdapterStatus s_storage_status = SdRepositoryAdapterStatus::Idle;
WeightProfileSettings s_pending_settings = {};
bool s_storage_saving = false;
bool s_save_preferences_after_record = false;

// The RTC and the stored records/settings behind the flow's port.
class WeightPort final : public WeightFlowPort {
public:
  bool read_today(PregnancyDate &today) override {
    StickyRtcDateTime value = {};
    if (sticky_rtc_read(value) != ESP_OK) return false;
    today = {value.year, value.month, value.day};
    return true;
  }
  const WeightService &service() const override { return s_store.service(); }
  WeightProfileSettings settings() const override { return s_store.settings(); }
};

WeightPort s_port;
WeightFlow s_flow(s_port);
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

bool status_page_active() {
  return s_store.pending() || (s_storage_status != SdRepositoryAdapterStatus::Idle &&
                               s_storage_status != SdRepositoryAdapterStatus::Ok);
}

void render(bool full = false) {
  sticky_display_set_battery_overlay_enabled(false);
  const bool status = status_page_active();
  const int page_id = status ? kStatusPageId : static_cast<int>(s_flow.page());
  sticky_app_set_key_policy(StickyAppId::Weight,
                            status ? KeyPolicy::Locked
                                   : weight_page_key_policy(s_flow.page()));
  if (s_refresh.page_changes(page_id)) show_press_feedback();
  else s_press.clear();
  if (status) {
    repository_status_ui::render(*s_canvas, "WEIGHT DATA", s_store.pending(),
                                  s_storage_saving, s_storage_status);
  } else weight_page_render(*s_canvas, s_store.service(), s_store.settings(), s_flow.state());
  if (s_refresh.needs_full(page_id, full)) sticky_display_refresh_monochrome();
  else sticky_display_refresh_partial();
}
// A shortcut request waits for the card data to load, then applies once. It is
// dropped when it is older than kEntryWindowUs.
constexpr int64_t kEntryWindowUs = 5000000LL;
std::atomic<int64_t> s_entry_requested_us{0};

bool take_entry_request() {
  const int64_t at = s_entry_requested_us.load();
  if (at == 0) return false;
  if (esp_timer_get_time() - at > kEntryWindowUs) {
    s_entry_requested_us.store(0);
    return false;
  }
  if (s_store.pending() || status_page_active() || !s_store.ready())
    return false;
  s_entry_requested_us.store(0);
  return true;
}
void begin_load() {
  s_flow.reset_to_dashboard();
  s_storage_saving = false;
  s_save_preferences_after_record = false;
  s_storage_status = s_store.begin_load();
  if (s_storage_status == SdRepositoryAdapterStatus::Pending)
    s_storage_status = SdRepositoryAdapterStatus::Idle;
}
void begin_preferences_save(const WeightProfileSettings &settings) {
  s_pending_settings = settings;
  s_storage_saving = true;
  s_storage_status = s_store.begin_save_preferences(settings);
  if (s_storage_status == SdRepositoryAdapterStatus::Pending)
    s_storage_status = SdRepositoryAdapterStatus::Idle;
}
bool poll_store() {
  if (!s_store.pending()) return false;
  auto status = s_store.poll();
  if (status == SdRepositoryAdapterStatus::Pending) return false;
  if (status == SdRepositoryAdapterStatus::Ok && s_save_preferences_after_record) {
    s_save_preferences_after_record = false;
    begin_preferences_save(s_pending_settings);
    return true;
  }
  if (status == SdRepositoryAdapterStatus::Ok && s_storage_saving)
    s_flow.record_saved();
  s_storage_saving = false;
  s_storage_status = status;
  return true;
}
// Starts the repository write for a record the flow asked to save.
void begin_record_save(const WeightFlowResult &result) {
  s_pending_settings = result.settings;
  s_save_preferences_after_record = result.preferences_after_record;
  s_storage_saving = true;
  s_storage_status = s_store.begin_upsert(result.record);
  if (s_storage_status == SdRepositoryAdapterStatus::Pending)
    s_storage_status = SdRepositoryAdapterStatus::Idle;
  else s_save_preferences_after_record = false;
}

// Carries out what the flow asked for after an action.
void apply_flow_result(const WeightFlowResult &result) {
  if (result.open_launcher) {
    show_press_feedback();
    sticky_app_request_launcher();
  }
  if (result.invalid_clock)
    s_storage_status = SdRepositoryAdapterStatus::InvalidArgument;
  if (result.save_preferences)
    begin_preferences_save(result.settings);
  if (result.save_record)
    begin_record_save(result);
  if (result.erase_record) {
    s_storage_saving = true;
    s_storage_status = s_store.begin_erase(result.erase_id);
    if (s_storage_status == SdRepositoryAdapterStatus::Pending)
      s_storage_status = SdRepositoryAdapterStatus::Idle;
  }
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
    WeightTarget targets[16];
    const size_t count = weight_page_targets(s_flow.page(), w, h, targets, 16U);
    for (size_t i = 0U; i < count && !found; ++i) {
      if (targets[i].rect.contains(x, y)) { target = targets[i].rect; found = true; }
    }
  }
  if (found) s_press.set(target);
}
void handle(int x, int y) {
  if (s_store.pending()) return;
  const auto back = prototype_ui::header_action_rect(s_canvas->width(), s_canvas->height());
  if (s_storage_status != SdRepositoryAdapterStatus::Idle && s_storage_status != SdRepositoryAdapterStatus::Ok) {
    if (back.contains(x, y)) { s_press.set(back); show_press_feedback(); sticky_app_request_launcher(); return; }
    if (repository_status_ui::retry_rect(*s_canvas).contains(x, y)) begin_load();
    render(true);
    return;
  }
  const auto hit = weight_page_action_at(s_flow.page(), s_canvas->width(), s_canvas->height(), x, y);
  const WeightFlowResult result = s_flow.handle_action(hit);
  apply_flow_result(result);
  // The tracker promotes a page change to a full refresh; value edits on the
  // same page stay partial (the display layer still forces periodic cleanup).
  if (result.render) render();
}
// Opens today's entry when a shortcut asked for it; true when the page changed.
bool apply_entry_request() {
  if (!take_entry_request()) return false;
  const WeightFlowResult result = s_flow.enter_record_today();
  apply_flow_result(result);
  return result.render;
}
void task(void *) {
  begin_load(); s_flow.read_today(); render(true);
  unsigned clock_polls = 0U;
  while (true) {
    if (poll_store()) render(true);
    if (!s_store.pending() && s_storage_status == SdRepositoryAdapterStatus::Ok && !s_store.ready()) {
      begin_load(); render(true);
    }
    if (sticky_app_lifecycle_checkpoint(s_lifecycle)) {
      if (!s_store.pending() && !s_store.ready()) {
        begin_load();
      }
      s_flow.read_today();
      apply_entry_request();  // before the first redraw: one refresh, not two
      render(true);
    }
    if (apply_entry_request()) render();
    if (++clock_polls >= 25U && s_flow.page() == WeightPage::Dashboard && !s_store.pending()) {
      clock_polls = 0U;
      if (s_flow.refresh_today()) render(true);
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
    vTaskDelay(pdMS_TO_TICKS(40));
  }
}
}  // namespace

void weight_app_request_record_today() {
  s_entry_requested_us.store(esp_timer_get_time());
}
esp_err_t weight_app_start(Canvas &canvas) {
  app_log_register_tag(kTag);
  if (s_task) return ESP_OK;
  s_canvas = &canvas;
  return xTaskCreate(task, "weight", 5120U, nullptr, 3U, &s_task) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
esp_err_t weight_app_pause() { return sticky_app_lifecycle_pause(s_lifecycle, s_task); }
esp_err_t weight_app_resume() {
  if (!s_task) return ESP_ERR_INVALID_STATE;
  sticky_app_lifecycle_resume(s_lifecycle); return ESP_OK;
}
esp_err_t weight_app_prepare_power_sleep(uint32_t &current, uint32_t &next) {
  current = 0U; next = 0U; return weight_app_pause();
}
uint32_t weight_app_power_sleep_timeout_ms() {
  return s_store.pending() || (s_flow.page() != WeightPage::Dashboard && s_flow.page() != WeightPage::History) ? 0U : 60U * 1000U;
}
