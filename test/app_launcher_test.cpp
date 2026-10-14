#include <cassert>
#include <cstdint>
#include <fstream>
#include <initializer_list>

#include "app_pages.h"
#include "canvas.h"
#include "prototype_ui.h"
#include "ui_language.h"
#include "ui_tokens.h"

namespace {
size_t count_level(Canvas &canvas, GrayLevel level) {
  size_t count = 0U;
  for (int y = 0; y < canvas.height(); ++y)
    for (int x = 0; x < canvas.width(); ++x)
      count += canvas.pixel_at(x, y) == level ? 1U : 0U;
  return count;
}

void write_preview(Canvas &canvas, const char *path) {
  std::ofstream output(path, std::ios::binary);
  output << "P6\n" << canvas.width() << " " << canvas.height()
         << "\n255\n";
  for (int y = 0; y < canvas.height(); ++y) {
    for (int x = 0; x < canvas.width(); ++x) {
      const uint8_t value = static_cast<uint8_t>(canvas.pixel_at(x, y)) * 85U;
      output.write(reinterpret_cast<const char *>(&value), 1);
      output.write(reinterpret_cast<const char *>(&value), 1);
      output.write(reinterpret_cast<const char *>(&value), 1);
    }
  }
}
} // namespace

int main() {
  const prototype_ui::Rect landscape_header_action =
      prototype_ui::header_action_rect(800, 480);
  const prototype_ui::Rect portrait_header_action =
      prototype_ui::header_action_rect(480, 800);
  assert(landscape_header_action.height >= ui_tokens::kMinimumTouchHeight);
  assert(portrait_header_action.height >= ui_tokens::kMinimumTouchHeight);

  uint8_t framebuffer[800U * 480U / 2U] = {};
  Canvas canvas(800U, 480U, framebuffer, sizeof(framebuffer));
  ui_language_set(UiLanguage::English);
  app_page_render_launcher(canvas, StickyAppId::Pregnancy);
  write_preview(canvas, "/tmp/app_launcher.ppm");
  StickyAppId selected = StickyAppId::Settings;
  assert(app_page_launcher_app_at(800, 480, 100, 150, selected));
  assert(selected == StickyAppId::Pregnancy);
  assert(app_page_launcher_app_at(800, 480, 500, 150, selected));
  assert(selected == StickyAppId::Weight);
  assert(app_page_launcher_app_at(800, 480, 100, 310, selected));
  assert(selected == StickyAppId::Kicks);
  assert(app_page_launcher_app_at(800, 480, 500, 310, selected));
  assert(selected == StickyAppId::Settings);
  assert(!app_page_launcher_app_at(800, 480, 400, 310, selected));
  assert(!app_page_launcher_app_at(800, 480, 100, 430, selected));
  assert(app_page_launcher_close_at(800, 480, 720, 45));
  assert(app_page_launcher_close_at(800, 480, 720, 67));
  assert(!app_page_launcher_close_at(800, 480, 720, 68));
  assert(!app_page_launcher_language_at(800, 480, 730, 50));
  ui_language_set(UiLanguage::ChineseSimplified);
  app_page_render_launcher(canvas, StickyAppId::Pregnancy);
  assert(canvas.pixel_at(24, 99) != GrayLevel::White);
  assert(count_level(canvas, GrayLevel::DarkGray) == 0U);
  assert(count_level(canvas, GrayLevel::LightGray) == 0U);
  write_preview(canvas, "/tmp/app_launcher_zh.ppm");

  canvas.set_rotation(CanvasRotation::Deg90Clockwise);
  app_page_render_launcher(canvas, StickyAppId::Pregnancy);
  assert(canvas.width() == 480U);
  assert(canvas.height() == 800U);
  assert(app_page_launcher_app_at(480, 800, 100, 150, selected));
  assert(selected == StickyAppId::Pregnancy);
  assert(app_page_launcher_app_at(480, 800, 100, 310, selected));
  assert(selected == StickyAppId::Weight);
  assert(app_page_launcher_app_at(480, 800, 100, 450, selected));
  assert(selected == StickyAppId::Kicks);
  assert(app_page_launcher_app_at(480, 800, 100, 600, selected));
  assert(selected == StickyAppId::Settings);
  assert(!app_page_launcher_app_at(480, 800, 100, 720, selected));
  assert(app_page_launcher_close_at(480, 800, 400, 40));
  assert(app_page_launcher_close_at(480, 800, 400, 67));
  assert(!app_page_launcher_close_at(480, 800, 400, 68));
  assert(!app_page_launcher_app_at(480, 800, 470, 680, selected));
  assert(count_level(canvas, GrayLevel::DarkGray) == 0U);
  assert(count_level(canvas, GrayLevel::LightGray) == 0U);
  write_preview(canvas, "/tmp/app_launcher_portrait_zh.ppm");
  for (const CanvasRotation rotation : {CanvasRotation::Deg0,
                                        CanvasRotation::Deg90Clockwise,
                                        CanvasRotation::Deg180,
                                        CanvasRotation::Deg90CounterClockwise}) {
    canvas.set_rotation(rotation);
    for (int y = 0; y < canvas.height(); ++y) {
      for (int x = 0; x < canvas.width(); ++x) {
        if (app_page_launcher_app_at(canvas.width(), canvas.height(), x, y,
                                    selected)) {
          assert(selected == StickyAppId::Pregnancy ||
                 selected == StickyAppId::Weight ||
                 selected == StickyAppId::Kicks ||
                 selected == StickyAppId::Settings);
          assert(!app_page_launcher_close_at(canvas.width(), canvas.height(),
                                             x, y));
        }
      }
    }
  }
  return 0;
}
