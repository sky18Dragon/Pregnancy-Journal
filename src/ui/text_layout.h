#pragma once

#include <cstdint>

#include "canvas.h"

// Draws bounded UTF-8 text, wrapping at spaces when possible and at code-point
// boundaries otherwise. Returns the number of rendered lines.
int ui_draw_wrapped_text(Canvas &canvas, int x, int y, int maximum_width,
                         const char *text, uint8_t scale, GrayLevel color,
                         int maximum_lines, int line_gap = 6);

// Draws bounded UTF-8 text at an exact pixel height.
int ui_draw_wrapped_text_pixels(Canvas &canvas, int x, int y,
                                int maximum_width, const char *text,
                                uint8_t pixel_size, GrayLevel color,
                                int maximum_lines, int line_gap = 6,
                                FontWeight weight = FontWeight::Medium);
