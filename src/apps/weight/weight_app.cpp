#include "weight_app.h"

#include "app_log.h"
#include "canvas.h"
#include "domain_repository_stores.h"
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
#include "weight_pages.h"

namespace {
constexpr char kTag[] = "WEIGHT";
Canvas *s_canvas = nullptr;
TaskHandle_t s_task = nullptr;
StickyAppLifecycle s_lifecycle = {};
auto &s_store = pregnancy_repository::weight_repository_store();
using sticky_storage::SdRepositoryAdapterStatus;
SdRepositoryAdapterStatus s_storage_status = SdRepositoryAdapterStatus::Idle;
WeightPageState s_state = {};
WeightProfileSettings s_pending_settings = {};
bool s_storage_saving = false;
bool s_save_preferences_after_record = false;

bool read_today() {
  StickyRtcDateTime value = {};
  if (sticky_rtc_read(value) != ESP_OK) return false;
  s_state.today = {value.year, value.month, value.day};
  return pregnancy_date_valid(s_state.today);
}
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
  const int page_id = status ? kStatusPageId : static_cast<int>(s_state.page);
  if (s_refresh.page_changes(page_id)) show_press_feedback();
  else s_press.clear();
  if (status) {
    repository_status_ui::render(*s_canvas, "WEIGHT DATA", s_store.pending(),
                                  s_storage_saving, s_storage_status);
  } else weight_page_render(*s_canvas, s_store.service(), s_store.settings(), s_state);
  if (s_refresh.needs_full(page_id, full)) sticky_display_refresh_monochrome();
  else sticky_display_refresh_partial();
}
void begin_load() {
  s_state.page = WeightPage::Dashboard;
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
    s_state.page = s_state.editing_history ? WeightPage::History : WeightPage::Dashboard;
  s_storage_saving = false;
  s_storage_status = status;
  return true;
}
void save_record() {
  WeightRecord record = {};
  if (s_state.edit_id != 0U) {
    const auto *existing = s_store.service().find(s_state.edit_id);
    if (existing != nullptr) record = *existing;  // Preserve notes while editing the number.
  }
  record.id = s_state.edit_id;
  record.date = s_state.edit_date;
  record.weight_tenths_kg = weight_stored_value(s_state.edit_display_weight, s_store.settings().use_pounds);
  s_pending_settings = s_store.settings();
  s_save_preferences_after_record = s_pending_settings.baseline_tenths_kg == 0U;
  if (s_save_preferences_after_record) {
    const auto &service = s_store.service();
    const auto *first = service.count() == 0U ? nullptr : service.history_at(service.count() - 1U);
    s_pending_settings.baseline_tenths_kg = first ? first->weight_tenths_kg : record.weight_tenths_kg;
  }
  s_storage_saving = true;
  s_storage_status = s_store.begin_upsert(record);
  if (s_storage_status == SdRepositoryAdapterStatus::Pending)
    s_storage_status = SdRepositoryAdapterStatus::Idle;
  else s_save_preferences_after_record = false;
}
void open_editor(const WeightRecord *record, bool history) {
  s_state.editing_history = history;
  s_state.edit_id = record ? record->id : 0U;
  s_state.edit_date = record ? record->date : s_state.today;
  const auto *latest = s_store.service().latest();
  // Start from the record, else the latest one, else the pre-pregnancy
  // baseline from the profile (close to the first reading), else 60.0 kg.
  const uint16_t baseline = s_store.settings().baseline_tenths_kg;
  s_state.edit_display_weight = weight_display_value(record ? record->weight_tenths_kg
                                                    : latest ? latest->weight_tenths_kg
                                                    : baseline != 0U ? baseline : 600U,
                                                      s_store.settings().use_pounds);
  s_state.page = WeightPage::Editor;
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
    const size_t count = weight_page_targets(s_state.page, w, h, targets, 16U);
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
  const auto hit = weight_page_action_at(s_state.page, s_canvas->width(), s_canvas->height(), x, y);
  const bool pounds = s_store.settings().use_pounds;
  switch (hit.action) {
  case WeightAction::None: return;
  case WeightAction::Back:
    if (s_state.page == WeightPage::Dashboard) { show_press_feedback(); sticky_app_request_launcher(); return; }
    if (s_state.page == WeightPage::ReplaceConfirm || s_state.page == WeightPage::DeleteConfirm)
      s_state.page = WeightPage::Editor;
    else s_state.page = s_state.editing_history && s_state.page == WeightPage::Editor ? WeightPage::History : WeightPage::Dashboard;
    break;
  case WeightAction::ToggleUnit: {
    auto settings = s_store.settings(); settings.use_pounds = !settings.use_pounds;
    s_state.editing_history = false;
    begin_preferences_save(settings);
    break;
  }
  case WeightAction::ToggleTrend: s_state.trend_days = s_state.trend_days == 7U ? 30U : 7U; break;
  case WeightAction::AddToday:
    if (!read_today()) { s_storage_status = SdRepositoryAdapterStatus::InvalidArgument; break; }
    open_editor(s_store.service().find_date(s_state.today), false); break;
  case WeightAction::History: s_state.history_offset = 0U; s_state.page = WeightPage::History; break;
  case WeightAction::Profile:
    s_state.profile = s_store.settings();
    if (s_state.profile.baseline_tenths_kg == 0U) s_state.profile.baseline_tenths_kg = 600U;
    s_state.editing_history = false; s_state.page = WeightPage::Profile; break;
  case WeightAction::SelectRecord: {
    const auto *record = s_store.service().history_at(s_state.history_offset + hit.row);
    if (record) open_editor(record, true);
    break;
  }
  case WeightAction::Previous:
    if (s_state.history_offset >= kWeightHistoryRows) s_state.history_offset -= kWeightHistoryRows;
    break;
  case WeightAction::Next:
    if (s_state.history_offset + kWeightHistoryRows < s_store.service().count()) s_state.history_offset += kWeightHistoryRows;
    break;
  case WeightAction::MinusOne: case WeightAction::MinusTenth:
  case WeightAction::PlusTenth: case WeightAction::PlusOne: {
    const int delta = hit.action == WeightAction::MinusOne ? -10 : hit.action == WeightAction::MinusTenth ? -1 : hit.action == WeightAction::PlusTenth ? 1 : 10;
    const int value = static_cast<int>(s_state.edit_display_weight) + delta;
    if (value >= weight_display_value(300U, pounds) && value <= weight_display_value(2000U, pounds))
      s_state.edit_display_weight = static_cast<uint16_t>(value);
    break;
  }
  case WeightAction::HeightDownBig: s_state.profile.height_cm = s_state.profile.height_cm >= 105U ? s_state.profile.height_cm - 5U : 100U; break;
  case WeightAction::HeightUpBig: s_state.profile.height_cm = s_state.profile.height_cm <= 215U ? s_state.profile.height_cm + 5U : 220U; break;
  case WeightAction::BaselineDownBig: s_state.profile.baseline_tenths_kg = s_state.profile.baseline_tenths_kg >= 310U ? s_state.profile.baseline_tenths_kg - 10U : 300U; break;
  case WeightAction::BaselineUpBig: s_state.profile.baseline_tenths_kg = s_state.profile.baseline_tenths_kg <= 1990U ? s_state.profile.baseline_tenths_kg + 10U : 2000U; break;
  case WeightAction::HeightDown: if (s_state.profile.height_cm > 100U) --s_state.profile.height_cm; break;
  case WeightAction::HeightUp: if (s_state.profile.height_cm < 220U) ++s_state.profile.height_cm; break;
  case WeightAction::BaselineDown: if (s_state.profile.baseline_tenths_kg > 300U) --s_state.profile.baseline_tenths_kg; break;
  case WeightAction::BaselineUp: if (s_state.profile.baseline_tenths_kg < 2000U) ++s_state.profile.baseline_tenths_kg; break;
  case WeightAction::Save:
    if (s_state.page == WeightPage::Profile) begin_preferences_save(s_state.profile);
    else if (s_store.service().find_date(s_state.edit_date) != nullptr) s_state.page = WeightPage::ReplaceConfirm;
    else save_record();
    break;
  case WeightAction::Delete: if (s_state.edit_id != 0U) s_state.page = WeightPage::DeleteConfirm; break;
  case WeightAction::Cancel: s_state.page = WeightPage::Editor; break;
  case WeightAction::Confirm:
    if (s_state.page == WeightPage::ReplaceConfirm) save_record();
    else {
      s_storage_saving = true;
      s_storage_status = s_store.begin_erase(s_state.edit_id);
      if (s_storage_status == SdRepositoryAdapterStatus::Pending) s_storage_status = SdRepositoryAdapterStatus::Idle;
    }
    break;
  }
  // The tracker promotes a page change to a full refresh; value edits on the
  // same page stay partial (the display layer still forces periodic cleanup).
  render();
}
void task(void *) {
  begin_load(); read_today(); render(true);
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
      read_today(); render(true);
    }
    if (++clock_polls >= 25U && s_state.page == WeightPage::Dashboard && !s_store.pending()) {
      clock_polls = 0U;
      const auto before = s_state.today;
      if (read_today() && (before.year != s_state.today.year || before.month != s_state.today.month || before.day != s_state.today.day)) render(true);
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
  return s_store.pending() || (s_state.page != WeightPage::Dashboard && s_state.page != WeightPage::History) ? 0U : 60U * 1000U;
}
