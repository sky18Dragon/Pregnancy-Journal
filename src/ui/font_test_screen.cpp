#include "font_test_screen.h"

#include <array>
#include <cstdio>

#include "canvas.h"
#include "font.h"
#include "ui_tokens.h"

namespace {

void draw_thick_line(Canvas &canvas, int x0, int y0, int x1, int y1,
                     int thickness) {
  for (int offset = 0; offset < thickness; ++offset) {
    if (y0 == y1)
      canvas.draw_line(x0, y0 + offset, x1, y1 + offset, GrayLevel::Black);
    else
      canvas.draw_line(x0 + offset, y0, x1 + offset, y1, GrayLevel::Black);
  }
}

void draw_test_row(Canvas &canvas, int y, int label_x, int regular_x,
                   int medium_x, uint8_t size) {
  char label[8] = {};
  std::snprintf(label, sizeof(label), "%u", static_cast<unsigned>(size));
  canvas.draw_text_pixels(label_x, y, label, 12, GrayLevel::Black,
                          FontWeight::Regular);
  canvas.draw_text_pixels(regular_x, y, "孕周 Ag12", size, GrayLevel::Black,
                          FontWeight::Regular);
  canvas.draw_text_pixels(medium_x, y, "孕周 Ag12", size, GrayLevel::Black,
                          FontWeight::Medium);
}

void draw_geometry(Canvas &canvas, int x, int y, int width) {
  canvas.draw_text_pixels(x, y, "线条 / 边框 / 图标 16·20·24", 14,
                          GrayLevel::Black, FontWeight::Medium);
  const int first_y = y + 28;
  draw_thick_line(canvas, x, first_y, x + width, first_y, 1);
  draw_thick_line(canvas, x, first_y + 16, x + width, first_y + 16, 2);

  const int border_y = first_y + 32;
  canvas.draw_rect(x, border_y, 64, 32, GrayLevel::Black);
  canvas.draw_rect(x + 76, border_y, 64, 32, GrayLevel::Black);
  canvas.draw_rect(x + 77, border_y + 1, 62, 30, GrayLevel::Black);

  const std::array<int, 3> icon_sizes = {
      ui_tokens::kIconSmall, ui_tokens::kIcon, ui_tokens::kIconLarge};
  const int icon_start = x + (width >= 300 ? 180 : 154);
  for (size_t index = 0U; index < icon_sizes.size(); ++index) {
    const int size = icon_sizes[index];
    const int center_x = icon_start + static_cast<int>(index) * 52;
    const int center_y = border_y + 16;
    canvas.draw_circle(center_x, center_y, size / 2, GrayLevel::Black);
    canvas.draw_circle(center_x, center_y, size / 2 - 1, GrayLevel::Black);
  }
}

void draw_required_samples(Canvas &canvas, int x, int y, int line_step,
                           uint8_t size) {
  constexpr const char *kSamples[] = {
      "孕期健康提醒",          "宝宝正在健康成长", "今天是孕期第 24 周",
      "距离预产期还有 112 天", "0123456789",       "ABCDEFGHIJKLMN",
      "abcdefghijklmn",        "孕馨醒餐检健康期",
  };
  for (size_t index = 0U; index < sizeof(kSamples) / sizeof(kSamples[0]);
       ++index) {
    canvas.draw_text_pixels(x, y + static_cast<int>(index) * line_step,
                            kSamples[index], size, GrayLevel::Black,
                            FontWeight::Regular);
  }
}

} // namespace

void ui_render_font_test_screen(Canvas &canvas) {
  canvas.clear(GrayLevel::White);
  const bool portrait = canvas.height() > canvas.width();
  canvas.draw_text_pixels(ui_tokens::kContentMargin, 16, "墨水屏字体测试",
                          ui_tokens::kPageHeading, GrayLevel::Black,
                          FontWeight::Medium);
  canvas.draw_text_pixels(ui_tokens::kContentMargin, 52,
                          "原生 1-bit 字形 · 小字笔画清晰", 14,
                          GrayLevel::Black, FontWeight::Regular);

  if (portrait) {
    canvas.draw_text_pixels(24, 80, "字号", 14, GrayLevel::Black,
                            FontWeight::Medium);
    canvas.draw_text_pixels(66, 80, "Regular", 14, GrayLevel::Black,
                            FontWeight::Medium);
    canvas.draw_text_pixels(258, 80, "Medium", 14, GrayLevel::Black,
                            FontWeight::Medium);
    canvas.draw_line(24, 103, 456, 103, GrayLevel::Black);
    constexpr std::array<uint8_t, 7> kSizes = {12U, 14U, 16U, 18U,
                                               20U, 22U, 24U};
    int y = 112;
    for (const uint8_t size : kSizes) {
      draw_test_row(canvas, y, 24, 66, 258, size);
      y += 32;
    }
    draw_required_samples(canvas, 24, 350, 27, 14);
    draw_geometry(canvas, 24, 572, 432);
    canvas.draw_text_pixels(24, 684,
                            "基线：中文 21 / 40 周 · 60% · 30 cm · 600 g", 14,
                            GrayLevel::Black, FontWeight::Regular);
    canvas.draw_text_pixels(24, 716, "检查：细笔不断、粗笔不糊、标点不漂移", 14,
                            GrayLevel::Black, FontWeight::Regular);
    return;
  }

  canvas.draw_text_pixels(24, 80, "字号", 14, GrayLevel::Black,
                          FontWeight::Medium);
  canvas.draw_text_pixels(66, 80, "Regular", 14, GrayLevel::Black,
                          FontWeight::Medium);
  canvas.draw_text_pixels(258, 80, "Medium", 14, GrayLevel::Black,
                          FontWeight::Medium);
  canvas.draw_line(24, 103, 480, 103, GrayLevel::Black);
  constexpr std::array<uint8_t, 7> kSizes = {12U, 14U, 16U, 18U, 20U, 22U, 24U};
  int y = 112;
  for (const uint8_t size : kSizes) {
    draw_test_row(canvas, y, 24, 66, 258, size);
    y += 42;
  }
  canvas.draw_text_pixels(520, 80, "规定文本 / 复杂字", 16, GrayLevel::Black,
                          FontWeight::Medium);
  draw_required_samples(canvas, 520, 112, 29, 14);
  draw_geometry(canvas, 520, 350, 230);
}
