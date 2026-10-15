#include <cassert>
#include <cstdint>
#include <cstring>
#include <fstream>

#include "canvas.h"
#include "settings_pages.h"
#include "ui_language.h"

namespace {

void write_preview(Canvas &canvas, const char *path) {
  std::ofstream output(path, std::ios::binary);
  output << "P6\n" << canvas.width() << " " << canvas.height() << "\n255\n";
  for (int y = 0; y < canvas.height(); ++y) {
    for (int x = 0; x < canvas.width(); ++x) {
      const uint8_t value =
          canvas.pixel_at(x, y) == GrayLevel::Black ? 0U : 255U;
      output.write(reinterpret_cast<const char *>(&value), 1);
      output.write(reinterpret_cast<const char *>(&value), 1);
      output.write(reinterpret_cast<const char *>(&value), 1);
    }
  }
}

// Geometry contract: every target (for every enabled-flag combination) is on
// screen, at least 48 px each way, does not overlap another, and the hit test
// returns the target under its centre. Disabled buttons are absent.
void test_target_contract() {
  const SettingsPage pages[] = {
      SettingsPage::Main,           SettingsPage::TimeEditor,
      SettingsPage::PregnancySummary, SettingsPage::StorageSummary,
      SettingsPage::StorageConfirm, SettingsPage::LegacySource,
      SettingsPage::LegacyMigration, SettingsPage::LegacyClearConfirm,
      SettingsPage::CardDataDeleteConfirm, SettingsPage::DeviceResetConfirm};
  const int sizes[2][2] = {{800, 480}, {480, 800}};
  for (const auto &size : sizes) {
    for (SettingsPage page : pages) {
      for (int flags = 0; flags < 4; ++flags) {
        const bool primary = (flags & 1) != 0;
        const bool secondary = (flags & 2) != 0;
        SettingsTarget targets[16];
        const size_t count = settings_page_targets(page, size[0], size[1], primary,
                                                   secondary, targets, 16U);
        assert(count >= 1U);
        for (size_t a = 0U; a < count; ++a) {
          const auto &r = targets[a].rect;
          assert(r.x >= 0 && r.y >= 0 && r.x + r.width <= size[0] &&
                 r.y + r.height <= size[1]);
          assert(r.height >= 48 && r.width >= 48);
          assert(settings_page_action_at(page, size[0], size[1], r.x + r.width / 2,
                                         r.y + r.height / 2, primary,
                                         secondary) == targets[a].action);
          for (size_t b = a + 1U; b < count; ++b) {
            const auto &o = targets[b].rect;
            assert(!(r.x < o.x + o.width && o.x < r.x + r.width &&
                     r.y < o.y + o.height && o.y < r.y + r.height));
          }
        }
      }
    }
  }
}

} // namespace

int main() {
  test_target_contract();
  uint8_t framebuffer[800U * 480U / 2U] = {};
  Canvas canvas(800U, 480U, framebuffer, sizeof(framebuffer));
  ui_language_set(UiLanguage::English);
  settings_page_render_main(canvas, true, "2026.09.13 14:42", "2026.12.31");
  write_preview(canvas, "/tmp/settings_main_landscape.ppm");
  assert(settings_page_action_at(SettingsPage::Main, 800, 480, 100, 120) ==
         SettingsAction::ToggleLanguage);
  assert(settings_page_action_at(SettingsPage::Main, 800, 480, 500, 180) ==
         SettingsAction::EditTime);
  assert(settings_page_action_at(SettingsPage::Main, 800, 480, 100, 240) ==
         SettingsAction::PregnancySettings);
  assert(settings_page_action_at(SettingsPage::Main, 800, 480, 500, 300) ==
         SettingsAction::RefreshDisplay);
  assert(settings_page_action_at(SettingsPage::Main, 800, 480, 400, 370) ==
         SettingsAction::StorageSettings);
  assert(settings_page_action_at(SettingsPage::Main, 800, 480, 730, 40) ==
         SettingsAction::Back);

  SettingsStoragePageState storage_state = {};
  storage_state.card = SettingsStorageCardView::ReadWrite;
  storage_state.dataset = SettingsStorageDatasetView::NotInitialized;
  storage_state.requests_open = true;
  storage_state.initialize_enabled = true;
  storage_state.migration_enabled = true;
  storage_state.delete_enabled = true;
  storage_state.capacity_known = true;
  storage_state.total_mib = 30708U;
  storage_state.free_mib = 2936U;
  std::strcpy(storage_state.dataset_id_prefix, "00b454dc");
  storage_state.dataset_generation = 2U;
  settings_page_render_storage(canvas, storage_state);
  write_preview(canvas, "/tmp/settings_storage_landscape.ppm");
  assert(settings_page_action_at(SettingsPage::StorageSummary, 800, 480, 200,
                                 430) ==
         SettingsAction::BeginStorageInitialization);
  assert(settings_page_action_at(SettingsPage::StorageSummary, 800, 480, 200,
                                 430, false) == SettingsAction::None);
  assert(settings_page_action_at(SettingsPage::StorageSummary, 800, 480, 600,
                                 430, true, true) ==
         SettingsAction::ManageLegacyData);
  assert(settings_page_action_at(SettingsPage::StorageSummary, 800, 480, 200,
                                 250, true, true) ==
         SettingsAction::BeginCardDataDelete);
  assert(settings_page_action_at(SettingsPage::StorageSummary, 800, 480, 200,
                                 320, true, true) ==
         SettingsAction::BeginDeviceReset);
  settings_page_render_card_data_delete_confirm(canvas, "00b454dc");
  write_preview(canvas, "/tmp/settings_card_delete_landscape.ppm");
  assert(settings_page_action_at(SettingsPage::CardDataDeleteConfirm,
                                 800, 480, 600, 430) ==
         SettingsAction::ConfirmCardDataDelete);
  settings_page_render_device_reset_confirm(canvas);
  write_preview(canvas, "/tmp/settings_device_reset_landscape.ppm");
  assert(settings_page_action_at(SettingsPage::DeviceResetConfirm,
                                 800, 480, 600, 430) ==
         SettingsAction::ConfirmDeviceReset);
  const SettingsMigrationPageState migration_state = {
      SettingsMigrationView::Verified, 4U, 3U, 1U, 0U, 0U, true, true, true};
  settings_page_render_legacy_migration(canvas, migration_state);
  write_preview(canvas, "/tmp/settings_migration_landscape.ppm");
  assert(settings_page_action_at(SettingsPage::LegacyMigration, 800, 480,
                                 200, 430, true, true) ==
         SettingsAction::BeginLegacyImport);
  assert(settings_page_action_at(SettingsPage::LegacyMigration, 800, 480,
                                 600, 430, true, true) ==
         SettingsAction::BeginLegacyClear);
  settings_page_render_legacy_clear_confirm(canvas);
  assert(settings_page_action_at(SettingsPage::LegacyClearConfirm, 800, 480,
                                 600, 430) ==
         SettingsAction::ConfirmLegacyClear);
  settings_page_render_storage_confirm(canvas);
  write_preview(canvas, "/tmp/settings_storage_confirm_landscape.ppm");
  assert(settings_page_action_at(SettingsPage::StorageConfirm, 800, 480, 100,
                                 430) ==
         SettingsAction::CancelStorageInitialization);
  assert(settings_page_action_at(SettingsPage::StorageConfirm, 800, 480, 600,
                                 430) ==
         SettingsAction::ConfirmStorageInitialization);
  settings_page_render_time_editor(canvas, "202609111530", false);
  assert(settings_page_action_at(SettingsPage::TimeEditor, 800, 480, 520,
                                 120) == SettingsAction::Digit1);
  assert(settings_page_action_at(SettingsPage::TimeEditor, 800, 480, 720,
                                 340) == SettingsAction::Save);

  canvas.set_rotation(CanvasRotation::Deg90Clockwise);
  settings_page_render_main(canvas, true, "2026.09.13 14:42", "2026.12.31");
  write_preview(canvas, "/tmp/settings_main_portrait.ppm");
  assert(settings_page_action_at(SettingsPage::Main, 480, 800, 100, 150) ==
         SettingsAction::ToggleLanguage);
  assert(settings_page_action_at(SettingsPage::Main, 480, 800, 100, 250) ==
         SettingsAction::EditTime);
  assert(settings_page_action_at(SettingsPage::Main, 480, 800, 100, 350) ==
         SettingsAction::PregnancySettings);
  assert(settings_page_action_at(SettingsPage::Main, 480, 800, 100, 460) ==
         SettingsAction::RefreshDisplay);
  assert(settings_page_action_at(SettingsPage::Main, 480, 800, 100, 570) ==
         SettingsAction::StorageSettings);
  assert(settings_page_action_at(SettingsPage::Main, 480, 800, 420, 40) ==
         SettingsAction::Back);
  settings_page_render_storage(canvas, storage_state);
  write_preview(canvas, "/tmp/settings_storage_portrait.ppm");
  assert(settings_page_action_at(SettingsPage::StorageSummary, 480, 800, 100,
                                 748, true, true) ==
         SettingsAction::BeginStorageInitialization);
  assert(settings_page_action_at(SettingsPage::StorageSummary, 480, 800, 360,
                                 748, true, true) ==
         SettingsAction::ManageLegacyData);
  assert(settings_page_action_at(SettingsPage::StorageSummary, 480, 800, 100,
                                 500, true, true) ==
         SettingsAction::BeginDeviceReset);
  assert(settings_page_action_at(SettingsPage::StorageSummary, 480, 800, 100,
                                 360, true, true) ==
         SettingsAction::BeginCardDataDelete);
  settings_page_render_legacy_migration(canvas, migration_state);
  write_preview(canvas, "/tmp/settings_migration_portrait.ppm");
  settings_page_render_storage_confirm(canvas);
  write_preview(canvas, "/tmp/settings_storage_confirm_portrait.ppm");
  settings_page_render_device_reset_confirm(canvas);
  write_preview(canvas, "/tmp/settings_device_reset_portrait.ppm");
  assert(settings_page_action_at(SettingsPage::StorageConfirm, 480, 800, 100,
                                 748) ==
         SettingsAction::CancelStorageInitialization);
  assert(settings_page_action_at(SettingsPage::StorageConfirm, 480, 800, 360,
                                 748) ==
         SettingsAction::ConfirmStorageInitialization);

  ui_language_set(UiLanguage::ChineseSimplified);
  settings_page_render_storage(canvas, storage_state);
  write_preview(canvas, "/tmp/settings_storage_portrait_zh.ppm");
  settings_page_render_storage_confirm(canvas);
  write_preview(canvas, "/tmp/settings_storage_confirm_portrait_zh.ppm");
  assert(std::strcmp(ui_text("MICROSD STORAGE"), "MICROSD 数据") == 0);
  ui_language_set(UiLanguage::English);
  settings_page_render_time_editor(canvas, "202609111530", false);
  write_preview(canvas, "/tmp/settings_time_portrait.ppm");
  assert(settings_page_action_at(SettingsPage::TimeEditor, 480, 800, 80, 350) ==
         SettingsAction::Digit1);
  assert(settings_page_action_at(SettingsPage::TimeEditor, 480, 800, 400,
                                 580) == SettingsAction::Save);
  char digit = 0;
  assert(settings_action_digit(SettingsAction::Digit7, digit));
  assert(digit == '7');
  SettingsStoragePageState absent = {}, inserted = {};
  assert(settings_storage_state_equal(absent, inserted));
  inserted.mount_generation = 2;
  inserted.card = SettingsStorageCardView::ReadWrite;
  inserted.eject_enabled = true;
  assert(!settings_storage_state_equal(absent, inserted));
  auto shown = inserted;
  assert(settings_storage_state_equal(shown, inserted));
  inserted.mount_generation++;
  assert(!settings_storage_state_equal(shown, inserted));
  inserted = shown; inserted.operation_message = "CARD SAFELY EJECTED";
  assert(!settings_storage_state_equal(shown, inserted));
  inserted = shown; inserted.eject_enabled = false;
  assert(!settings_storage_state_equal(shown, inserted));
  inserted = shown; inserted.free_mib++;
  assert(!settings_storage_state_equal(shown, inserted));
  return 0;
}
