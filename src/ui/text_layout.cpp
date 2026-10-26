#include "text_layout.h"

#include <cctype>
#include <cstddef>
#include <cstring>

#include "font.h"

namespace {
bool opening_punctuation(uint32_t codepoint) {
  return codepoint == 0x3008U || codepoint == 0x300AU ||
         codepoint == 0x300CU || codepoint == 0x300EU ||
         codepoint == 0x3010U || codepoint == 0xFF08U ||
         codepoint == 0x2018U || codepoint == 0x201CU;
}

bool closing_punctuation(uint32_t codepoint) {
  return codepoint == 0x3001U || codepoint == 0x3002U ||
         codepoint == 0xFF0CU || codepoint == 0xFF0EU ||
         codepoint == 0xFF01U || codepoint == 0xFF1FU ||
         codepoint == 0xFF1AU || codepoint == 0xFF1BU ||
         codepoint == 0x3009U || codepoint == 0x300BU ||
         codepoint == 0x300DU || codepoint == 0x300FU ||
         codepoint == 0x3011U || codepoint == 0xFF09U ||
         codepoint == 0x2019U || codepoint == 0x201DU;
}
} // namespace

#ifdef UI_TEXT_OBSERVER
namespace {
TextTruncationObserver s_truncation_observer = nullptr;
}
void ui_set_truncation_observer(TextTruncationObserver observer) {
  s_truncation_observer = observer;
}
#endif

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
    size_t break_length = 0U; // line length at the last break opportunity
    size_t break_skip = 0U;   // characters consumed after the break (the space)
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
      if (codepoint == static_cast<uint32_t>(' ')) {
        break_length = static_cast<size_t>(probe - cursor);
        break_skip = 1U;
      } else if (codepoint == static_cast<uint32_t>('-') && probe > cursor &&
                 std::isalpha(static_cast<unsigned char>(probe[-1])) != 0 &&
                 std::isalpha(static_cast<unsigned char>(*next)) != 0) {
        // A hyphen between letters is a break opportunity; it stays at the end
        // of the line (so "-50%" or "10-12" are never split).
        break_length = candidate_length;
        break_skip = 0U;
      } else if (codepoint >= 0x2E80U && !opening_punctuation(codepoint)) {
        const char *lookahead = next;
        uint32_t following = 0U;
        font_decode_utf8(lookahead, following);
        // Chinese can wrap between characters, but punctuation belongs with
        // its phrase. Do not leave a closing mark at the start of a line.
        if (!closing_punctuation(following)) {
          break_length = candidate_length;
          break_skip = 0U;
        }
      }
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
    const bool wrap_at_break = *probe != '\0' && break_length > 0U;
    if (wrap_at_break)
      line_length = break_length;
    while (line_length > 0U && cursor[line_length - 1U] == ' ')
      --line_length;
    char line[192] = {};
    std::memcpy(line, cursor, line_length);
    canvas.draw_text_pixels(x, y + rendered * (pixel_size + line_gap), line,
                            pixel_size, color, weight);
    cursor += wrap_at_break ? break_length + break_skip : accepted;
    ++rendered;
  }
#ifdef UI_TEXT_OBSERVER
  while (*cursor == ' ')
    ++cursor;
  if (*cursor != '\0' && s_truncation_observer != nullptr)
    s_truncation_observer(text);
#endif
  return rendered;
}

int ui_draw_wrapped_text(Canvas &canvas, int x, int y, int maximum_width,
                         const char *text, uint8_t scale, GrayLevel color,
                         int maximum_lines, int line_gap) {
  return ui_draw_wrapped_text_pixels(
      canvas, x, y, maximum_width, text, font_ui_pixel_size(scale), color,
      maximum_lines, line_gap, FontWeight::Regular);
}
