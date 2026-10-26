#pragma once

#include <cstdint>

#include "canvas.h"
#include "ui_tokens.h"

namespace prototype_ui {

struct Rect {
  int x;
  int y;
  int width;
  int height;

  bool contains(int px, int py) const {
    return px >= x && py >= y && px < x + width && py < y + height;
  }
};

constexpr int kPagePadding = 24;
constexpr int kHeaderHeight = 64;
constexpr int kFooterY = 400;
constexpr int kFooterHeight = 56;

bool portrait_layout(int width, int height);
Rect header_action_rect(int width, int height);
void begin_page(Canvas &canvas);
// Draws already-translated `text` centred in `rect`. A label wider than the
// rectangle is split over two lines instead of spilling out of it.
void draw_centered_label(Canvas &canvas, const Rect &rect, const char *text,
                         uint8_t pixel_size, GrayLevel color,
                         FontWeight weight = FontWeight::Medium);
// Draws a rectangular border of `stroke` px inside `rect` (the outer extent
// does not change with the stroke).
void draw_frame(Canvas &canvas, const Rect &rect, int stroke,
                GrayLevel color = GrayLevel::Black);
// Horizontal progress bar: a bordered track whose left `percent` (0-100) is
// filled black.
void draw_progress_bar(Canvas &canvas, const Rect &rect, int percent);
void draw_button(Canvas &canvas, const Rect &rect, const char *label,
                 bool filled = false, uint8_t scale = 2);
void draw_button_pixels(Canvas &canvas, const Rect &rect, const char *label,
                        bool filled, uint8_t pixel_size);
void draw_header(Canvas &canvas, const char *title, const char *subtitle,
                 const char *back_label = nullptr);
void draw_chevron(Canvas &canvas, int x, int y,
                  GrayLevel color = GrayLevel::Black);
void draw_status_square(Canvas &canvas, int x, int y, bool done,
                        GrayLevel color);

} // namespace prototype_ui
