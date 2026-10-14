#pragma once

#include <cstdint>

#include "pregnancy_state.h"

class Canvas;

enum class SettingsPage : uint8_t {
  Main,
  TimeEditor,
  PregnancySummary,
  StorageSummary,
  StorageConfirm,
  LegacySource,
  LegacyMigration,
  LegacyClearConfirm,
  CardDataDeleteConfirm,
  DeviceResetConfirm,
};

enum class SettingsAction : uint8_t {
  None,
  ToggleLanguage,
  EditTime,
  RefreshDisplay,
  PregnancySettings,
  StorageSettings,
  BeginStorageInitialization,
  SafeEjectStorage,
  ConfirmStorageInitialization,
  CancelStorageInitialization,
  ManageLegacyData,
  ManageLegacyCommon,
  ManageLegacyKicks,
  BeginLegacyImport,
  BeginLegacyClear,
  ConfirmLegacyClear,
  CancelLegacyClear,
  BeginCardDataDelete,
  ConfirmCardDataDelete,
  CancelCardDataDelete,
  BeginDeviceReset,
  ConfirmDeviceReset,
  CancelDeviceReset,
  EditPregnancy,
  Home,
  Back,
  Delete,
  Save,
  Digit0,
  Digit1,
  Digit2,
  Digit3,
  Digit4,
  Digit5,
  Digit6,
  Digit7,
  Digit8,
  Digit9,
};

enum class SettingsStorageCardView : uint8_t {
  Missing = 0,
  ReadWrite,
  ReadOnly,
  Full,
  Unavailable,
};

enum class SettingsStorageDatasetView : uint8_t {
  Unknown = 0,
  NotInitialized,
  Ready,
  Degraded,
  Corrupt,
  ReadError,
};

struct SettingsStoragePageState {
  uint32_t mount_generation = 0U;
  SettingsStorageCardView card = SettingsStorageCardView::Missing;
  SettingsStorageDatasetView dataset = SettingsStorageDatasetView::Unknown;
  bool requests_open = false;
  bool initialize_enabled = false;
  bool eject_enabled = false;
  bool migration_enabled = false;
  bool delete_enabled = false;
  bool capacity_known = false;
  uint32_t total_mib = 0U;
  uint32_t free_mib = 0U;
  char dataset_id_prefix[9] = {};
  uint64_t dataset_generation = 0U;
  const char *operation_message = nullptr;
};

enum class SettingsMigrationView : uint8_t {
  NotScanned = 0,
  Importing,
  Verified,
  NeedsReview,
  SourceError,
  StorageError,
  Cleared,
};

struct SettingsMigrationPageState {
  SettingsMigrationView view = SettingsMigrationView::NotScanned;
  uint32_t valid = 0U;
  uint32_t imported = 0U;
  uint32_t already_present = 0U;
  uint32_t conflicts = 0U;
  uint32_t corrupt = 0U;
  bool kicks_only = false;
  bool kicks_preserved = false;
  bool import_enabled = false;
  bool clear_enabled = false;
};

void settings_page_render_main(Canvas &canvas, bool rtc_ready,
                               const char *clock_text, const char *due_text,
                               bool save_failed = false);
void settings_page_render_time_editor(Canvas &canvas, const char *digits,
                                      bool input_error);
void settings_page_render_pregnancy(Canvas &canvas,
                                    const PregnancyProfile &profile,
                                    const PregnancyProgress &progress,
                                    bool configured);
void settings_page_render_storage(Canvas &canvas,
                                  const SettingsStoragePageState &state);
void settings_page_render_storage_confirm(Canvas &canvas);
void settings_page_render_legacy_source(Canvas &canvas);
void settings_page_render_legacy_migration(
    Canvas &canvas, const SettingsMigrationPageState &state);
void settings_page_render_legacy_clear_confirm(Canvas &canvas, bool kicks_only = false);
void settings_page_render_card_data_delete_confirm(
    Canvas &canvas, const char *dataset_id_prefix);
void settings_page_render_device_reset_confirm(Canvas &canvas);
SettingsAction settings_page_action_at(SettingsPage page, int width, int height,
                                       int x, int y,
                                       bool primary_action_enabled = true,
                                       bool secondary_action_enabled = false);
bool settings_action_digit(SettingsAction action, char &digit);

// Compare rendered values explicitly; padding is not UI state.
bool settings_storage_state_equal(const SettingsStoragePageState &a,
                                  const SettingsStoragePageState &b);
