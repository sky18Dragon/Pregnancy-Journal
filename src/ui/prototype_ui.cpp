#include "prototype_ui.h"

#include "font.h"
#include "ui_language.h"
#include "ui_tokens.h"

namespace prototype_ui {

bool portrait_layout(int width, int height) { return height > width; }

Rect header_action_rect(int width, int height) {
  (void)height;
  return {width - 144, 20, 120, ui_tokens::kMinimumTouchHeight};
}

void begin_page(Canvas &canvas) { canvas.clear(GrayLevel::White); }

void draw_button(Canvas &canvas, const Rect &rect, const char *label,
                 bool filled, uint8_t scale) {
  const GrayLevel foreground = filled ? GrayLevel::White : GrayLevel::Black;
  if (filled)
    canvas.fill_rect(rect.x, rect.y, rect.width, rect.height, GrayLevel::Black);
  else
    canvas.draw_rect(rect.x, rect.y, rect.width, rect.height, GrayLevel::Black);
  const char *translated = ui_text(label);
  const int width = font_text_width(translated, scale, FontWeight::Medium);
  const int height = font_ui_pixel_size(scale);
  canvas.draw_text(rect.x + (rect.width - width) / 2,
                   rect.y + (rect.height - height) / 2, translated, scale,
                   foreground, FontWeight::Medium);
}

void draw_header(Canvas &canvas, const char *title, const char *subtitle,
                 const char *back_label) {
  const bool portrait = portrait_layout(canvas.width(), canvas.height());
  canvas.draw_text_pixels(kPagePadding, 22, ui_text(title),
                          portrait ? 24U : 28U, GrayLevel::Black,
                          FontWeight::Medium);
  if (subtitle != nullptr)
    canvas.draw_text_pixels(kPagePadding, 57, ui_text(subtitle), 14U,
                            GrayLevel::Black, FontWeight::Regular);
  if (back_label != nullptr)
    draw_button(canvas, header_action_rect(canvas.width(), canvas.height()),
                back_label, false, 2);
}

void draw_label(Canvas &canvas, int x, int y, const char *label) {
  canvas.draw_text(x, y, ui_text(label), 2, GrayLevel::Black,
                   FontWeight::Medium);
}

void draw_chevron(Canvas &canvas, int x, int y, GrayLevel color) {
  canvas.draw_line(x, y, x + 7, y + 8, color);
  canvas.draw_line(x + 7, y + 8, x, y + 16, color);
  canvas.draw_line(x + 1, y, x + 8, y + 8, color);
  canvas.draw_line(x + 8, y + 8, x + 1, y + 16, color);
}

void draw_status_square(Canvas &canvas, int x, int y, bool done,
                        GrayLevel color) {
  if (done)
    canvas.fill_rect(x, y, 14, 14, color);
  else
    canvas.draw_rect(x, y, 14, 14, color);
}

} // namespace prototype_ui
