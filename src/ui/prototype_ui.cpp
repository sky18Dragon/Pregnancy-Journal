#include "prototype_ui.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "font.h"
#include "text_layout.h"
#include "ui_language.h"
#include "ui_tokens.h"

namespace prototype_ui {

bool portrait_layout(int width, int height) { return height > width; }

Rect header_action_rect(int width, int height) {
  (void)height;
  return {width - 144, 20, 120, ui_tokens::kMinimumTouchHeight};
}

void begin_page(Canvas &canvas) { canvas.clear(GrayLevel::White); }

namespace {

// Splits `text` into two lines at the break that balances their widths:
// at a space when there is one, otherwise between two characters.
void split_two_lines(const char *text, uint8_t pixel_size, FontWeight weight,
                     char *first, char *second, size_t capacity) {
  const size_t length = std::strlen(text);
  size_t best = 0U;
  int best_width = 1 << 30;
  bool best_is_space = false;
  for (size_t index = 1U; index < length; ++index) {
    const bool space = text[index] == ' ';
    const bool boundary = (static_cast<uint8_t>(text[index]) & 0xC0U) != 0x80U;
    if (!space && !boundary) continue;
    char head[96] = {};
    const size_t head_length = std::min(index, sizeof(head) - 1U);
    std::memcpy(head, text, head_length);
    const int head_width = font_text_width_pixels(head, pixel_size, weight);
    const int tail_width = font_text_width_pixels(
        text + index + (space ? 1U : 0U), pixel_size, weight);
    const int widest = std::max(head_width, tail_width);
    // A space break always beats a mid-word break.
    if ((space && !best_is_space) ||
        (space == best_is_space && widest < best_width)) {
      best = index;
      best_width = widest;
      best_is_space = space;
    }
  }
  if (best == 0U) best = length / 2U;
  std::snprintf(first, capacity, "%.*s", static_cast<int>(best), text);
  std::snprintf(second, capacity, "%s", text + best + (best_is_space ? 1U : 0U));
}

} // namespace

void draw_centered_label(Canvas &canvas, const Rect &rect, const char *text,
                         uint8_t pixel_size, GrayLevel color,
                         FontWeight weight) {
  constexpr int kLabelMargin = 8;
  const int width = font_text_width_pixels(text, pixel_size, weight);
  if (width <= rect.width - 2 * kLabelMargin ||
      rect.height < 2 * pixel_size + 6) {
    canvas.draw_text_pixels(rect.x + (rect.width - width) / 2,
                            rect.y + (rect.height - pixel_size) / 2, text,
                            pixel_size, color, weight);
    return;
  }
  char first[96] = {};
  char second[96] = {};
  split_two_lines(text, pixel_size, weight, first, second, sizeof(first));
  const int top = rect.y + (rect.height - (2 * pixel_size + 4)) / 2;
  const char *lines[2] = {first, second};
  for (int line = 0; line < 2; ++line) {
    const int line_width = font_text_width_pixels(lines[line], pixel_size, weight);
    canvas.draw_text_pixels(rect.x + (rect.width - line_width) / 2,
                            top + line * (pixel_size + 4), lines[line],
                            pixel_size, color, weight);
  }
}

void draw_frame(Canvas &canvas, const Rect &rect, int stroke, GrayLevel color) {
  for (int inset = 0; inset < stroke; ++inset)
    canvas.draw_rect(rect.x + inset, rect.y + inset, rect.width - 2 * inset,
                     rect.height - 2 * inset, color);
}

void draw_progress_bar(Canvas &canvas, const Rect &rect, int percent) {
  const int clamped = std::max(0, std::min(100, percent));
  draw_frame(canvas, rect, ui_tokens::kBorderStroke);
  canvas.fill_rect(rect.x, rect.y, rect.width * clamped / 100, rect.height,
                   GrayLevel::Black);
}

void draw_button(Canvas &canvas, const Rect &rect, const char *label,
                 bool filled, uint8_t scale) {
  const GrayLevel foreground = filled ? GrayLevel::White : GrayLevel::Black;
  if (filled)
    canvas.fill_rect(rect.x, rect.y, rect.width, rect.height, GrayLevel::Black);
  else
    draw_frame(canvas, rect, ui_tokens::kBorderStroke);
  draw_centered_label(canvas, rect, ui_text(label), font_ui_pixel_size(scale),
                      foreground, FontWeight::Medium);
}

void draw_header(Canvas &canvas, const char *title, const char *subtitle,
                 const char *back_label) {
  const bool portrait = portrait_layout(canvas.width(), canvas.height());
  // Text must stay clear of the header action button.
  const int available =
      (back_label != nullptr
           ? header_action_rect(canvas.width(), canvas.height()).x - 12
           : canvas.width() - kPagePadding) -
      kPagePadding;
  uint8_t title_size = portrait ? ui_tokens::kSectionHeading : ui_tokens::kPageHeading;
  while (title_size > ui_tokens::kEmphasis &&
         font_text_width_pixels(ui_text(title), title_size,
                                FontWeight::Medium) > available)
    title_size = static_cast<uint8_t>(title_size - 2U);
  // A title that still does not fit wraps to two lines and pushes the
  // subtitle down, rather than running under the action button.
  int title_lines = 1;
  if (font_text_width_pixels(ui_text(title), title_size, FontWeight::Medium) >
      available) {
    title_lines = ui_draw_wrapped_text_pixels(
                      canvas, kPagePadding, 12, available, ui_text(title),
                      title_size, GrayLevel::Black, 2, 2, FontWeight::Medium) >
                          0
                      ? 2
                      : 1;
  } else {
    canvas.draw_text_pixels(kPagePadding, 22, ui_text(title), title_size,
                            GrayLevel::Black, FontWeight::Medium);
  }
  if (subtitle != nullptr)
    ui_draw_wrapped_text_pixels(canvas, kPagePadding,
                                title_lines == 2 ? 66 : 57, available,
                                ui_text(subtitle), ui_tokens::kSecondaryBody,
                                GrayLevel::Black, 2, 2, FontWeight::Regular);
  if (back_label != nullptr)
    draw_button(canvas, header_action_rect(canvas.width(), canvas.height()),
                back_label, false, 2);
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
