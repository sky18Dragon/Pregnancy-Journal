#include <cassert>
#include <cstdint>
#include <fstream>

#include "canvas.h"
#include "chinese_font_assets.h"
#include "font.h"
#include "font_test_screen.h"

namespace {

int black_pixel_count(const Canvas &canvas) {
  int count = 0;
  for (int y = 0; y < canvas.height(); ++y) {
    for (int x = 0; x < canvas.width(); ++x) {
      if (canvas.pixel_at(x, y) == GrayLevel::Black) {
        ++count;
      }
    }
  }
  return count;
}

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
  for (uint8_t size = 12U; size <= 24U; ++size)
    assert(font_resolved_pixel_size(size, FontWeight::Regular) == size);
  const ChineseFontStrike &strike =
      chinese_font_strike(18U, FontWeight::Regular);
  const ChineseFontGlyph *cjk = chinese_font_glyph(strike, 0x5B55U);
  const ChineseFontGlyph *latin = chinese_font_glyph(strike, 'g');
  const ChineseFontGlyph *dense = chinese_font_glyph(strike, 0x99A8U);
  assert(cjk != nullptr && latin != nullptr);
  assert(dense != nullptr);
  assert(cjk->advance > 0U && latin->advance > 0U);
  assert(cjk->bearing_y < 0 && latin->bearing_y < 0);

  // Unsupported codepoints must resolve to the native-size '?' strike rather
  // than the legacy 5x7 bitmap scaler.
  constexpr char kUnsupported[] = "\xF4\x8F\xBF\xBF"; // U+10FFFF
  assert(font_text_width_pixels(kUnsupported, 18U, FontWeight::Regular) ==
         font_text_width_pixels("?", 18U, FontWeight::Regular));
  canvas.clear();
  canvas.draw_text_pixels(0, 0, kUnsupported, 18U, GrayLevel::Black,
                          FontWeight::Regular);
  const int unsupported_pixels = black_pixel_count(canvas);
  canvas.clear();
  canvas.draw_text_pixels(0, 0, "?", 18U, GrayLevel::Black,
                          FontWeight::Regular);
  assert(unsupported_pixels == black_pixel_count(canvas));

  // Real-panel evidence showed that the former W3 Regular strikes could leave
  // only fragile one-pixel segments at body-copy sizes. The reinforced small
  // Regular family must retain most of Medium's ink coverage while remaining
  // optically lighter and therefore distinct in the hierarchy.
  for (uint8_t size = 12U; size <= 18U; ++size) {
    canvas.clear();
    canvas.draw_text_pixels(0, 0, "胎动比平时少规律改变", size,
                            GrayLevel::Black, FontWeight::Regular);
    const int reinforced_regular_pixels = black_pixel_count(canvas);
    canvas.clear();
    canvas.draw_text_pixels(0, 0, "胎动比平时少规律改变", size,
                            GrayLevel::Black, FontWeight::Medium);
    const int medium_pixels = black_pixel_count(canvas);
    assert(reinforced_regular_pixels < medium_pixels);
    assert(reinforced_regular_pixels * 100 >= medium_pixels * 80);
  }

  ui_render_font_test_screen(canvas);
  assert(canvas.width() == 800U && canvas.height() == 480U);
  write_preview(canvas, "/tmp/font_test_landscape.ppm");

  canvas.set_rotation(CanvasRotation::Deg90Clockwise);
  ui_render_font_test_screen(canvas);
  assert(canvas.width() == 480U && canvas.height() == 800U);
  write_preview(canvas, "/tmp/font_test_portrait.ppm");
  return 0;
}
