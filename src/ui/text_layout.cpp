#include "text_layout.h"

#include <cstddef>
#include <cstring>

#include "font.h"

int ui_draw_wrapped_text_pixels(Canvas &canvas, int x, int y,
                                int maximum_width, const char *text,
                                uint8_t pixel_size, GrayLevel color,
                                int maximum_lines, int line_gap,
                                FontWeight weight) {
  if (text == nullptr || maximum_width <= 0 || pixel_size == 0U ||
      maximum_lines <= 0)
    return 0;
  const char *cursor = text;
  int rendered = 0;
  while (*cursor != '\0' && rendered < maximum_lines) {
    while (*cursor == ' ')
      ++cursor;
    const char *probe = cursor;
    size_t accepted = 0U;
    size_t last_space = 0U;
    char candidate[192] = {};
    while (*probe != '\0') {
      const char *next = probe;
      uint32_t codepoint = 0U;
      if (!font_decode_utf8(next, codepoint))
        break;
      const size_t candidate_length = static_cast<size_t>(next - cursor);
      if (candidate_length >= sizeof(candidate))
        break;
      std::memcpy(candidate, cursor, candidate_length);
      candidate[candidate_length] = '\0';
      if (font_text_width_pixels(candidate, pixel_size, weight) > maximum_width)
        break;
      accepted = candidate_length;
      if (codepoint == static_cast<uint32_t>(' '))
        last_space = static_cast<size_t>(probe - cursor);
      probe = next;
    }
    if (accepted == 0U) {
      const char *next = cursor;
      uint32_t ignored = 0U;
      if (!font_decode_utf8(next, ignored))
        break;
      accepted = static_cast<size_t>(next - cursor);
    }
    size_t line_length = accepted;
    if (*probe != '\0' && last_space > 0U)
      line_length = last_space;
    while (line_length > 0U && cursor[line_length - 1U] == ' ')
      --line_length;
    char line[192] = {};
    std::memcpy(line, cursor, line_length);
    canvas.draw_text_pixels(x, y + rendered * (pixel_size + line_gap), line,
                            pixel_size, color, weight);
    cursor += (*probe != '\0' && last_space > 0U) ? last_space + 1U : accepted;
    ++rendered;
  }
  return rendered;
}

int ui_draw_wrapped_text(Canvas &canvas, int x, int y, int maximum_width,
                         const char *text, uint8_t scale, GrayLevel color,
                         int maximum_lines, int line_gap) {
  return ui_draw_wrapped_text_pixels(
      canvas, x, y, maximum_width, text, font_ui_pixel_size(scale), color,
      maximum_lines, line_gap, FontWeight::Regular);
}
