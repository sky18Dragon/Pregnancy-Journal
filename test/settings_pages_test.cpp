#include <cassert>
#include <cstdint>
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

} // namespace

int main() {
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
         SettingsAction::Home);
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
         SettingsAction::Home);
  settings_page_render_time_editor(canvas, "202609111530", false);
  write_preview(canvas, "/tmp/settings_time_portrait.ppm");
  assert(settings_page_action_at(SettingsPage::TimeEditor, 480, 800, 80, 350) ==
         SettingsAction::Digit1);
  assert(settings_page_action_at(SettingsPage::TimeEditor, 480, 800, 400,
                                 580) == SettingsAction::Save);
  char digit = 0;
  assert(settings_action_digit(SettingsAction::Digit7, digit));
  assert(digit == '7');
  return 0;
}
