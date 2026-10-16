#pragma once

#include <cstddef>
#include <cstdint>

#include "digit_fields.h"
#include "pregnancy_state.h"
#include "key_policy.h"
#include "shortcut_target.h"
#include "prototype_ui.h"

class Canvas;

enum class SettingsPage : uint8_t {
  Main,
  TimeEditor,
  PregnancySummary,
  StorageSummary,
  StorageConfirm,
  StorageMore, // import old data, delete card data, reset device binding
  LegacyMigration,
  LegacyClearConfirm,
  CardDataDeleteConfirm,
  DeviceResetConfirm,
  Shortcut, // double-click target
};

// How the hardware keys act on `page`. Editors and confirmation dialogs are Locked.
// 各页对硬件按键的反应；编辑页和确认对话框为 Locked。
inline KeyPolicy settings_page_key_policy(SettingsPage page) {
  switch (page) {
  case SettingsPage::Main:
  case SettingsPage::Shortcut:
  case SettingsPage::PregnancySummary:
  case SettingsPage::StorageSummary:
  case SettingsPage::StorageMore:
  case SettingsPage::LegacyMigration:
    return KeyPolicy::Free;
  case SettingsPage::TimeEditor:
  case SettingsPage::StorageConfirm:
  case SettingsPage::LegacyClearConfirm:
  case SettingsPage::CardDataDeleteConfirm:
  case SettingsPage::DeviceResetConfirm:
    return KeyPolicy::Locked;
  }
  return KeyPolicy::Locked;
}

enum class SettingsAction : uint8_t {
  None,
  ToggleLanguage,
  EditTime,
  RefreshDisplay,
  PregnancySettings,
  StorageSettings,
  ShortcutSettings, // main page: open the shortcut page
  ShortcutWeight,   // shortcut page: pick a double-click target
  ShortcutBabyWeek,
  ShortcutKick,
  ShortcutOff,
  BeginStorageInitialization,
  SafeEjectStorage,
  ConfirmStorageInitialization,
  CancelStorageInitialization,
  ManageStorage, // storage summary -> "more" page
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
  Field0, // time editor: select the year / month / day / hour / minute
  Field1, // field to edit (see settings_action_field)
  Field2,
  Field3,
  Field4,
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
                               bool save_failed = false,
                               StickyShortcutTarget shortcut =
                                   StickyShortcutTarget::WeightRecordToday);
// The double-click target page: one row per target, the current one inverted.
// 双击目标页：每个目标一行，当前项反色。
void settings_page_render_shortcut(Canvas &canvas, StickyShortcutTarget current);
// The action a row of the shortcut page performs, and the target it picks.
SettingsAction settings_shortcut_action(StickyShortcutTarget target);
bool settings_action_shortcut(SettingsAction action, StickyShortcutTarget &target);
void settings_page_render_time_editor(Canvas &canvas,
                                      const DigitFields &fields,
                                      bool input_error);
void settings_page_render_pregnancy(Canvas &canvas,
                                    const PregnancyProfile &profile,
                                    const PregnancyProgress &progress,
                                    bool configured);
void settings_page_render_storage(Canvas &canvas,
                                  const SettingsStoragePageState &state);
void settings_page_render_storage_confirm(Canvas &canvas);
void settings_page_render_storage_more(Canvas &canvas,
                                       const SettingsStoragePageState &state);
void settings_page_render_legacy_migration(
    Canvas &canvas, const SettingsMigrationPageState &state);
void settings_page_render_legacy_clear_confirm(Canvas &canvas, bool kicks_only = false);
void settings_page_render_card_data_delete_confirm(
    Canvas &canvas, const char *dataset_id_prefix);
void settings_page_render_device_reset_confirm(Canvas &canvas);
// Where the header action of `page` goes inside Settings; false = leaves
// Settings for the launcher (the main page).
bool settings_page_back_target(SettingsPage page, SettingsPage &target);

// Upper bound on the targets of any page (time editor: 5 fields + 12 keys + back).
constexpr size_t kMaxSettingsTargets = 24U;

// One tappable rectangle. The renderers draw these same rectangles.
struct SettingsTarget {
  prototype_ui::Rect rect;
  SettingsAction action;
};

// Lists the active hit targets of `page` (at most `capacity` written; returns
// the count written). Disabled primary/secondary buttons are omitted, exactly
// as they are not tappable. The hit test is the first target containing the
// point.
size_t settings_page_targets(SettingsPage page, int width, int height,
                             bool primary_action_enabled,
                             bool secondary_action_enabled,
                             SettingsTarget *out, size_t capacity);

SettingsAction settings_page_action_at(SettingsPage page, int width, int height,
                                       int x, int y,
                                       bool primary_action_enabled = true,
                                       bool secondary_action_enabled = false);
bool settings_action_digit(SettingsAction action, char &digit);
// Maps Field0..Field4 to the field index 0..4.
bool settings_action_field(SettingsAction action, size_t &field);

// Compare rendered values explicitly; padding is not UI state.
bool settings_storage_state_equal(const SettingsStoragePageState &a,
                                  const SettingsStoragePageState &b);
