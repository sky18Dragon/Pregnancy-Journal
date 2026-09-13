#include <cassert>
#include <cstdint>
#include <fstream>

#include "app_pages.h"
#include "canvas.h"
#include "ui_language.h"

namespace {
void write_preview(Canvas &canvas, const char *path) {
  std::ofstream output(path, std::ios::binary);
  output << "P6\n800 480\n255\n";
  for (int y = 0; y < 480; ++y) {
    for (int x = 0; x < 800; ++x) {
      const uint8_t value = static_cast<uint8_t>(canvas.pixel_at(x, y)) * 85U;
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
  app_page_render_launcher(canvas, StickyAppId::Pregnancy);
  write_preview(canvas, "/tmp/app_launcher.ppm");
  StickyAppId selected = StickyAppId::Settings;
  assert(app_page_launcher_app_at(800, 480, 100, 190, selected));
  assert(selected == StickyAppId::Pregnancy);
  assert(app_page_launcher_app_at(800, 480, 350, 190, selected));
  assert(selected == StickyAppId::Checkup);
  assert(app_page_launcher_app_at(800, 480, 650, 190, selected));
  assert(selected == StickyAppId::Reminder);
  assert(app_page_launcher_app_at(800, 480, 350, 330, selected));
  assert(selected == StickyAppId::Weight);
  assert(app_page_launcher_app_at(800, 480, 650, 330, selected));
  assert(selected == StickyAppId::Kicks);
  assert(app_page_launcher_app_at(800, 480, 620, 50, selected));
  assert(selected == StickyAppId::Settings);
  assert(!app_page_launcher_app_at(800, 480, 270, 260, selected));
  assert(app_page_launcher_language_at(800, 480, 730, 50));
  ui_language_set(UiLanguage::ChineseSimplified);
  app_page_render_launcher(canvas, StickyAppId::Pregnancy);
  assert(canvas.pixel_at(300, 132) == GrayLevel::Black);
  assert(canvas.pixel_at(400, 144) == GrayLevel::Black);
  write_preview(canvas, "/tmp/app_launcher_zh.ppm");
  return 0;
}
