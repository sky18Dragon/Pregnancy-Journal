#include <cassert>
#include <cstring>

#include "settings_flow.h"

namespace {

// A fake of every service Settings talks to; it records what was called.
class FakePort final : public SettingsFlowPort {
public:
  bool clock_ok = true;
  bool accept_write = true;
  SettingsClockTime now = {2026U, 10U, 10U, 14U, 42U};
  SettingsClockTime written = {};
  int writes = 0;
  SettingsStoragePageState storage = {};
  bool dataset_id = false;
  bool submit_ok = true;
  bool pending = false;
  bool complete = false;
  bool eject_ok = true;
  bool delete_ok = true;
  bool reset_ok = true;
  int prepared = 0, forgotten = 0, imports = 0, clears = 0, deletes = 0,
      delete_preps = 0, resets = 0, ejects = 0, submits = 0;

  bool read_clock(SettingsClockTime &time) override {
    if (!clock_ok) return false;
    time = now;
    return true;
  }
  bool write_clock(const SettingsClockTime &time) override {
    if (!accept_write) return false;
    written = time;
    ++writes;
    return true;
  }
  SettingsStoragePageState storage_state() override { return storage; }
  void prepare_initialization() override { dataset_id = true; ++prepared; }
  bool has_dataset_id() const override { return dataset_id; }
  void forget_dataset_id() override { dataset_id = false; ++forgotten; }
  const char *submit_initialization() override {
    ++submits;
    return submit_ok ? nullptr : "INITIALIZATION FAILED";
  }
  const char *safe_eject() override {
    ++ejects;
    return eject_ok ? "SAFE TO REMOVE CARD" : "SAFE EJECT FAILED";
  }
  bool migration_pending() override { return pending; }
  bool migration_complete() override { return complete; }
  void begin_import() override { ++imports; }
  void clear_old_copies() override { ++clears; }
  void prepare_delete() override { ++delete_preps; }
  const char *delete_card_data(bool enabled, bool &deleted) override {
    ++deletes;
    deleted = enabled && delete_ok;
    return deleted ? "PREGJNL DATA DELETED" : "APP DATA DELETE FAILED";
  }
  const char *reset_device_binding() override {
    ++resets;
    return reset_ok ? "DEVICE BINDING RESET" : "DEVICE RESET FAILED";
  }
};

void type(SettingsFlow &flow, const char *digits) {
  for (const char *p = digits; *p; ++p)
    assert(!flow.handle_action(static_cast<SettingsAction>(
                                   static_cast<int>(SettingsAction::Digit0) + (*p - '0')))
                .toggle_language);
}

bool same(const char *a, const char *b) { return a && std::strcmp(a, b) == 0; }

} // namespace

int main() {
  FakePort port;
  SettingsFlow flow(port);
  assert(flow.page() == SettingsPage::Main);

  // Plain navigation, redraws only; the language toggle is a request.
  SettingsFlowResult r = flow.handle_action(SettingsAction::ToggleLanguage);
  assert(r.toggle_language && !r.render);
  r = flow.handle_action(SettingsAction::RefreshDisplay);
  assert(r.render && r.render_full && flow.page() == SettingsPage::Main);
  r = flow.handle_action(SettingsAction::PregnancySettings);
  assert(r.render && flow.page() == SettingsPage::PregnancySummary);
  r = flow.handle_action(SettingsAction::EditPregnancy);
  assert(r.request_pregnancy && !r.render);
  r = flow.handle_action(SettingsAction::Home);
  assert(r.request_home && !r.render);
  flow.handle_action(SettingsAction::Back);
  assert(flow.page() == SettingsPage::Main);
  // Back from the main page leaves for the launcher.
  r = flow.handle_action(SettingsAction::Back);
  assert(r.open_launcher && !r.render && flow.page() == SettingsPage::Main);

  // Time editor: prefilled from the RTC, one field edited, then saved.
  r = flow.handle_action(SettingsAction::EditTime);
  assert(r.render && flow.page() == SettingsPage::TimeEditor && flow.editing_time());
  assert(std::strcmp(flow.time_fields().digits(), "202610101442") == 0);
  flow.handle_action(SettingsAction::Field4);
  type(flow, "07");
  assert(std::strcmp(flow.time_fields().digits(), "202610101407") == 0);
  // An impossible digit is refused with the error flag; DEL clears the error.
  flow.handle_action(SettingsAction::Field1);
  r = flow.handle_action(SettingsAction::Digit3);
  assert(r.render && flow.input_error());
  flow.handle_action(SettingsAction::Delete);
  assert(!flow.input_error());
  flow.handle_action(SettingsAction::Field1);
  type(flow, "10");
  // An incomplete value cannot be saved.
  flow.handle_action(SettingsAction::Field0);
  type(flow, "20");
  assert(!flow.time_fields().complete());
  r = flow.handle_action(SettingsAction::Save);
  assert(r.render && r.render_full && flow.input_error() && port.writes == 0 &&
         flow.page() == SettingsPage::TimeEditor);
  type(flow, "26");
  // A rejected RTC write keeps the editor open with the error.
  port.accept_write = false;
  flow.handle_action(SettingsAction::Save);
  assert(flow.input_error() && flow.page() == SettingsPage::TimeEditor);
  port.accept_write = true;
  r = flow.handle_action(SettingsAction::Save);
  assert(r.render_full && port.writes == 1 && flow.page() == SettingsPage::Main &&
         !flow.input_error());
  assert(port.written.year == 2026U && port.written.hour == 14U &&
         port.written.minute == 7U);
  // Without an RTC the editor starts empty.
  port.clock_ok = false;
  flow.handle_action(SettingsAction::EditTime);
  assert(std::strcmp(flow.time_fields().digits(), "____________") == 0);
  flow.handle_action(SettingsAction::Back);
  port.clock_ok = true;

  // Storage initialization needs the dataset id and a still-valid card.
  flow.handle_action(SettingsAction::StorageSettings);
  assert(flow.page() == SettingsPage::StorageSummary && flow.operation_message() == nullptr);
  r = flow.handle_action(SettingsAction::BeginStorageInitialization);
  assert(!r.render && flow.page() == SettingsPage::StorageSummary && port.prepared == 0);
  port.storage.initialize_enabled = true;
  r = flow.handle_action(SettingsAction::BeginStorageInitialization);
  assert(r.render && flow.page() == SettingsPage::StorageConfirm && port.prepared == 1);
  flow.handle_action(SettingsAction::Back); // leaving the dialog forgets the id
  assert(flow.page() == SettingsPage::StorageSummary && !port.dataset_id);
  flow.handle_action(SettingsAction::BeginStorageInitialization);
  flow.handle_action(SettingsAction::CancelStorageInitialization);
  assert(flow.page() == SettingsPage::StorageSummary && !port.dataset_id);
  flow.handle_action(SettingsAction::BeginStorageInitialization);
  port.storage.initialize_enabled = false; // the card changed meanwhile
  r = flow.handle_action(SettingsAction::ConfirmStorageInitialization);
  assert(r.render && same(flow.operation_message(), "CARD CHANGED · TRY AGAIN") &&
         port.submits == 0 && flow.page() == SettingsPage::StorageSummary);
  port.storage.initialize_enabled = true;
  flow.handle_action(SettingsAction::BeginStorageInitialization);
  port.submit_ok = false;
  flow.handle_action(SettingsAction::ConfirmStorageInitialization);
  assert(same(flow.operation_message(), "INITIALIZATION FAILED") && port.submits == 1);
  flow.handle_action(SettingsAction::BeginStorageInitialization);
  port.submit_ok = true;
  flow.handle_action(SettingsAction::ConfirmStorageInitialization);
  assert(flow.operation_message() == nullptr && port.submits == 2);
  flow.set_operation_message("DATASET CREATED");
  flow.clear_operation_message();
  assert(flow.operation_message() == nullptr);
  r = flow.handle_action(SettingsAction::SafeEjectStorage);
  assert(r.render_full && same(flow.operation_message(), "SAFE TO REMOVE CARD"));
  port.eject_ok = false;
  flow.handle_action(SettingsAction::SafeEjectStorage);
  assert(same(flow.operation_message(), "SAFE EJECT FAILED"));

  // More options: import, delete and reset, each behind its own guard.
  flow.handle_action(SettingsAction::ManageStorage);
  assert(flow.page() == SettingsPage::StorageMore);
  port.pending = true; // an import is running: no switching sources
  r = flow.handle_action(SettingsAction::ManageLegacyKicks);
  assert(!r.render && flow.page() == SettingsPage::StorageMore);
  port.pending = false;
  r = flow.handle_action(SettingsAction::ManageLegacyKicks);
  assert(r.render && flow.page() == SettingsPage::LegacyMigration && flow.migrating_kicks());
  r = flow.handle_action(SettingsAction::BeginLegacyImport);
  assert(!r.render && port.imports == 0); // migration not enabled
  port.storage.migration_enabled = true;
  r = flow.handle_action(SettingsAction::BeginLegacyImport);
  assert(r.render && port.imports == 1);
  r = flow.handle_action(SettingsAction::BeginLegacyClear);
  assert(!r.render && flow.page() == SettingsPage::LegacyMigration); // not complete
  port.complete = true;
  flow.handle_action(SettingsAction::BeginLegacyClear);
  assert(flow.page() == SettingsPage::LegacyClearConfirm);
  flow.handle_action(SettingsAction::CancelLegacyClear);
  assert(flow.page() == SettingsPage::LegacyMigration && port.clears == 0);
  flow.handle_action(SettingsAction::BeginLegacyClear);
  r = flow.handle_action(SettingsAction::ConfirmLegacyClear);
  assert(r.render_full && port.clears == 1 && flow.page() == SettingsPage::LegacyMigration);
  flow.handle_action(SettingsAction::Back);
  assert(flow.page() == SettingsPage::StorageMore);
  flow.handle_action(SettingsAction::ManageLegacyCommon);
  assert(!flow.migrating_kicks());
  flow.handle_action(SettingsAction::Back);

  r = flow.handle_action(SettingsAction::BeginCardDataDelete);
  assert(!r.render && port.delete_preps == 0 && flow.page() == SettingsPage::StorageMore);
  port.storage.delete_enabled = true;
  flow.handle_action(SettingsAction::BeginCardDataDelete);
  assert(flow.page() == SettingsPage::CardDataDeleteConfirm && port.delete_preps == 1);
  flow.handle_action(SettingsAction::CancelCardDataDelete);
  assert(flow.page() == SettingsPage::StorageMore && port.deletes == 0);
  flow.handle_action(SettingsAction::BeginCardDataDelete);
  r = flow.handle_action(SettingsAction::ConfirmCardDataDelete);
  assert(r.render_full && r.notify_data_deleted && port.deletes == 1 &&
         same(flow.operation_message(), "PREGJNL DATA DELETED") &&
         flow.page() == SettingsPage::StorageSummary);
  flow.handle_action(SettingsAction::ManageStorage);
  flow.handle_action(SettingsAction::BeginCardDataDelete);
  port.delete_ok = false;
  r = flow.handle_action(SettingsAction::ConfirmCardDataDelete);
  assert(!r.notify_data_deleted && same(flow.operation_message(), "APP DATA DELETE FAILED"));
  // Deleting while it became unavailable is refused by the port, no notify.
  flow.handle_action(SettingsAction::ManageStorage);
  flow.handle_action(SettingsAction::BeginCardDataDelete);
  port.delete_ok = true;
  port.storage.delete_enabled = false;
  r = flow.handle_action(SettingsAction::ConfirmCardDataDelete);
  assert(!r.notify_data_deleted);

  flow.handle_action(SettingsAction::ManageStorage);
  flow.handle_action(SettingsAction::BeginDeviceReset);
  assert(flow.page() == SettingsPage::DeviceResetConfirm);
  flow.handle_action(SettingsAction::CancelDeviceReset);
  assert(flow.page() == SettingsPage::StorageMore && port.resets == 0);
  flow.handle_action(SettingsAction::BeginDeviceReset);
  r = flow.handle_action(SettingsAction::ConfirmDeviceReset);
  assert(r.render_full && port.resets == 1 && same(flow.operation_message(), "DEVICE BINDING RESET") &&
         flow.page() == SettingsPage::StorageSummary);
  flow.handle_action(SettingsAction::ManageStorage);
  flow.handle_action(SettingsAction::BeginDeviceReset);
  port.reset_ok = false;
  flow.handle_action(SettingsAction::ConfirmDeviceReset);
  assert(same(flow.operation_message(), "DEVICE RESET FAILED"));

  // The shortcut page: open it from Main, pick a target (saved, same page,
  // redraw), Back returns to Main. Picking never leaves the page.
  flow.handle_action(SettingsAction::Back);
  assert(flow.page() == SettingsPage::Main);
  r = flow.handle_action(SettingsAction::ShortcutSettings);
  assert(r.render && !r.select_shortcut && flow.page() == SettingsPage::Shortcut);
  const struct { SettingsAction action; StickyShortcutTarget target; } picks[] = {
      {SettingsAction::ShortcutKick, StickyShortcutTarget::KickStart},
      {SettingsAction::ShortcutOff, StickyShortcutTarget::Off},
      {SettingsAction::ShortcutBabyWeek, StickyShortcutTarget::BabyWeek},
      {SettingsAction::ShortcutWeight, StickyShortcutTarget::WeightRecordToday}};
  for (const auto &pick : picks) {
    r = flow.handle_action(pick.action);
    assert(r.render && r.select_shortcut && r.shortcut == pick.target &&
           !r.render_full && flow.page() == SettingsPage::Shortcut);
  }
  r = flow.handle_action(SettingsAction::Back);
  assert(r.render && !r.open_launcher && !r.select_shortcut &&
         flow.page() == SettingsPage::Main);

  // Entering the storage pages again clears the old message.
  flow.handle_action(SettingsAction::Back);
  assert(flow.page() == SettingsPage::Main);
  flow.handle_action(SettingsAction::StorageSettings);
  assert(flow.operation_message() == nullptr);
  return 0;
}
