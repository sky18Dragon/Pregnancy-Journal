#include "pregnancy_app.h"

#include <atomic>
#include <cstdio>

#include "app_log.h"
#include "battery_status_overlay.h"
#include "content_service.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pregnancy_flow.h"
#include "pregnancy_pages.h"
#include "pregnancy_repository_store.h"
#include "pregnancy_state.h"
#include "press_feedback.h"
#include "prototype_ui.h"
#include "refresh_policy.h"
#include "sticky_app.h"
#include "sticky_app_lifecycle.h"
#include "sticky_display.h"
#include "sticky_rtc.h"
#include "sticky_touch.h"

namespace {

constexpr char kTag[] = "PREGNANCY";
constexpr TickType_t kPollInterval = pdMS_TO_TICKS(40);
constexpr uint32_t kTaskStackSize = 5120U;
constexpr UBaseType_t kTaskPriority = 3U;
constexpr uint32_t kDashboardSleepTimeoutMs = 60U * 1000U;
constexpr int64_t kRtcPollIntervalUs = 60LL * 1000000LL;
// Idle time after a partial page change before one full refresh clears ghosting.
constexpr int64_t kGhostCleanupDelayUs = 5LL * 1000000LL;

Canvas *s_canvas = nullptr;
TaskHandle_t s_app_task = nullptr;
StickyAppLifecycle s_lifecycle = {};
CanvasRotation s_display_rotation = CanvasRotation::Deg0;
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
int64_t s_next_rtc_poll_us = 0;

// A partial page change left ghosting that the idle full refresh has not yet
// cleared; the repaint is due once the user has been idle until the deadline.
bool s_ghost_cleanup_pending = false;
int64_t s_ghost_cleanup_due_us = 0;

void defer_ghost_cleanup() {
  s_ghost_cleanup_due_us = esp_timer_get_time() + kGhostCleanupDelayUs;
}

// Entry timing: log marks around one entry (boot, resume or retry) show whether
// the time goes to the card read or to the screen refreshes. The window opens
// at the entry and closes once its last expected mark has been logged, so
// later refreshes are not counted as part of it.
constexpr unsigned kEntryRefreshLogLimit = 2U;
int64_t s_entry_us = 0;
unsigned s_entry_refreshes = 0U;
bool s_entry_close_after_render = false;

long entry_elapsed_ms() {
  return static_cast<long>((esp_timer_get_time() - s_entry_us) / 1000);
}

void mark_entry(const char *kind) {
  s_entry_us = esp_timer_get_time();
  s_entry_refreshes = 0U;
  s_entry_close_after_render = false;
  STICKY_LOGI(kTag, "entry_timing point=enter kind=%s t_ms=0", kind);
}

std::atomic<bool> s_setup_requested{false};
std::atomic<bool> s_dashboard_requested{false};
pregnancy_repository::PregnancyRepositoryStore &s_profile_store =
    pregnancy_repository::pregnancy_repository_store();
bool s_profile_loading = false;
// A profile the store already held is on screen while the card re-checks it.
bool s_profile_revalidating = false;
bool s_profile_saving = false;
bool s_retry_save = false;
bool s_reload_after_pending = false;
bool s_setup_after_load = false;
sticky_storage::SdRepositoryAdapterStatus s_storage_error =
    sticky_storage::SdRepositoryAdapterStatus::Idle;

prototype_ui::Rect storage_retry_rect() {
  return s_canvas->height() > s_canvas->width()
             ? prototype_ui::Rect{24, 720, 432, 56}
             : prototype_ui::Rect{24, 400, 752, 56};
}

const char *storage_error_copy(
    sticky_storage::SdRepositoryAdapterStatus status) {
  using sticky_storage::SdRepositoryAdapterStatus;
  switch (status) {
  case SdRepositoryAdapterStatus::CardMissing:
    return "MICROSD CARD IS REQUIRED";
  case SdRepositoryAdapterStatus::CardReadOnly:
    return "MICROSD CARD IS READ ONLY";
  case SdRepositoryAdapterStatus::CardFull:
  case SdRepositoryAdapterStatus::CapacityReached:
    return "MICROSD STORAGE IS FULL";
  case SdRepositoryAdapterStatus::DatasetUnavailable:
    return "CREATE OR SELECT A DATASET IN SETTINGS";
  case SdRepositoryAdapterStatus::RecoveryUnavailable:
  case SdRepositoryAdapterStatus::Corrupt:
    return "MICROSD DATA NEEDS REVIEW";
  case SdRepositoryAdapterStatus::StaleGeneration:
    return "MICROSD CARD CHANGED";
  default:
    return "MICROSD STORAGE UNAVAILABLE";
  }
}

void render_storage_state() {
  prototype_ui::begin_page(*s_canvas);
  prototype_ui::draw_header(*s_canvas, "MICROSD DATA",
                            "PREGNANCY PROFILE STORAGE", "BACK");
  const bool portrait = s_canvas->height() > s_canvas->width();
  if (s_profile_loading || s_profile_saving) {
    s_canvas->draw_text(32, portrait ? 260 : 176,
                        s_profile_saving ? "SAVING TO MICROSD"
                                         : "LOADING FROM MICROSD",
                        4, GrayLevel::Black);
    return;
  }
  s_canvas->draw_text(32, portrait ? 226 : 154,
                      storage_error_copy(s_storage_error), 3,
                      GrayLevel::Black);
  s_canvas->draw_text(32, portrait ? 286 : 210,
                      "NO FLASH FALLBACK WAS USED", 2,
                      GrayLevel::Black);
  prototype_ui::draw_button(*s_canvas, storage_retry_rect(), "RETRY", true, 2);
}

bool read_rtc(StickyRtcDateTime &value) {
  return sticky_rtc_is_ready() && sticky_rtc_read(value) == ESP_OK;
}

// The RTC behind the flow's clock port.
class RtcClockPort final : public PregnancyClockPort {
public:
  bool read(PregnancyClockTime &time) override {
    StickyRtcDateTime value = {};
    if (!read_rtc(value)) return false;
    time = {value.year, value.month, value.day, value.hour, value.minute,
            value.second};
    return true;
  }
  bool write(const PregnancyClockTime &time) override {
    StickyRtcDateTime value = {};
    value.year = time.year;
    value.month = time.month;
    value.day = time.day;
    value.hour = time.hour;
    value.minute = time.minute;
    value.second = time.second;
    return sticky_rtc_write(value) == ESP_OK;
  }
};

RtcClockPort s_clock_port;
PregnancyFlow s_flow(s_clock_port);

// True when going to `page_id` is a page change that is trialled with the
// partial waveform; must be asked before the tracker records the new page.
bool soft_page_change_to(int page_id) {
  return pregnancy_page_id_change_uses_partial(s_refresh.last_page(), page_id);
}

// Page changes use the full waveform and `partial` only applies when the page on
// screen stays the same (see PageRefreshTracker), except the Dashboard <-> Baby
// pair, which is trialled with the partial waveform when the caller allows it.
void refresh_display(bool partial, int page_id) {
  const int previous_page = s_refresh.last_page();
  const bool soft = partial && soft_page_change_to(page_id);
  const bool full = s_refresh.needs_full(page_id, !partial) && !soft;
  if (previous_page != page_id) {
    STICKY_LOGI(kTag, "page_refresh from=%d to=%d mode=%s", previous_page,
                page_id, full ? "full" : "partial");
  }
  if (full) {
    s_ghost_cleanup_pending = false;
  } else if (soft) {
    s_ghost_cleanup_pending = true;
  }
  const esp_err_t result = full ? sticky_display_refresh_monochrome()
                                : sticky_display_refresh_partial();
  // A partial request the display layer promoted to a full refresh has already
  // cleaned the panel: no idle repaint is due.
  if (sticky_display_partial_refreshes_since_full() == 0U)
    s_ghost_cleanup_pending = false;
  // The idle wait starts once the panel has settled, not when the refresh began.
  defer_ghost_cleanup();
  if (result != ESP_OK) {
    STICKY_LOGE(kTag, "display=refresh mode=%s result=%s",
                full ? "full" : "partial", esp_err_to_name(result));
  }
  if (s_entry_us != 0) {
    if (s_entry_refreshes < kEntryRefreshLogLimit) {
      ++s_entry_refreshes;
      STICKY_LOGI(kTag,
                  "entry_timing point=render n=%u mode=%s page=%d t_ms=%ld",
                  s_entry_refreshes, full ? "full" : "partial", page_id,
                  entry_elapsed_ms());
    }
    if (s_entry_close_after_render) s_entry_us = 0;
  }
}

bool status_page_active() {
  return s_profile_loading || s_profile_saving ||
         (s_storage_error != sticky_storage::SdRepositoryAdapterStatus::Idle &&
          s_storage_error != sticky_storage::SdRepositoryAdapterStatus::Ok);
}

void render_page(bool partial) {
  sticky_display_set_battery_overlay_enabled(false);
  s_canvas->set_rotation(s_display_rotation);
  const bool status = status_page_active();
  // A storage status page counts as home so the top key can still open the
  // launcher when the card is missing.
  sticky_app_set_key_policy(StickyAppId::Pregnancy,
                            status ? KeyPolicy::Home
                                   : pregnancy_page_key_policy(s_flow.page()));
  const int target_page = status ? kStatusPageId : static_cast<int>(s_flow.page());
  // A partial page change already answers in under a second, so it needs no
  // separate pressed-state refresh before it.
  if (s_refresh.page_changes(target_page) &&
      !(partial && soft_page_change_to(target_page)))
    show_press_feedback();
  else
    s_press.clear();
  if (status) {
    render_storage_state();
    refresh_display(partial, kStatusPageId);
    return;
  }
  switch (s_flow.page()) {
  case PregnancyPage::ClockSetup:
    pregnancy_page_render_clock_setup(*s_canvas, s_flow.clock_fields(),
                                      s_flow.input_error(), s_flow.configured());
    break;
  case PregnancyPage::SourceSetup:
    pregnancy_page_render_source_setup(*s_canvas, s_flow.configured());
    break;
  case PregnancyPage::DateSetup:
    pregnancy_page_render_profile_date_setup(
        *s_canvas, s_flow.date_fields(), s_flow.input_error(),
        s_flow.due_date_primary());
    break;
  case PregnancyPage::Dashboard: {
    pregnancy_page_render_dashboard(*s_canvas,
                                    s_flow.profile().estimated_due_date,
                                    s_flow.progress());
    int x = 0;
    int y = 0;
    pregnancy_page_status_battery_origin(s_canvas->height() > s_canvas->width(), x, y);
    battery_status_overlay_draw_compact(*s_canvas, x, y);
  } break;
  case PregnancyPage::Baby:
  case PregnancyPage::Mom:
    pregnancy_page_render_detail(
        *s_canvas, s_flow.page(), s_flow.progress(),
        pregnancy_content_for_week(s_flow.progress().weeks));
    break;
  }
  refresh_display(partial, static_cast<int>(s_flow.page()));
}

// Starts saving the profile the flow just validated.
void begin_profile_save() {
  s_retry_save = true;
  s_storage_error = s_profile_store.begin_save(s_flow.pending_profile());
  s_profile_saving =
      s_storage_error == sticky_storage::SdRepositoryAdapterStatus::Pending;
  if (s_profile_saving) s_storage_error =
      sticky_storage::SdRepositoryAdapterStatus::Idle;
}

// Runs one action through the flow and carries out what it asks for. Returns
// true when the page must be redrawn.
bool handle_action(PregnancyAction action) {
  const PregnancyFlowResult result = s_flow.handle_action(action);
  if (result.open_launcher) {
    show_press_feedback();
    sticky_app_request_launcher();
  }
  if (result.save_profile)
    begin_profile_save();
  return result.render;
}

PregnancyAction action_for_press(const StickyTouchPress &press) {
  int x = 0, y = 0;
  s_canvas->physical_to_logical(press.x, press.y, x, y);
  return pregnancy_page_action_at(s_flow.page(), s_flow.configured(),
                                  s_canvas->width(),
                                  s_canvas->height(), x, y);
}

// Shared tail once the flow holds the current profile.
void settle_flow_after_profile() {
  const bool setup_requested = s_setup_after_load;
  if (s_flow.page() != PregnancyPage::ClockSetup)
    s_setup_after_load = false;
  if (s_flow.settle_after_storage(setup_requested))
    s_next_rtc_poll_us = esp_timer_get_time() + kRtcPollIntervalUs;
}

// Applies a profile the flow can rely on and moves to the page it implies.
void settle_flow_with_profile(bool found, const PregnancyProfile &profile) {
  s_flow.profile_loaded(found, profile);
  settle_flow_after_profile();
}

// Begins reading the profile for boot, resume and reload. A profile the store
// still holds for this card is shown at once and re-checked quietly, so the
// loading page (and its extra full refresh) is only used for a cold read.
void start_profile_read() {
  const bool retained = s_profile_store.ready();
  s_storage_error = retained ? s_profile_store.begin_revalidate()
                             : s_profile_store.begin_load();
  const bool pending =
      s_storage_error == sticky_storage::SdRepositoryAdapterStatus::Pending;
  if (pending) {
    s_storage_error = sticky_storage::SdRepositoryAdapterStatus::Idle;
  } else {
    // A read that could not start never reports load_done, so the status page
    // that follows is the last mark of this entry.
    s_entry_close_after_render = true;
  }
  s_profile_revalidating = pending && retained;
  s_profile_loading = pending && !retained;
  if (s_profile_revalidating)
    settle_flow_with_profile(s_profile_store.found(), s_profile_store.profile());
}

void initialize_state() {
  s_flow.begin();
  s_retry_save = false;
  start_profile_read();
}

bool poll_profile_storage() {
  if (!s_profile_loading && !s_profile_saving && !s_profile_revalidating)
    return false;
  const sticky_storage::SdRepositoryAdapterStatus status =
      s_profile_store.poll();
  if (status == sticky_storage::SdRepositoryAdapterStatus::Pending)
    return false;
  const bool was_loading = s_profile_loading;
  const bool was_revalidating = s_profile_revalidating;
  const bool changed = s_profile_store.changed();
  s_profile_loading = false;
  s_profile_saving = false;
  s_profile_revalidating = false;
  if ((was_loading || was_revalidating) && s_entry_us != 0) {
    STICKY_LOGI(kTag,
                "entry_timing point=load_done mode=%s changed=%u status=%d t_ms=%ld",
                was_revalidating ? "revalidate" : "cold",
                static_cast<unsigned>(changed), static_cast<int>(status),
                entry_elapsed_ms());
    // A quiet re-check that found nothing new draws nothing more; any other
    // read is followed by one more render.
    if (was_revalidating && !changed &&
        status == sticky_storage::SdRepositoryAdapterStatus::Ok) {
      s_entry_us = 0;
    } else {
      s_entry_close_after_render = true;
    }
  }
  // A re-check that differs is only applied on the week pages. In a setup
  // page the user is mid-flow, and changing the flow's profile there would
  // desynchronise what is drawn from what the hit test assumes; the flow then
  // keeps the older profile until the next entry re-reads the store.
  const bool apply_revalidated =
      was_revalidating && changed && s_flow.showing_week_pages();
  if (status != sticky_storage::SdRepositoryAdapterStatus::Ok) {
    s_storage_error = status;
  } else {
    s_storage_error = sticky_storage::SdRepositoryAdapterStatus::Idle;
    if (was_loading || apply_revalidated) {
      s_flow.profile_loaded(s_profile_store.found(), s_profile_store.profile());
    } else if (!was_revalidating) {
      s_flow.profile_saved();
      s_retry_save = false;
      STICKY_LOGI(kTag, "profile=save backend=microsd result=ok");
    }
  }
  if (s_reload_after_pending) {
    s_reload_after_pending = false;
    s_retry_save = false;
    start_profile_read();
    return true;
  }
  if (status != sticky_storage::SdRepositoryAdapterStatus::Ok) {
    // A queued write may have reached durable storage before an I/O error was
    // reported. Reload on retry instead of blindly replaying the old revision.
    s_retry_save = false;
    return true;
  }
  // The card agreed with the profile already on screen, or the user is in a
  // setup page: nothing to redraw, so no further render will close the window.
  if (was_revalidating && !apply_revalidated) {
    s_entry_us = 0;
    return false;
  }
  settle_flow_after_profile();
  return true;
}

void retry_profile_storage() {
  if (!s_retry_save) mark_entry("retry");  // a retried read, not a save
  s_storage_error = s_retry_save
                        ? s_profile_store.begin_save(s_flow.pending_profile())
                        : s_profile_store.begin_load();
  s_profile_saving = s_retry_save &&
      s_storage_error == sticky_storage::SdRepositoryAdapterStatus::Pending;
  s_profile_loading = !s_retry_save &&
      s_storage_error == sticky_storage::SdRepositoryAdapterStatus::Pending;
  if (s_profile_loading || s_profile_saving)
    s_storage_error = sticky_storage::SdRepositoryAdapterStatus::Idle;
  // A read that could not even start has no load_done to close its window.
  if (!s_retry_save && !s_profile_loading) s_entry_us = 0;
}

// After a partial page change, repaints the current page with one full refresh
// once the user has been idle for kGhostCleanupDelayUs.
void run_ghost_cleanup() {
  if (!s_ghost_cleanup_pending ||
      esp_timer_get_time() < s_ghost_cleanup_due_us)
    return;
  if (status_page_active() || s_profile_store.pending()) {
    defer_ghost_cleanup();
    return;
  }
  STICKY_LOGI(kTag, "ghost_cleanup page=%s", pregnancy_page_name(s_flow.page()));
  render_page(false);
}

void update_for_date_rollover() {
  if (!s_flow.showing_week_pages() || esp_timer_get_time() < s_next_rtc_poll_us)
    return;
  s_next_rtc_poll_us = esp_timer_get_time() + kRtcPollIntervalUs;
  if (s_flow.refresh_for_new_day()) {
    render_page(true);
    STICKY_LOGI(kTag, "date=rollover week=%d day=%u result=updated",
                static_cast<int>(s_flow.progress().weeks),
                static_cast<unsigned>(s_flow.progress().days));
  }
}

void app_task(void *) {
  s_dashboard_requested.store(false);
  sticky_touch_clear_press();
  mark_entry("boot");
  initialize_state();
  render_page(false);
  while (true) {
    if (poll_profile_storage()) render_page(false);
    if (!s_profile_store.pending() &&
        s_profile_store.status() == sticky_storage::SdRepositoryAdapterStatus::Ok &&
        !s_profile_store.ready()) {
      s_flow.unconfigure();
      s_retry_save = false;
      start_profile_read();  // not ready(), so this is a cold read
      render_page(true);
    }
    if (sticky_app_lifecycle_checkpoint(s_lifecycle)) {
      mark_entry("resume");
      sticky_touch_clear_press();
      s_setup_after_load = s_setup_after_load || s_setup_requested.exchange(false);
      if (s_dashboard_requested.exchange(false) && s_flow.showing_week_pages())
        s_flow.handle_action(PregnancyAction::ShowOverview);
      if (s_profile_store.pending()) {
        s_reload_after_pending = true;
      } else {
        s_retry_save = false;
        start_profile_read();
      }
      render_page(false);
    }
    StickyTouchPress press = {};
    if (sticky_touch_take_press(press)) {
      defer_ghost_cleanup();
      if (s_profile_loading || s_profile_saving || s_profile_revalidating) {
        sticky_touch_clear_press();
        vTaskDelay(kPollInterval);
        continue;
      }
      if (s_storage_error !=
              sticky_storage::SdRepositoryAdapterStatus::Idle &&
          s_storage_error != sticky_storage::SdRepositoryAdapterStatus::Ok) {
        int x = 0, y = 0;
        s_canvas->physical_to_logical(press.x, press.y, x, y);
        if (prototype_ui::header_action_rect(
                s_canvas->width(), s_canvas->height()).contains(x, y)) {
          s_press.set(prototype_ui::header_action_rect(s_canvas->width(),
                                                       s_canvas->height()));
          show_press_feedback();
          sticky_app_request_launcher();
        } else if (storage_retry_rect().contains(x, y)) {
          s_press.set(storage_retry_rect());
          retry_profile_storage();
          render_page(true);
        }
        sticky_touch_clear_press();
        continue;
      }
      const PregnancyAction action = action_for_press(press);
      {
        // The tapped target comes from the same table the renderer drew.
        int x = 0, y = 0;
        s_canvas->physical_to_logical(press.x, press.y, x, y);
        STICKY_LOGI(kTag,
                    "tap physical=%d,%d logical=%d,%d canvas=%dx%d page=%s action=%s",
                    static_cast<int>(press.x), static_cast<int>(press.y), x, y,
                    static_cast<int>(s_canvas->width()),
                    static_cast<int>(s_canvas->height()),
                    pregnancy_page_name(s_flow.page()),
                    pregnancy_action_name(action));
        PregnancyTarget targets[kMaxPregnancyTargets];
        const size_t count = pregnancy_page_targets(
            s_flow.page(), s_flow.configured(), s_canvas->width(),
            s_canvas->height(), targets, kMaxPregnancyTargets);
        for (size_t index = 0U; index < count; ++index) {
          if (targets[index].rect.contains(x, y)) {
            s_press.set(targets[index].rect);
            break;
          }
        }
      }
      if (handle_action(action)) {
        STICKY_LOGI(kTag, "page=%s after action=%s",
                    pregnancy_page_name(s_flow.page()),
                    pregnancy_action_name(action));
        sticky_touch_clear_press();
        render_page(true);
      }
      s_press.clear();
    }
    update_for_date_rollover();
    run_ghost_cleanup();
    vTaskDelay(kPollInterval);
  }
}

} // namespace

esp_err_t pregnancy_app_start(Canvas &canvas) {
  app_log_register_tag(kTag);
  if (s_app_task != nullptr)
    return ESP_OK;
  s_canvas = &canvas;
  return xTaskCreate(app_task, "pregnancy_app", kTaskStackSize, nullptr,
                     kTaskPriority, &s_app_task) == pdPASS
             ? ESP_OK
             : ESP_ERR_NO_MEM;
}

void pregnancy_app_set_display_rotation(CanvasRotation rotation) {
  s_display_rotation = rotation;
}

void pregnancy_app_request_setup() { s_setup_requested.store(true); }
void pregnancy_app_request_dashboard() { s_dashboard_requested.store(true); }

esp_err_t pregnancy_app_pause() {
  return sticky_app_lifecycle_pause(s_lifecycle, s_app_task);
}

esp_err_t pregnancy_app_resume() {
  if (s_app_task == nullptr)
    return ESP_ERR_INVALID_STATE;
  sticky_app_lifecycle_resume(s_lifecycle);
  return ESP_OK;
}

esp_err_t pregnancy_app_prepare_power_sleep(uint32_t &current, uint32_t &next) {
  current = 0U;
  next = 0U;
  const esp_err_t paused = pregnancy_app_pause();
  if (paused != ESP_OK)
    return paused;
  StickyRtcDateTime clock = {};
  if (!read_rtc(clock))
    return ESP_OK;
  const PregnancyDate today = {clock.year, clock.month, clock.day};
  int32_t day_index = 0;
  if (!pregnancy_date_to_day_index(today, day_index))
    return ESP_OK;
  current = static_cast<uint32_t>(day_index) * 86400U +
            static_cast<uint32_t>(clock.hour) * 3600U +
            static_cast<uint32_t>(clock.minute) * 60U + clock.second;
  next = static_cast<uint32_t>(day_index + 1) * 86400U + 3U * 3600U;
  return ESP_OK;
}

uint32_t pregnancy_app_power_sleep_timeout_ms() {
  return s_flow.showing_week_pages() ? kDashboardSleepTimeoutMs : 0U;
}

bool pregnancy_app_draw_sleep_status(Canvas &canvas) {
  if (s_flow.page() != PregnancyPage::Dashboard || status_page_active())
    return false;
  int x = 0;
  int y = 0;
  pregnancy_page_status_battery_origin(canvas.height() > canvas.width(), x, y);
  battery_status_overlay_draw_compact(canvas, x, y);
  pregnancy_page_render_sleep_moon(canvas);
  return true;
}
