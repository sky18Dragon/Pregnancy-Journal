#include "settings_flow.h"

#include <cstdio>

namespace {

unsigned parse(const DigitFields &fields, size_t offset, size_t length) {
  unsigned value = 0U;
  for (size_t index = 0U; index < length; ++index)
    value = value * 10U +
            static_cast<unsigned>(fields.digits()[offset + index] - '0');
  return value;
}

SettingsFlowResult rendered(bool full = false) {
  SettingsFlowResult result;
  result.render = true;
  result.render_full = full;
  return result;
}

} // namespace

void SettingsFlow::open_time_editor() {
  SettingsClockTime time;
  if (port_.read_clock(time)) {
    char digits[13] = {};
    std::snprintf(digits, sizeof(digits), "%04u%02u%02u%02u%02u",
                  static_cast<unsigned>(time.year % 10000U),
                  static_cast<unsigned>(time.month % 100U),
                  static_cast<unsigned>(time.day % 100U),
                  static_cast<unsigned>(time.hour % 100U),
                  static_cast<unsigned>(time.minute % 100U));
    time_fields_.set_digits(digits);
  } else {
    time_fields_.begin(DigitFieldKind::DateTime);
  }
  input_error_ = false;
  page_ = SettingsPage::TimeEditor;
}

void SettingsFlow::save_time() {
  if (!time_fields_.complete()) {
    input_error_ = true;
    return;
  }
  SettingsClockTime time;
  time.year = static_cast<uint16_t>(parse(time_fields_, 0U, 4U));
  time.month = static_cast<uint8_t>(parse(time_fields_, 4U, 2U));
  time.day = static_cast<uint8_t>(parse(time_fields_, 6U, 2U));
  time.hour = static_cast<uint8_t>(parse(time_fields_, 8U, 2U));
  time.minute = static_cast<uint8_t>(parse(time_fields_, 10U, 2U));
  if (!port_.write_clock(time)) {
    input_error_ = true;
    return;
  }
  page_ = SettingsPage::Main;
  input_error_ = false;
}

SettingsFlowResult SettingsFlow::handle_action(SettingsAction action) {
  char digit = 0;
  size_t field = 0U;
  if (settings_action_digit(action, digit)) {
    // An impossible digit (month 13, hour 25...) is refused on the spot.
    input_error_ = !time_fields_.input_digit(digit);
    return rendered();
  }
  if (settings_action_field(action, field)) {
    input_error_ = false;
    time_fields_.select_field(field);
    return rendered();
  }
  StickyShortcutTarget picked = StickyShortcutTarget::WeightRecordToday;
  if (settings_action_shortcut(action, picked)) {
    // Picking a target saves it and stays on the page; the new choice is
    // drawn inverted, which is the confirmation.
    SettingsFlowResult picked_result = rendered();
    picked_result.select_shortcut = true;
    picked_result.shortcut = picked;
    return picked_result;
  }
  SettingsFlowResult result;
  switch (action) {
  case SettingsAction::ShortcutSettings:
    page_ = SettingsPage::Shortcut;
    return rendered();
  case SettingsAction::ToggleLanguage:
    result.toggle_language = true;
    break;
  case SettingsAction::EditTime:
    open_time_editor();
    return rendered();
  case SettingsAction::RefreshDisplay:
    return rendered(true);
  case SettingsAction::PregnancySettings:
    page_ = SettingsPage::PregnancySummary;
    return rendered();
  case SettingsAction::StorageSettings:
    operation_message_ = nullptr;
    page_ = SettingsPage::StorageSummary;
    return rendered();
  case SettingsAction::BeginStorageInitialization:
    if (!port_.storage_state().initialize_enabled)
      break;
    port_.prepare_initialization();
    operation_message_ = nullptr;
    page_ = SettingsPage::StorageConfirm;
    return rendered();
  case SettingsAction::SafeEjectStorage:
    operation_message_ = port_.safe_eject();
    return rendered(true);
  case SettingsAction::ConfirmStorageInitialization: {
    const SettingsStoragePageState state = port_.storage_state();
    page_ = SettingsPage::StorageSummary;
    if (!state.initialize_enabled || !port_.has_dataset_id()) {
      operation_message_ = "CARD CHANGED · TRY AGAIN";
      return rendered();
    }
    operation_message_ = port_.submit_initialization();
    return rendered();
  }
  case SettingsAction::CancelStorageInitialization:
    port_.forget_dataset_id();
    page_ = SettingsPage::StorageSummary;
    return rendered();
  case SettingsAction::ManageStorage:
    page_ = SettingsPage::StorageMore;
    return rendered();
  case SettingsAction::ManageLegacyCommon:
  case SettingsAction::ManageLegacyKicks:
    if (!port_.migration_pending()) {
      migrating_kicks_ = action == SettingsAction::ManageLegacyKicks;
      page_ = SettingsPage::LegacyMigration;
      return rendered();
    }
    break;
  case SettingsAction::BeginLegacyImport:
    if (port_.storage_state().migration_enabled) {
      port_.begin_import();
      return rendered();
    }
    break;
  case SettingsAction::BeginLegacyClear:
    if (port_.migration_complete()) {
      page_ = SettingsPage::LegacyClearConfirm;
      return rendered();
    }
    break;
  case SettingsAction::CancelLegacyClear:
    page_ = SettingsPage::LegacyMigration;
    return rendered();
  case SettingsAction::ConfirmLegacyClear:
    port_.clear_old_copies();
    page_ = SettingsPage::LegacyMigration;
    return rendered(true);
  case SettingsAction::BeginCardDataDelete:
    if (port_.storage_state().delete_enabled) {
      port_.prepare_delete();
      page_ = SettingsPage::CardDataDeleteConfirm;
      return rendered();
    }
    break;
  case SettingsAction::CancelCardDataDelete:
    page_ = SettingsPage::StorageMore;
    return rendered();
  case SettingsAction::ConfirmCardDataDelete: {
    const bool enabled = port_.storage_state().delete_enabled;
    bool deleted = false;
    operation_message_ = port_.delete_card_data(enabled, deleted);
    page_ = SettingsPage::StorageSummary;
    result = rendered(true);
    result.notify_data_deleted = deleted;
    return result;
  }
  case SettingsAction::BeginDeviceReset:
    page_ = SettingsPage::DeviceResetConfirm;
    return rendered();
  case SettingsAction::CancelDeviceReset:
    page_ = SettingsPage::StorageMore;
    return rendered();
  case SettingsAction::ConfirmDeviceReset:
    operation_message_ = port_.reset_device_binding();
    page_ = SettingsPage::StorageSummary;
    return rendered(true);
  case SettingsAction::EditPregnancy:
    result.request_pregnancy = true;
    break;
  case SettingsAction::Home:
    result.request_home = true;
    break;
  case SettingsAction::Back: {
    SettingsPage target = SettingsPage::Main;
    if (!settings_page_back_target(page_, target)) {
      // The main page's header action leaves Settings for the launcher.
      result.open_launcher = true;
      break;
    }
    if (page_ == SettingsPage::StorageConfirm)
      port_.forget_dataset_id();
    page_ = target;
    return rendered();
  }
  case SettingsAction::Delete:
    time_fields_.erase();
    input_error_ = false;
    return rendered();
  case SettingsAction::Save:
    save_time();
    return rendered(true);
  default:
    break;
  }
  return result;
}
