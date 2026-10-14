#pragma once

#include <cstdint>

#include "canvas.h"

#ifdef UI_TEXT_OBSERVER
// Host preview only: called when wrapped text is cut off by the line limit.
using TextTruncationObserver = void (*)(const char *full_text);
void ui_set_truncation_observer(TextTruncationObserver observer);
#endif

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
