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
#include "kick_app.h"
#include "pregnancy_app.h"
#include "press_feedback.h"
#include "pregnancy_repository_store.h"
#include "scheduler.h"
#include "sticky_sleep_schedule.h"
#include "settings_app.h"
#include "weight_app.h"
#include "sd_storage_service.h"
#include "sticky_app_display_orientation.h"
#include "sticky_app_gesture.h"
#include "sticky_button.h"
#include "sticky_buzzer.h"
#include "sticky_key_router.h"
#include "sticky_shortcut.h"
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
// Keys pressed this soon after a key-driven page change are dropped: the panel
// is still redrawing and the press was most likely impatience, not a command.
constexpr int64_t kTransitionSettleUs = 1000000LL;
constexpr size_t kKeyPolicySlots = 16U;

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
// Mirror of the saved kick method for other tasks, and a pick waiting to be
// applied on the coordinator task (-1 = none).
std::atomic<uint8_t> s_kick_mode_pref{static_cast<uint8_t>(StickyKickModePref::Unset)};
std::atomic<int> s_kick_mode_request{-1};
std::atomic<uint8_t> s_shortcut_target{static_cast<uint8_t>(StickyShortcutTarget::WeightRecordToday)};
std::atomic<int> s_shortcut_request{-1};
KeyRouter s_key_router;
SideStepMerger s_side_merger;
// Policy of the page each app is showing, indexed by StickyAppId value.
std::atomic<uint8_t> s_key_policies[kKeyPolicySlots] = {};

void render_launcher(bool full_refresh = true);

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
  // A pick that is waiting to be applied must not be overwritten by the old value.
  if (s_kick_mode_request.load() < 0)
    s_kick_mode_pref.store(
        static_cast<uint8_t>(s_device_preferences.desired().kick_mode_pref));
  if (s_shortcut_request.load() < 0)
    s_shortcut_target.store(
        static_cast<uint8_t>(s_device_preferences.desired().shortcut_target));
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

// The launcher replaces the whole page, so every render defaults to a full refresh.
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
  const bool app_drew_status = !launcher_was_open &&
                               app->draw_sleep_status != nullptr &&
                               app->draw_sleep_status(*s_canvas);
  sticky_display_set_battery_overlay_enabled(!app_drew_status);
  sticky_display_set_battery_overlay_sleep_layout(true);
  if (!app_drew_status)
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

// Black bar along the bottom edge: acknowledges a swipe or side key during the
// ~2 s full refresh that follows.
void draw_switch_ack() {
  const int bar_height = 8;
  press_feedback::draw(*s_canvas,
                       {0, static_cast<int>(s_canvas->height()) - bar_height,
                        static_cast<int>(s_canvas->width()), bar_height});
  sticky_display_refresh_partial();
}

// Goes straight to the app `steps` places along the launcher order (negative =
// backwards, wrapping) without opening the launcher.
void switch_by_steps(int steps, const char *source) {
  StickyAppId target = s_manager.current_id();
  const int direction = steps < 0 ? -1 : 1;
  for (int count = steps < 0 ? -steps : steps; count > 0; --count)
    target = app_page_launcher_neighbor(target, direction);
  if (target == s_manager.current_id()) return;
  draw_switch_ack();
  select_app(target, source);
}

// Bottom-edge horizontal swipe: next/previous app.
void switch_to_neighbor_app(int direction) {
  switch_by_steps(direction, "swipe");
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
  else if (action == StickyAppGestureAction::NextApp ||
           action == StickyAppGestureAction::PreviousApp)
    switch_to_neighbor_app(action == StickyAppGestureAction::NextApp ? 1 : -1);
}

void handle_launcher_press(const StickyTouchPress &press) {
  int x = 0, y = 0;
  s_canvas->physical_to_logical(press.x, press.y, x, y);
  LauncherTarget targets[8];
  const size_t count = app_page_launcher_targets(
      s_canvas->width(), s_canvas->height(), targets,
      sizeof(targets) / sizeof(targets[0]));
  for (size_t index = 0U; index < count; ++index) {
    if (!targets[index].rect.contains(x, y)) continue;
    // The launcher hands over to a full refresh (~2 s). Show the tapped
    // target pressed first so the touch is acknowledged on the panel.
    press_feedback::draw(*s_canvas, targets[index].rect);
    sticky_display_refresh_partial();
    if (targets[index].close)
      close_launcher();
    else
      select_app(targets[index].app, "touch");
    return;
  }
}

KeyPolicy current_key_policy() {
  const unsigned slot = static_cast<unsigned>(s_manager.current_id());
  if (slot >= kKeyPolicySlots) return KeyPolicy::Locked;
  return static_cast<KeyPolicy>(s_key_policies[slot].load());
}

// TODO(hardware): which turned-screen layouts put the physical left key on the
// viewer's right is unconfirmed; only the upside-down layout is mirrored.
bool side_keys_flipped() {
  return s_canvas->rotation() == CanvasRotation::Deg180;
}

// After a key-driven page change, drop keys pressed while the panel redraws.
void settle_after_key_transition() {
  s_key_router.ignore_events_before(esp_timer_get_time() + kTransitionSettleUs);
}

// Baby Week on its dashboard, whatever page it was left on.
void go_home(const char *source) {
  pregnancy_app_request_dashboard();
  select_app(StickyAppId::Pregnancy, source);
}

// The double-click target chosen in Settings. The app gets the entry request
// before it is resumed, so it lands on the target page with a single refresh.
void run_shortcut() {
  const ShortcutAction action = shortcut_action(
      sticky_app_shortcut_target(), s_manager.current_id(),
      current_key_policy());
  switch (action) {
  case ShortcutAction::None:
    // Already at the target: just close the launcher if it was open. With the
    // shortcut off the key does nothing at all.
    if (s_launcher_open &&
        sticky_app_shortcut_target() != StickyShortcutTarget::Off)
      close_launcher();
    break;
  case ShortcutAction::GoHome:
    go_home("double_click");
    break;
  case ShortcutAction::OpenWeightToday:
    weight_app_request_record_today();
    select_app(shortcut_app(action), "double_click");
    break;
  case ShortcutAction::OpenKickStart:
    kick_app_request_start();
    select_app(shortcut_app(action), "double_click");
    break;
  }
}

void handle_button_record(const StickyButtonRecord &record) {
  mark_activity(monotonic_ms());
  sticky_buzzer_stop();
  if (record.event == StickyButtonEvent::SleepChord) {
    enter_sleep("side_button_chord");
    return;
  }
  KeyInput input;
  switch (record.event) {
  case StickyButtonEvent::PressDown: input = KeyInput::TopPress; break;
  case StickyButtonEvent::SingleClick: input = KeyInput::TopSingle; break;
  case StickyButtonEvent::DoubleClick: input = KeyInput::TopDouble; break;
  case StickyButtonEvent::SideSingle:
    input = record.side == SideKey::Right ? KeyInput::SideRight
                                          : KeyInput::SideLeft;
    break;
  default:
    return;
  }

  KeyContext context;
  context.launcher_open = s_launcher_open;
  context.policy = current_key_policy();
  context.side_keys_flipped = side_keys_flipped();
  const KeyDecision decision =
      s_key_router.route({input, record.at_us}, context);
  if (decision.action != KeyAction::SwitchApp) s_side_merger.clear();
  switch (decision.action) {
  case KeyAction::OpenLauncher:
    open_launcher();
    settle_after_key_transition();
    break;
  case KeyAction::CloseLauncher:
    close_launcher();
    settle_after_key_transition();
    break;
  case KeyAction::GoHome:
    go_home("top_click");
    settle_after_key_transition();
    break;
  case KeyAction::RunShortcut:
    run_shortcut();
    settle_after_key_transition();
    break;
  case KeyAction::SwitchApp:
    s_side_merger.add(decision.step, record.at_us);
    break;
  case KeyAction::CapturePress: {
    const StickyAppDescriptor *app = s_manager.current();
    if (app != nullptr && app->capture_press != nullptr)
      app->capture_press(record.at_us);
    break;
  }
  case KeyAction::None:
  case KeyAction::PageFlip:
    // PageFlip has no consumer yet (no Pager page).
    break;
  }
}

// Runs the app switch collected from quick side-key presses once they settle.
void apply_pending_side_steps() {
  int steps = 0;
  if (!s_side_merger.take(esp_timer_get_time(), steps) || steps == 0) return;
  const KeyPolicy policy = current_key_policy();
  if (s_launcher_open ||
      (policy != KeyPolicy::Free && policy != KeyPolicy::Home))
    return;
  switch_by_steps(steps, "side_key");
  settle_after_key_transition();
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
      s_kick_mode_request.store(-1);
      s_shortcut_request.store(-1);
      s_shortcut_target.store(
          static_cast<uint8_t>(StickyShortcutTarget::WeightRecordToday));
      s_kick_mode_pref.store(static_cast<uint8_t>(StickyKickModePref::Unset));
      ui_language_set(UiLanguage::English);
      settings_app_notify_preferences_changed();
      STICKY_LOGI(kTag,
                  "storage=application-data-deleted caches=invalidated");
    }
    poll_device_settings();
    if (s_language_toggle_requested.exchange(false)) toggle_language();
    const int kick_mode_request = s_kick_mode_request.exchange(-1);
    if (kick_mode_request >= 0)
      s_device_preferences.set_kick_mode_pref(
          static_cast<StickyKickModePref>(kick_mode_request));
    const int shortcut_request = s_shortcut_request.exchange(-1);
    if (shortcut_request >= 0)
      s_device_preferences.set_shortcut_target(
          static_cast<StickyShortcutTarget>(shortcut_request));
    update_display_orientation();
    StickyButtonRecord record;
    while (sticky_button_take_record(record)) handle_button_record(record);
    apply_pending_side_steps();

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

void sticky_app_set_key_policy(StickyAppId app, KeyPolicy policy) {
  const unsigned slot = static_cast<unsigned>(app);
  if (slot < kKeyPolicySlots)
    s_key_policies[slot].store(static_cast<uint8_t>(policy));
}

StickyKickModePref sticky_app_kick_mode_pref() {
  return static_cast<StickyKickModePref>(s_kick_mode_pref.load());
}

StickyShortcutTarget sticky_app_shortcut_target() {
  const uint8_t value = s_shortcut_target.load();
  return value < kStickyShortcutTargetCount
             ? static_cast<StickyShortcutTarget>(value)
             : StickyShortcutTarget::WeightRecordToday;
}

void sticky_app_set_shortcut_target(StickyShortcutTarget target) {
  if (static_cast<uint8_t>(target) >= kStickyShortcutTargetCount) return;
  s_shortcut_target.store(static_cast<uint8_t>(target));
  s_shortcut_request.store(static_cast<int>(target));
}

void sticky_app_set_kick_mode_pref(StickyKickModePref pref) {
  s_kick_mode_pref.store(static_cast<uint8_t>(pref));
  s_kick_mode_request.store(static_cast<int>(pref));
}

void sticky_app_request_language_toggle() {
  s_language_toggle_requested.store(true);
}

void sticky_app_notify_application_data_deleted() {
  s_application_data_deleted.store(true);
}

bool sticky_app_preferences_save_failed() {
  return s_preferences_save_failed.load();
}
