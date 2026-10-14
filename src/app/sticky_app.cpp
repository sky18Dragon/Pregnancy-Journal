#include "sticky_app.h"

#include <atomic>
#include <cstdint>

#include "app_log.h"
#include "app_manager.h"
#include "app_pages.h"
#include "app_registry.h"
#include "board_charger.h"
#include "board_power.h"
#include "canvas.h"
#include "domain_repository_stores.h"
#include "device_preferences_sync.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pregnancy_app.h"
#include "pregnancy_repository_store.h"
#include "scheduler.h"
#include "sticky_sleep_schedule.h"
#include "settings_app.h"
#include "sd_storage_service.h"
#include "sticky_app_display_orientation.h"
#include "sticky_app_gesture.h"
#include "sticky_button.h"
#include "sticky_buzzer.h"
#include "sticky_display.h"
#include "sticky_imu.h"
#include "sticky_rtc.h"
#include "sticky_touch.h"
#include "ui_language.h"

namespace {

constexpr char kTag[] = "sticky_app";
constexpr TickType_t kPollInterval = pdMS_TO_TICKS(40);
constexpr uint8_t kInitialImuSampleAttempts = 7U;
constexpr TickType_t kInitialImuSampleRetry = pdMS_TO_TICKS(10);
constexpr uint32_t kTaskStackSize = 4608U;
constexpr UBaseType_t kTaskPriority = 4U;
constexpr uint32_t kSleepContextMagic = 0x53434657U;
constexpr uint32_t kLauncherIdleCloseMs = 30000U;

struct StickySleepContext {
  uint32_t magic;
  StickyAppId app;
  CanvasRotation rotation;
};

RTC_NOINIT_ATTR StickySleepContext s_sleep_context;
Canvas *s_canvas = nullptr;
TaskHandle_t s_task = nullptr;
StickyAppManager s_manager(sticky_app_registry_data(),
                           sticky_app_registry_count(), StickyAppId::Pregnancy);
StickyScheduler s_scheduler;
bool s_launcher_open = false;
uint32_t s_last_activity_ms = 0U;
uint8_t s_sleep_failures = 0U;
uint32_t s_sleep_retry_at_ms = 0U;
std::atomic<int> s_requested_app{-1};
std::atomic<bool> s_launcher_requested{false};
std::atomic<bool> s_language_toggle_requested{false};
std::atomic<bool> s_application_data_deleted{false};
StickyImuOrientation s_last_orientation = StickyImuOrientation::Unknown;
DevicePreferencesSync s_device_preferences;
std::atomic<bool> s_preferences_save_failed{false};

void render_launcher(bool full_refresh = false);

uint32_t monotonic_ms() {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000LL);
}

// User input restarts the idle timer and clears any sleep retry back-off.
void mark_activity(uint32_t at_ms) {
  s_last_activity_ms = at_ms;
  s_sleep_failures = 0U;
}

bool sleep_retry_allowed(uint32_t now_ms) {
  return s_sleep_failures == 0U ||
         static_cast<int32_t>(now_ms - s_sleep_retry_at_ms) >= 0;
}

template <typename Store>
bool reload_repository_store(Store &store, const char *name) {
  constexpr unsigned kMaximumPolls = 125U;
  bool load_started = false;
  sticky_storage::SdRepositoryAdapterStatus status = store.status();
  for (unsigned poll = 0U; poll < kMaximumPolls; ++poll) {
    if (store.pending()) {
      status = store.poll();
      if (status == sticky_storage::SdRepositoryAdapterStatus::Pending) {
        vTaskDelay(kPollInterval);
        continue;
      }
      if (!load_started) continue;
      if (status == sticky_storage::SdRepositoryAdapterStatus::Ok &&
          store.ready()) return true;
      break;
    }
    if (!load_started) {
      status = store.begin_load();
      load_started =
          status == sticky_storage::SdRepositoryAdapterStatus::Pending;
      if (load_started) continue;
      const sticky_storage::SdStorageServiceStatus storage =
          sticky_storage::sd_storage_service_status();
      if (storage.card_state == sticky_storage::SdCardState::Mounting ||
          (storage.running && !storage.manifest_known) ||
          (storage.manifest_selected && !storage.recovery_known)) {
        vTaskDelay(kPollInterval);
        continue;
      }
      break;
    }
  }
  STICKY_LOGW(kTag, "repository=%s reload=%u result=unavailable", name,
              static_cast<unsigned>(status));
  return false;
}

void configure_app_rotation(StickyAppId app, CanvasRotation rotation) {
  if (app == StickyAppId::Pregnancy)
    pregnancy_app_set_display_rotation(rotation);
}

CanvasRotation rotation_for_app(StickyAppId app,
                                StickyImuOrientation orientation,
                                CanvasRotation fallback) {
  const StickyAppDescriptor *descriptor = sticky_app_registry_find(app);
  CanvasRotation rotation = fallback;
  if (descriptor != nullptr) {
    sticky_app_display_rotation(descriptor->rotation, orientation, fallback,
                                rotation);
  }
  return rotation;
}

StickyImuState read_initial_imu_state() {
  StickyImuState state = {};
  for (uint8_t attempt = 0U; attempt < kInitialImuSampleAttempts; ++attempt) {
    if (sticky_imu_get_state(state) == ESP_OK && state.valid)
      return state;
    vTaskDelay(kInitialImuSampleRetry);
  }
  return {};
}

void poll_device_settings() {
  const bool previously_failed = s_device_preferences.saving_failed();
  s_device_preferences.tick(
      pregnancy_repository::device_preferences_repository_store(),
      sticky_storage::sd_storage_service_status(),
      monotonic_ms());
  s_preferences_save_failed.store(s_device_preferences.saving_failed());
  const UiLanguage previous = ui_language_get();
  ui_language_set(s_device_preferences.desired().language);
  if (previous != ui_language_get()) {
    if (s_launcher_open) render_launcher();
    settings_app_notify_preferences_changed();
  }
  if (previously_failed != s_device_preferences.saving_failed()) {
    STICKY_LOGW(kTag, "preferences=sync retry_pending=%u",
                s_device_preferences.saving_failed() ? 1U : 0U);
    settings_app_notify_preferences_changed();
  }
}

void persist_current_app() {
  s_device_preferences.set_last_app(s_manager.current_id());
}

void toggle_language() {
  s_device_preferences.toggle_language();
}

void render_launcher(bool full_refresh) {
  sticky_display_set_battery_overlay_enabled(false);
  CanvasRotation rotation = s_canvas->rotation();
  sticky_app_launcher_rotation(s_last_orientation, rotation, rotation);
  s_canvas->set_rotation(rotation);
  app_page_render_launcher(*s_canvas, s_manager.current_id());
  const esp_err_t result = full_refresh ? sticky_display_refresh_monochrome()
                                        : sticky_display_refresh_partial();
  if (result != ESP_OK) {
    STICKY_LOGE(kTag, "launcher=render result=%s", esp_err_to_name(result));
  }
}

bool open_launcher() {
  if (s_launcher_open)
    return true;
  const esp_err_t result = s_manager.pause_current();
  if (result != ESP_OK)
    return false;
  sticky_touch_clear_press();
  sticky_touch_clear_interaction();
  s_launcher_open = true;
  render_launcher();
  STICKY_LOGI(kTag, "launcher=open current=%s result=ok",
              sticky_app_id_name(s_manager.current_id()));
  return true;
}

void close_launcher() {
  if (!s_launcher_open)
    return;
  s_launcher_open = false;
  sticky_touch_clear_press();
  sticky_touch_clear_interaction();
  const CanvasRotation rotation =
      rotation_for_app(s_manager.current_id(), s_last_orientation,
                       s_canvas->rotation());
  s_canvas->set_rotation(rotation);
  configure_app_rotation(s_manager.current_id(), rotation);
  const esp_err_t result = s_manager.resume_current();
  STICKY_LOGI(kTag, "launcher=close current=%s result=%s",
              sticky_app_id_name(s_manager.current_id()),
              esp_err_to_name(result));
}

void select_app(StickyAppId app, const char *source) {
  const StickyAppId previous = s_manager.current_id();
  const CanvasRotation previous_rotation = s_canvas->rotation();
  const CanvasRotation destination_rotation =
      rotation_for_app(app, s_last_orientation, previous_rotation);
  if (!s_launcher_open && s_manager.pause_current() != ESP_OK) return;
  s_canvas->set_rotation(destination_rotation);
  configure_app_rotation(app, destination_rotation);
  const esp_err_t result = s_manager.switch_from_paused_to(app);
  if (result != ESP_OK) {
    s_launcher_open = false;
    if (s_manager.pause_current() == ESP_OK) {
      s_canvas->set_rotation(previous_rotation);
      configure_app_rotation(previous, previous_rotation);
      s_manager.resume_current();
    }
    STICKY_LOGE(kTag, "app=switch from=%s to=%s source=%s result=%s",
                sticky_app_id_name(previous), sticky_app_id_name(app), source,
                esp_err_to_name(result));
    return;
  }
  s_launcher_open = false;
  sticky_touch_clear_press();
  sticky_touch_clear_interaction();
  persist_current_app();
  STICKY_LOGI(kTag, "app=switch from=%s to=%s source=%s result=ok",
              sticky_app_id_name(previous),
              sticky_app_id_name(s_manager.current_id()), source);
}

void update_display_orientation() {
  StickyImuState state = {};
  if (sticky_imu_get_state(state) != ESP_OK || !state.valid || state.moving ||
      state.orientation == StickyImuOrientation::Unknown ||
      state.orientation == s_last_orientation) {
    return;
  }

  s_last_orientation = state.orientation;
  if (s_launcher_open) {
    CanvasRotation rotation = s_canvas->rotation();
    if (sticky_app_launcher_rotation(s_last_orientation, rotation, rotation) &&
        rotation != s_canvas->rotation()) {
      s_canvas->set_rotation(rotation);
      render_launcher(true);
    }
    return;
  }

  const StickyAppId app = s_manager.current_id();
  const CanvasRotation current = s_canvas->rotation();
  const CanvasRotation rotation =
      rotation_for_app(app, s_last_orientation, current);
  if (rotation == current)
    return;

  const esp_err_t pause = s_manager.pause_current();
  if (pause != ESP_OK) {
    STICKY_LOGE(kTag, "orientation app=%s pause=%s result=failed",
                sticky_app_id_name(app), esp_err_to_name(pause));
    return;
  }
  s_canvas->set_rotation(rotation);
  configure_app_rotation(app, rotation);
  const esp_err_t resume = s_manager.resume_current();
  STICKY_LOGI(kTag,
              "orientation app=%s imu=%s display_rotation=%s result=%s",
              sticky_app_id_name(app),
              sticky_imu_orientation_name(s_last_orientation),
              sticky_app_display_rotation_name(rotation),
              esp_err_to_name(resume));
}

void draw_sleep_indicator() {
  const int left = static_cast<int>(s_canvas->width()) - 45;
  constexpr int top = 10;
  s_canvas->fill_rect(left, top, 35, 35, GrayLevel::White);
  s_canvas->fill_circle(left + 16, top + 17, 11, GrayLevel::Black);
  s_canvas->fill_circle(left + 21, top + 12, 10, GrayLevel::White);
}

void enter_sleep(const char *source) {
  const StickyAppDescriptor *app = s_manager.current();
  if (app == nullptr || !app->sleep_allowed())
    return;
  const bool launcher_was_open = s_launcher_open;
  const auto rollback = [&]() {
    if (s_sleep_failures < UINT8_MAX) ++s_sleep_failures;
    const uint32_t delay_ms = sticky_sleep_retry_delay_ms(s_sleep_failures);
    s_sleep_retry_at_ms = monotonic_ms() + delay_ms;
    STICKY_LOGW(kTag, "power=sleep source=%s failures=%u retry_ms=%u "
                "result=rolled_back",
                source, static_cast<unsigned>(s_sleep_failures),
                static_cast<unsigned>(delay_ms));
    if (launcher_was_open) {
      s_launcher_open = true;
      render_launcher(true);
    } else {
      s_manager.resume_current();
    }
  };
  uint32_t now = 0U;
  uint32_t next = 0U;
  const esp_err_t prepare = app->prepare_sleep(now, next);
  if (prepare != ESP_OK) {
    rollback();
    return;
  }

  if (now == 0U) {
    StickyRtcDateTime clock = {};
    if (sticky_rtc_read(clock) != ESP_OK ||
        !sticky_rtc_epoch_seconds(clock, now)) {
      rollback();
      return;
    }
  }

  sticky_sleep_schedule_configure(s_scheduler, now, next);
  const uint64_t wake_us = sticky_sleep_schedule_wake_us(s_scheduler, now);

  const esp_err_t storage_prepare =
      sticky_storage::sd_storage_service_prepare_sleep();
  if (storage_prepare != ESP_OK) {
    STICKY_LOGE(kTag,
                "power=sleep source=%s storage=%s result=blocked",
                source, esp_err_to_name(storage_prepare));
    rollback();
    return;
  }

  s_launcher_open = false;
  s_sleep_context = {kSleepContextMagic, s_manager.current_id(),
                     s_canvas->rotation()};
  sticky_display_set_battery_overlay_enabled(true);
  sticky_display_set_battery_overlay_sleep_layout(true);
  draw_sleep_indicator();
  sticky_display_cancel_app_transition_refresh();
  sticky_display_refresh_monochrome();
  sticky_buzzer_stop();
  sticky_touch_stop();
  sticky_imu_stop_monitoring();
  sticky_display_sleep();
  STICKY_LOGI(kTag, "power=sleep source=%s app=%s wake_us=%llu result=ready",
              source, sticky_app_id_name(s_manager.current_id()),
              static_cast<unsigned long long>(wake_us));
  board_power_enter_deep_sleep(wake_us);
}

void handle_interaction(const StickyTouchInteraction &interaction) {
  int sx = 0, sy = 0, ex = 0, ey = 0;
  s_canvas->physical_to_logical(interaction.start_x, interaction.start_y, sx,
                                sy);
  s_canvas->physical_to_logical(interaction.end_x, interaction.end_y, ex, ey);
  const StickyAppGestureSample sample = {
      static_cast<int>(s_canvas->width()),
      static_cast<int>(s_canvas->height()),
      sx,
      sy,
      ex,
      ey,
      interaction.ended_at_ms - interaction.started_at_ms,
      s_launcher_open,
  };
  const StickyAppGestureAction action = sticky_app_gesture_classify(sample);
  if (action == StickyAppGestureAction::OpenLauncher)
    open_launcher();
  else if (action == StickyAppGestureAction::CloseLauncher)
    close_launcher();
}

void handle_launcher_press(const StickyTouchPress &press) {
  int x = 0, y = 0;
  s_canvas->physical_to_logical(press.x, press.y, x, y);
  if (app_page_launcher_close_at(s_canvas->width(), s_canvas->height(), x, y)) {
    close_launcher();
    return;
  }
  if (app_page_launcher_language_at(s_canvas->width(), s_canvas->height(), x,
                                    y)) {
    toggle_language();
    return;
  }
  StickyAppId selected = StickyAppId::Pregnancy;
  if (app_page_launcher_app_at(s_canvas->width(), s_canvas->height(), x, y,
                               selected)) {
    select_app(selected, "touch");
  }
}

void app_task(void *) {
  while (true) {
    if (s_application_data_deleted.exchange(false)) {
      reload_repository_store(pregnancy_repository::pregnancy_repository_store(),
                              "pregnancy-after-delete");
      reload_repository_store(pregnancy_repository::kick_repository_store(),
                              "kicks-after-delete");
      reload_repository_store(pregnancy_repository::weight_repository_store(),
                              "weight-after-delete");
      reload_repository_store(
          pregnancy_repository::device_preferences_repository_store(),
          "preferences-after-delete");
      reload_repository_store(
          pregnancy_repository::network_authorization_repository_store(),
          "network-authorization-after-delete");
      s_scheduler.cancel(1U);
      s_scheduler.cancel(2U);
      s_scheduler.cancel(3U);
      s_scheduler.cancel(4U);
      s_device_preferences = {};
      ui_language_set(UiLanguage::English);
      settings_app_notify_preferences_changed();
      STICKY_LOGI(kTag,
                  "storage=application-data-deleted caches=invalidated");
    }
    poll_device_settings();
    if (s_language_toggle_requested.exchange(false)) toggle_language();
    update_display_orientation();
    StickyButtonEvent event = StickyButtonEvent::None;
    if (sticky_button_take_event(event)) {
      mark_activity(monotonic_ms());
      sticky_buzzer_stop();
      if (event == StickyButtonEvent::SingleClick) {
        if (s_launcher_open)
          close_launcher();
        else
          open_launcher();
      } else if (event == StickyButtonEvent::DoubleClick) {
        select_app(StickyAppId::Pregnancy, "double_click");
      } else if (event == StickyButtonEvent::SleepChord) {
        enter_sleep("side_button_chord");
      }
    }

    StickyTouchInteraction interaction = {};
    while (sticky_touch_take_interaction(interaction)) {
      mark_activity(interaction.ended_at_ms);
      sticky_buzzer_stop();
      handle_interaction(interaction);
    }
    if (s_launcher_open) {
      StickyTouchPress press = {};
      if (sticky_touch_take_press(press)) {
        mark_activity(press.captured_at_ms);
        handle_launcher_press(press);
      }
    } else if (s_launcher_requested.exchange(false)) {
      open_launcher();
    } else if (s_requested_app.load() >= 0) {
      const int requested = s_requested_app.exchange(-1);
      const StickyAppId requested_app = static_cast<StickyAppId>(requested);
      if (sticky_app_id_valid(requested_app))
        select_app(requested_app, "app_request");
    } else if (s_manager.current_id() == StickyAppId::Settings &&
               settings_app_take_home_request()) {
      select_app(StickyAppId::Pregnancy, "settings");
    } else if (s_manager.current_id() == StickyAppId::Settings &&
               settings_app_take_pregnancy_request()) {
      pregnancy_app_request_setup();
      select_app(StickyAppId::Pregnancy, "settings");
    }

    const uint32_t now_ms = monotonic_ms();
    if (s_launcher_open &&
        now_ms - s_last_activity_ms >= kLauncherIdleCloseMs) {
      close_launcher();
    }
    const StickyAppDescriptor *app = s_manager.current();
    if (!s_launcher_open && app != nullptr && app->sleep_timeout_ms() > 0U &&
        app->sleep_allowed() && !board_charger_external_power_present() &&
        now_ms - s_last_activity_ms >= app->sleep_timeout_ms() &&
        sleep_retry_allowed(now_ms)) {
      enter_sleep("idle_timeout");
    }
    vTaskDelay(kPollInterval);
  }
}

} // namespace

esp_err_t sticky_app_start(Canvas &canvas) {
  app_log_register_tag(kTag);
  if (s_task != nullptr)
    return ESP_OK;
  s_canvas = &canvas;
  esp_err_t result = sticky_button_init();
  if (result != ESP_OK)
    return result;

  // RTC_NOINIT memory survives deep sleep but not power loss; validate every
  // field instead of trusting the magic value alone.
  const bool valid_context =
      s_sleep_context.magic == kSleepContextMagic &&
      sticky_app_registry_find(s_sleep_context.app) != nullptr &&
      static_cast<uint8_t>(s_sleep_context.rotation) <=
          static_cast<uint8_t>(CanvasRotation::Deg90CounterClockwise);
  reload_repository_store(pregnancy_repository::pregnancy_repository_store(),
                          "pregnancy");
  StickyAppId initial =
      valid_context
          ? s_sleep_context.app
          : (pregnancy_repository::device_preferences_repository_store().ready()
                 ? pregnancy_repository::device_preferences_repository_store()
                       .settings()
                       .last_app
                 : StickyAppId::Pregnancy);
  // A stored V2-only app must resolve before applying its rotation policy.
  if (sticky_app_registry_find(initial) == nullptr)
    initial = StickyAppId::Pregnancy;
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
    StickyRtcDateTime clock = {};
    if (sticky_rtc_read(clock) == ESP_OK &&
        sticky_sleep_schedule_is_daily_window(clock.hour, clock.minute))
      initial = StickyAppId::Pregnancy;
  }
  const CanvasRotation fallback_rotation =
      valid_context ? s_sleep_context.rotation : CanvasRotation::Deg0;
  s_sleep_context.magic = 0U;
  canvas.set_rotation(fallback_rotation);
  const esp_err_t imu_result = sticky_imu_start_monitoring();
  if (imu_result == ESP_OK) {
    const StickyImuState state = read_initial_imu_state();
    if (state.valid) {
      s_last_orientation = state.orientation != StickyImuOrientation::Unknown
                               ? state.orientation
                               : state.observed_orientation;
    }
  } else {
    STICKY_LOGW(kTag, "orientation monitor=%s result=disabled",
                esp_err_to_name(imu_result));
  }
  const CanvasRotation initial_rotation =
      rotation_for_app(initial, s_last_orientation, fallback_rotation);
  canvas.set_rotation(initial_rotation);
  configure_app_rotation(initial, initial_rotation);
  result = s_manager.start(canvas, initial);
  if (result != ESP_OK) {
    sticky_imu_stop_monitoring();
    return result;
  }
  mark_activity(monotonic_ms());
  if (xTaskCreate(app_task, "app_coordinator", kTaskStackSize, nullptr,
                  kTaskPriority, &s_task) != pdPASS) {
    sticky_imu_stop_monitoring();
    return ESP_ERR_NO_MEM;
  }
  STICKY_LOGI(kTag,
              "runtime=ready apps=%u default=pregnancy current=%s result=ok",
              static_cast<unsigned>(sticky_app_registry_count()),
              sticky_app_id_name(initial));
  return ESP_OK;
}

void sticky_app_request(StickyAppId app) {
  if (sticky_app_id_valid(app))
    s_requested_app.store(static_cast<int>(app));
}

void sticky_app_request_launcher() { s_launcher_requested.store(true); }

void sticky_app_request_language_toggle() {
  s_language_toggle_requested.store(true);
}

void sticky_app_notify_application_data_deleted() {
  s_application_data_deleted.store(true);
}

bool sticky_app_preferences_save_failed() {
  return s_preferences_save_failed.load();
}
