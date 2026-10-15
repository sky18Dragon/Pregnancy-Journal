#include "settings_app.h"

#include <cstdio>
#include <cstring>
#include <atomic>

#include "app_log.h"
#include "bound_credentials.h"
#include "canvas.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "legacy_data_migration.h"
#include "legacy_nvs_source.h"
#include "pregnancy_repository_store.h"
#include "sd_storage_service.h"
#include "settings_flow.h"
#include "settings_pages.h"
#include "sticky_app.h"
#include "sticky_app_lifecycle.h"
#include "sticky_display.h"
#include "press_feedback.h"
#include "refresh_policy.h"
#include "sticky_rtc.h"
#include "sticky_touch.h"
#include "ui_language.h"
#include "wifi_ram_configuration.h"

namespace {

using namespace sticky_storage;

constexpr char kTag[] = "settings_app";
constexpr uint32_t kSleepTimeoutMs = 3U * 60U * 1000U;
Canvas *s_canvas = nullptr;
TaskHandle_t s_task = nullptr;
StickyAppLifecycle s_lifecycle = {};
SettingsStoragePageState s_displayed_storage = {};
bool s_home_requested = false;
bool s_pregnancy_requested = false;
std::atomic<bool> s_preferences_changed{false};
PregnancyProfile s_profile = {};
PregnancyProgress s_progress = {};
bool s_profile_configured = false;
char s_storage_dataset_id[33] = {};
SdStorageRequestHandle s_storage_request = {};
SdDataDeleteConfirmation s_delete_confirmation = {};
bool s_storage_request_pending = false;
LegacyDataMigration s_common_migration(legacy_nvs_source());
LegacyDataMigration s_kick_migration(legacy_kick_nvs_source());

// The services behind the flow's port (defined after the helpers below).
class SettingsPort final : public SettingsFlowPort {
public:
  bool read_clock(SettingsClockTime &time) override;
  bool write_clock(const SettingsClockTime &time) override;
  SettingsStoragePageState storage_state() override;
  void prepare_initialization() override;
  bool has_dataset_id() const override;
  void forget_dataset_id() override;
  const char *submit_initialization() override;
  const char *safe_eject() override;
  bool migration_pending() override;
  bool migration_complete() override;
  void begin_import() override;
  void clear_old_copies() override;
  void prepare_delete() override;
  const char *delete_card_data(bool enabled, bool &deleted) override;
  const char *reset_device_binding() override;
};

SettingsPort s_port;
SettingsFlow s_flow(s_port);

LegacyDataMigration &active_migration() {
  return s_flow.migrating_kicks() ? s_kick_migration : s_common_migration;
}

SettingsStoragePageState storage_page_state() {
  const SdStorageServiceStatus status = sd_storage_service_status();
  SettingsStoragePageState state = {};
  state.mount_generation = status.mount_generation;
  switch (status.card_state) {
  case SdCardState::ReadyReadWrite:
    state.card = SettingsStorageCardView::ReadWrite;
    break;
  case SdCardState::ReadyReadOnly:
    state.card = SettingsStorageCardView::ReadOnly;
    break;
  case SdCardState::Full:
    state.card = SettingsStorageCardView::Full;
    break;
  case SdCardState::Absent:
  case SdCardState::Unmounted:
    state.card = SettingsStorageCardView::Missing;
    break;
  case SdCardState::Mounting:
  case SdCardState::Fault:
  case SdCardState::Ejecting:
    state.card = SettingsStorageCardView::Unavailable;
    break;
  }

  if (!status.manifest_known) {
    state.dataset = SettingsStorageDatasetView::Unknown;
  } else {
    switch (status.manifest_state) {
    case SdManifestProbeState::NotInitialized:
      state.dataset = SettingsStorageDatasetView::NotInitialized;
      break;
    case SdManifestProbeState::Ready:
      state.dataset = SettingsStorageDatasetView::Ready;
      break;
    case SdManifestProbeState::Degraded:
      state.dataset = SettingsStorageDatasetView::Degraded;
      break;
    case SdManifestProbeState::Corrupt:
      state.dataset = SettingsStorageDatasetView::Corrupt;
      break;
    case SdManifestProbeState::IoError:
      state.dataset = SettingsStorageDatasetView::ReadError;
      break;
    }
  }
  state.requests_open = status.running &&
                        status.accepting_manifest_requests &&
                        !s_storage_request_pending;
  const bool dataset_usable =
      state.dataset == SettingsStorageDatasetView::Ready ||
      state.dataset == SettingsStorageDatasetView::Degraded;
  state.initialize_enabled =
      state.card == SettingsStorageCardView::ReadWrite &&
      state.dataset == SettingsStorageDatasetView::NotInitialized &&
      state.requests_open;
  state.eject_enabled =
      (state.card == SettingsStorageCardView::ReadWrite ||
       state.card == SettingsStorageCardView::ReadOnly ||
       state.card == SettingsStorageCardView::Full) &&
      status.pending_work == 0U && !s_storage_request_pending &&
      !active_migration().pending();
  state.migration_enabled =
      state.card == SettingsStorageCardView::ReadWrite &&
      dataset_usable &&
      status.accepting_repository_requests && !s_storage_request_pending &&
      !active_migration().pending();
  state.delete_enabled =
      state.card == SettingsStorageCardView::ReadWrite &&
      dataset_usable &&
      status.manifest_selected && status.pending_work == 0U &&
      !s_storage_request_pending && !active_migration().pending();
  state.capacity_known = status.capacity_known;
  state.total_mib =
      static_cast<uint32_t>(status.total_bytes / (1024U * 1024U));
  state.free_mib =
      static_cast<uint32_t>(status.free_bytes / (1024U * 1024U));
  if (status.manifest_selected) {
    std::memcpy(state.dataset_id_prefix,
                status.selected_manifest.dataset_id, 8U);
    state.dataset_id_prefix[8] = '\0';
    state.dataset_generation = status.selected_manifest.commit_generation;
  }
  state.operation_message = s_storage_request_pending
                                ? "PENDING · KEEP CARD INSERTED"
                                : s_flow.operation_message();
  return state;
}

SettingsMigrationPageState migration_page_state() {
  SettingsMigrationPageState state = {};
  const LegacyMigrationStatus status = active_migration().status();
  switch (status) {
  case LegacyMigrationStatus::Pending:
    state.view = SettingsMigrationView::Importing;
    break;
  case LegacyMigrationStatus::Complete:
    state.view = SettingsMigrationView::Verified;
    break;
  case LegacyMigrationStatus::CompleteWithConflicts:
  case LegacyMigrationStatus::ClearNotAllowed:
    state.view = SettingsMigrationView::NeedsReview;
    break;
  case LegacyMigrationStatus::SourceIoError:
  case LegacyMigrationStatus::ClearFailed:
    state.view = SettingsMigrationView::SourceError;
    break;
  case LegacyMigrationStatus::StorageError:
    state.view = SettingsMigrationView::StorageError;
    break;
  case LegacyMigrationStatus::Cleared:
    state.view = SettingsMigrationView::Cleared;
    break;
  case LegacyMigrationStatus::Idle:
    state.view = SettingsMigrationView::NotScanned;
    break;
  }
  const LegacyMigrationReport &report = active_migration().report();
  state.valid = report.records_valid;
  state.imported = report.records_imported;
  state.already_present = report.records_already_present;
  state.conflicts = report.records_conflicted;
  state.corrupt = report.records_corrupt;
  state.kicks_only = s_flow.migrating_kicks();
  state.kicks_preserved = report.legacy_kicks_preserved;
  state.import_enabled = storage_page_state().migration_enabled;
  state.clear_enabled = status == LegacyMigrationStatus::Complete;
  return state;
}

void generate_dataset_id() {
  uint8_t random_bytes[16] = {};
  esp_fill_random(random_bytes, sizeof(random_bytes));
  constexpr char kHex[] = "0123456789abcdef";
  for (size_t index = 0U; index < sizeof(random_bytes); ++index) {
    s_storage_dataset_id[index * 2U] = kHex[random_bytes[index] >> 4U];
    s_storage_dataset_id[index * 2U + 1U] =
        kHex[random_bytes[index] & 0x0FU];
  }
  s_storage_dataset_id[32] = '\0';
}

const char *storage_result_message(SdManifestRequestStatus status) {
  switch (status) {
  case SdManifestRequestStatus::Ok:
    return "DATASET CREATED";
  case SdManifestRequestStatus::StaleGeneration:
    return "CARD CHANGED · TRY AGAIN";
  case SdManifestRequestStatus::CardMissing:
  case SdManifestRequestStatus::CardReadOnly:
  case SdManifestRequestStatus::NoSpace:
  case SdManifestRequestStatus::CardUnavailable:
    return "CARD NOT WRITABLE";
  case SdManifestRequestStatus::AlreadyInitialized:
    return "CARD ALREADY INITIALIZED";
  case SdManifestRequestStatus::ConfirmationRequired:
    return "CONFIRMATION REQUIRED";
  default:
    return "INITIALIZATION FAILED";
  }
}

void load_profile() {
  const auto &store = pregnancy_repository::pregnancy_repository_store();
  s_profile_configured = store.ready() && store.found();
  if (s_profile_configured) s_profile = store.profile();
  StickyRtcDateTime now = {};
  if (s_profile_configured && sticky_rtc_read(now) == ESP_OK) {
    const PregnancyDate today = {now.year, now.month, now.day};
    if (!pregnancy_progress_calculate(today, s_profile, s_progress))
      s_profile_configured = false;
  }
}


bool SettingsPort::read_clock(SettingsClockTime &time) {
  StickyRtcDateTime value = {};
  if (sticky_rtc_read(value) != ESP_OK) return false;
  time = {value.year, value.month, value.day, value.hour, value.minute};
  return true;
}

bool SettingsPort::write_clock(const SettingsClockTime &time) {
  StickyRtcDateTime value = {};
  value.year = time.year;
  value.month = time.month;
  value.day = time.day;
  value.hour = time.hour;
  value.minute = time.minute;
  return sticky_rtc_write(value) == ESP_OK;
}

SettingsStoragePageState SettingsPort::storage_state() {
  return storage_page_state();
}

void SettingsPort::prepare_initialization() { generate_dataset_id(); }
bool SettingsPort::has_dataset_id() const { return s_storage_dataset_id[0] != '\0'; }
void SettingsPort::forget_dataset_id() { s_storage_dataset_id[0] = '\0'; }

const char *SettingsPort::submit_initialization() {
  const SdStorageServiceStatus status = sd_storage_service_status();
  const esp_err_t result = sd_storage_service_submit_manifest_initialize(
      status.mount_generation, s_storage_dataset_id, true, s_storage_request);
  if (result == ESP_OK) {
    s_storage_request_pending = true;
    STICKY_LOGI(kTag, "storage=initialize request=%u result=queued",
                static_cast<unsigned>(s_storage_request.id));
    return nullptr;
  }
  STICKY_LOGW(kTag, "storage=initialize result=%s", esp_err_to_name(result));
  return "INITIALIZATION FAILED";
}

const char *SettingsPort::safe_eject() {
  return sd_storage_service_safe_eject() == ESP_OK ? "SAFE TO REMOVE CARD"
                                                  : "SAFE EJECT FAILED";
}

bool SettingsPort::migration_pending() { return active_migration().pending(); }
bool SettingsPort::migration_complete() {
  return active_migration().status() == LegacyMigrationStatus::Complete;
}
void SettingsPort::begin_import() { active_migration().begin_import(); }
void SettingsPort::clear_old_copies() { active_migration().clear_old_copies(); }

void SettingsPort::prepare_delete() {
  const auto status = sd_storage_service_status();
  s_delete_confirmation = {};
  s_delete_confirmation.mount_generation = status.mount_generation;
  s_delete_confirmation.manifest_selected = status.manifest_selected;
  if (status.manifest_selected)
    std::memcpy(s_delete_confirmation.dataset_id,
                status.selected_manifest.dataset_id,
                sizeof(s_delete_confirmation.dataset_id));
}

const char *SettingsPort::delete_card_data(bool enabled, bool &deleted) {
  const esp_err_t result =
      enabled ? sd_storage_service_delete_application_data(s_delete_confirmation, true)
              : ESP_ERR_INVALID_STATE;
  deleted = result == ESP_OK;
  return deleted ? "PREGJNL DATA DELETED" : "APP DATA DELETE FAILED";
}

const char *SettingsPort::reset_device_binding() {
  const esp_err_t key_result = bound_credentials_reset_device_key();
  const esp_err_t wifi_result = wifi_forget_ram_configuration();
  if (wifi_result != ESP_OK)
    STICKY_LOGI(kTag, "network=ram-forget result=%s",
                esp_err_to_name(wifi_result));
  return key_result == ESP_OK ? "DEVICE BINDING RESET" : "DEVICE RESET FAILED";
}

PageRefreshTracker s_refresh;
press_feedback::Pending s_press;

// Draws the tapped target pressed and shows it once, before a page change
// whose full refresh would otherwise leave the panel unchanged for ~2 s.
void show_press_feedback() {
  press_feedback::Rect rect = {};
  if (!s_press.take(rect)) return;
  press_feedback::draw(*s_canvas, rect);
  sticky_display_refresh_partial();
}

void render(bool full = false) {
  sticky_display_set_battery_overlay_enabled(false);
  if (s_refresh.page_changes(static_cast<int>(s_flow.page())))
    show_press_feedback();
  else
    s_press.clear();
  const SettingsPage page = s_flow.page();
  if (page == SettingsPage::Main) {
    StickyRtcDateTime now = {};
    const bool rtc_ready = sticky_rtc_read(now) == ESP_OK;
    char clock[24] = "UNAVAILABLE";
    if (rtc_ready)
      std::snprintf(
          clock, sizeof(clock), "%04u.%02u.%02u %02u:%02u",
          static_cast<unsigned>(now.year), static_cast<unsigned>(now.month),
          static_cast<unsigned>(now.day), static_cast<unsigned>(now.hour),
          static_cast<unsigned>(now.minute));
    load_profile();
    char due[16] = "NOT SET";
    if (s_profile_configured)
      std::snprintf(due, sizeof(due), "%04u.%02u.%02u",
                    static_cast<unsigned>(s_profile.estimated_due_date.year),
                    static_cast<unsigned>(s_profile.estimated_due_date.month),
                    static_cast<unsigned>(s_profile.estimated_due_date.day));
    settings_page_render_main(*s_canvas, rtc_ready, clock, due,
                              sticky_app_preferences_save_failed());
  } else if (page == SettingsPage::TimeEditor) {
    settings_page_render_time_editor(*s_canvas, s_flow.time_fields(), s_flow.input_error());
  } else if (page == SettingsPage::PregnancySummary) {
    load_profile();
    settings_page_render_pregnancy(*s_canvas, s_profile, s_progress,
                                   s_profile_configured);
  } else if (page == SettingsPage::StorageSummary) {
    s_displayed_storage = storage_page_state();
    settings_page_render_storage(*s_canvas, s_displayed_storage);
  } else if (page == SettingsPage::StorageConfirm) {
    settings_page_render_storage_confirm(*s_canvas);
  } else if (page == SettingsPage::StorageMore) {
    s_displayed_storage = storage_page_state();
    settings_page_render_storage_more(*s_canvas, s_displayed_storage);
  } else if (page == SettingsPage::LegacyMigration) {
    settings_page_render_legacy_migration(*s_canvas, migration_page_state());
  } else if (page == SettingsPage::LegacyClearConfirm) {
    settings_page_render_legacy_clear_confirm(*s_canvas, s_flow.migrating_kicks());
  } else if (page == SettingsPage::CardDataDeleteConfirm) {
    settings_page_render_card_data_delete_confirm(
        *s_canvas, storage_page_state().dataset_id_prefix);
  } else {
    settings_page_render_device_reset_confirm(*s_canvas);
  }
  // A page change is a full-screen replacement: always full, never partial.
  if (s_refresh.needs_full(static_cast<int>(s_flow.page()), full))
    sticky_display_refresh_monochrome();
  else
    sticky_display_refresh_partial();
}

void poll_storage_request() {
  if (!s_storage_request_pending)
    return;
  SdStorageManifestResult result = {};
  const esp_err_t collected = sd_storage_service_collect_manifest_result(
      s_storage_request, result);
  if (collected == ESP_ERR_NOT_FINISHED)
    return;

  s_storage_request_pending = false;
  s_storage_dataset_id[0] = '\0';
  if (collected == ESP_OK) {
    s_flow.set_operation_message(storage_result_message(result.outcome.status));
    STICKY_LOGI(kTag, "storage=initialize request=%u result=%u changed=%u",
                static_cast<unsigned>(result.request_id),
                static_cast<unsigned>(result.outcome.status),
                result.outcome.changed ? 1U : 0U);
  } else {
    s_flow.set_operation_message("INITIALIZATION FAILED");
    STICKY_LOGW(kTag, "storage=initialize collect=%s",
                esp_err_to_name(collected));
  }
  if (s_flow.page() == SettingsPage::StorageSummary ||
      s_flow.page() == SettingsPage::StorageMore)
    render();
}

// Carries out what the flow asked for after an action.
void handle(SettingsAction action) {
  const SettingsFlowResult result = s_flow.handle_action(action);
  if (result.toggle_language)
    sticky_app_request_language_toggle();
  if (result.open_launcher) {
    show_press_feedback();
    sticky_app_request_launcher();
  }
  if (result.request_home) {
    show_press_feedback();
    s_home_requested = true;
  }
  if (result.request_pregnancy)
    s_pregnancy_requested = true;
  if (result.notify_data_deleted)
    sticky_app_notify_application_data_deleted();
  if (result.render)
    render(result.render_full);
}

void task(void *) {
  render(true);
  while (true) {
    if (s_preferences_changed.exchange(false)) render(true);
    if (sticky_app_lifecycle_checkpoint(s_lifecycle))
      render(true);
    if (s_flow.page() == SettingsPage::StorageSummary ||
        s_flow.page() == SettingsPage::StorageMore) {
      auto current = storage_page_state();
      if (current.mount_generation != s_displayed_storage.mount_generation) {
        s_flow.clear_operation_message();
        current = storage_page_state();
      }
      if (!settings_storage_state_equal(current, s_displayed_storage)) {
        render(true);
        // A tap intended for the old page must not invoke the newly assigned
        // action at the same coordinates (initialize/eject share a button).
        sticky_touch_clear_press();
        continue;
      }
    }
    StickyTouchPress press = {};
    if (sticky_touch_take_press(press)) {
      int x = 0, y = 0;
      s_canvas->physical_to_logical(press.x, press.y, x, y);
      const SettingsPage page = s_flow.page();
      bool primary_action_enabled = true;
      bool secondary_action_enabled = false;
      if (page == SettingsPage::StorageSummary) {
        const SettingsStoragePageState state = storage_page_state();
        if (!settings_storage_state_equal(state, s_displayed_storage)) {
          render(true);
          sticky_touch_clear_press();
          continue;
        }
        primary_action_enabled =
            state.initialize_enabled || state.eject_enabled;
        secondary_action_enabled = true;
      } else if (page == SettingsPage::StorageMore) {
        const SettingsStoragePageState state = storage_page_state();
        if (!settings_storage_state_equal(state, s_displayed_storage)) {
          render(true);
          sticky_touch_clear_press();
          continue;
        }
        primary_action_enabled = state.migration_enabled;
        secondary_action_enabled = state.delete_enabled;
      } else if (page == SettingsPage::LegacyMigration) {
        const SettingsMigrationPageState state = migration_page_state();
        primary_action_enabled = state.import_enabled;
        secondary_action_enabled = state.clear_enabled;
      }
      SettingsAction action = settings_page_action_at(
          page, s_canvas->width(), s_canvas->height(), x, y,
          primary_action_enabled, secondary_action_enabled);
      if (page == SettingsPage::StorageSummary &&
          action == SettingsAction::BeginStorageInitialization) {
        if (!s_displayed_storage.initialize_enabled &&
            s_displayed_storage.eject_enabled)
          action = SettingsAction::SafeEjectStorage;
      }
      const int w = s_canvas->width(), h = s_canvas->height();
      SettingsTarget targets[kMaxSettingsTargets];
      const size_t target_count = settings_page_targets(
          page, w, h, primary_action_enabled, secondary_action_enabled,
          targets, kMaxSettingsTargets);
      for (size_t i = 0U; i < target_count; ++i) {
        if (targets[i].rect.contains(x, y)) {
          s_press.set(targets[i].rect);
          break;
        }
      }
      handle(action);
      s_press.clear();
    }
    poll_storage_request();
    if (active_migration().pending()) {
      const LegacyMigrationStatus before = active_migration().status();
      const LegacyMigrationStatus after = active_migration().poll();
      if (before == LegacyMigrationStatus::Pending &&
          after != LegacyMigrationStatus::Pending &&
          s_flow.page() == SettingsPage::LegacyMigration) {
        render(true);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(40));
  }
}

} // namespace

esp_err_t settings_app_start(Canvas &canvas) {
  app_log_register_tag(kTag);
  if (s_task != nullptr)
    return ESP_OK;
  s_canvas = &canvas;
  return xTaskCreate(task, "settings", 4608U, nullptr, 3U, &s_task) == pdPASS
             ? ESP_OK
             : ESP_ERR_NO_MEM;
}

esp_err_t settings_app_pause() {
  return sticky_app_lifecycle_pause(s_lifecycle, s_task);
}
esp_err_t settings_app_resume() {
  sticky_app_lifecycle_resume(s_lifecycle);
  return ESP_OK;
}
esp_err_t settings_app_prepare_power_sleep(uint32_t &current, uint32_t &next) {
  current = 0U;
  next = 0U;
  return settings_app_pause();
}
uint32_t settings_app_power_sleep_timeout_ms() { return kSleepTimeoutMs; }
bool settings_app_power_sleep_allowed() {
  return !s_flow.editing_time() &&
         !s_storage_request_pending && !active_migration().pending();
}
bool settings_app_take_home_request() {
  const bool value = s_home_requested;
  s_home_requested = false;
  return value;
}
bool settings_app_take_pregnancy_request() {
  const bool value = s_pregnancy_requested;
  s_pregnancy_requested = false;
  return value;
}
void settings_app_notify_preferences_changed() {
  s_preferences_changed.store(true);
}
