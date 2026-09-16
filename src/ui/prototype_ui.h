#pragma once

#include <cstdint>

#include "canvas.h"

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
void draw_button(Canvas &canvas, const Rect &rect, const char *label,
                 bool filled = false, uint8_t scale = 2);
void draw_header(Canvas &canvas, const char *title, const char *subtitle,
                 const char *back_label = nullptr);
void draw_label(Canvas &canvas, int x, int y, const char *label);
void draw_chevron(Canvas &canvas, int x, int y,
                  GrayLevel color = GrayLevel::Black);
void draw_status_square(Canvas &canvas, int x, int y, bool done,
                        GrayLevel color);

} // namespace prototype_ui
