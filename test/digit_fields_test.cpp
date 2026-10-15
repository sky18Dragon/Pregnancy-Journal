#include <cassert>
#include <cstring>
#include <vector>

#include "canvas.h"
#include "digit_fields.h"

namespace {
void type(DigitFields &f, const char *digits) {
  for (const char *p = digits; *p; ++p) assert(f.input_digit(*p));
}
} // namespace

int main() {
  // Sequential entry fills the fields and moves on automatically.
  DigitFields clock(DigitFieldKind::DateTime);
  assert(std::strcmp(clock.digits(), "____________") == 0);
  assert(!clock.complete());
  type(clock, "202610101530");
  assert(clock.complete());
  assert(std::strcmp(clock.digits(), "202610101530") == 0);
  assert(clock.active_field() == 4U); // stays on the last field

  // Editing one field replaces just that field.
  clock.select_field(4U);
  type(clock, "45");
  assert(std::strcmp(clock.digits(), "202610101545") == 0);
  clock.select_field(3U);
  type(clock, "07");
  assert(std::strcmp(clock.digits(), "202610100745") == 0);
  assert(clock.active_field() == 4U); // completing hour selects minute

  // Selecting then typing one digit leaves a partial field until finished.
  clock.select_field(1U);
  type(clock, "1");
  assert(std::strcmp(clock.digits(), "20261_100745") == 0);
  assert(!clock.complete());
  type(clock, "2");
  assert(clock.complete());
  assert(clock.active_field() == 2U);

  // Impossible digits are rejected without changing anything.
  clock.select_field(1U);
  assert(!clock.input_digit('2')); // month cannot start with 2
  assert(clock.input_digit('1'));
  assert(!clock.input_digit('3')); // 13
  assert(clock.input_digit('2'));
  clock.select_field(2U);
  assert(!clock.input_digit('4')); // day cannot start with 4
  assert(clock.input_digit('3'));
  assert(!clock.input_digit('2')); // 32
  assert(clock.input_digit('1'));
  clock.select_field(3U);
  assert(!clock.input_digit('3')); // hour cannot start with 3
  assert(clock.input_digit('2'));
  assert(!clock.input_digit('4')); // 24
  assert(clock.input_digit('3'));
  clock.select_field(4U);
  assert(!clock.input_digit('6')); // minute cannot start with 6
  assert(clock.input_digit('5'));
  assert(clock.input_digit('9'));
  clock.select_field(0U);
  assert(!clock.input_digit('3'));
  assert(!clock.input_digit('0'));
  assert(clock.input_digit('2'));
  assert(!clock.input_digit('1')); // 21xx
  assert(clock.input_digit('0'));
  assert(!clock.input_digit('x'));

  // set_digits loads a full value and selects the first field.
  DigitFields due(DigitFieldKind::Date);
  assert(due.field_count() == 3U && due.total() == 8U);
  assert(due.set_digits("20270220"));
  assert(due.complete() && due.active_field() == 0U);
  assert(!due.set_digits("2027022"));
  assert(!due.complete());
  assert(std::strcmp(due.digits(), "________") == 0);
  assert(!due.set_digits("2027022a"));

  // Erase: clears typed digits, then the field, then moves back.
  DigitFields e(DigitFieldKind::DateTime);
  type(e, "20261");               // year done, month has one digit typed
  assert(e.active_field() == 1U);
  e.erase();                      // removes the typed "1"
  assert(std::strcmp(e.digits(), "2026________") == 0);
  e.erase();                      // empty field: back to the year
  assert(e.active_field() == 0U);
  e.erase();                      // clears the year
  assert(std::strncmp(e.digits(), "____", 4) == 0);

  // Geometry: cells fit the box, are tap-sized, ordered and disjoint.
  const prototype_ui::Rect boxes[] = {{30, 118, 430, 86}, {24, 102, 432, 76},
                                      {24, 156, 424, 64}, {24, 150, 432, 72}};
  const uint8_t sizes[] = {28, 21, 28, 21};
  for (size_t b = 0U; b < 4U; ++b) {
    for (DigitFieldKind kind : {DigitFieldKind::DateTime, DigitFieldKind::Date}) {
      prototype_ui::Rect cells[5];
      const size_t count = digit_field_cells(boxes[b], kind, sizes[b], cells, 5U);
      assert(count == (kind == DigitFieldKind::DateTime ? 5U : 3U));
      for (size_t i = 0U; i < count; ++i) {
        assert(cells[i].width >= 48 && cells[i].height >= 48);
        assert(cells[i].x >= boxes[b].x &&
               cells[i].x + cells[i].width <= boxes[b].x + boxes[b].width);
        assert(cells[i].y >= boxes[b].y &&
               cells[i].y + cells[i].height <= boxes[b].y + boxes[b].height);
        if (i > 0U) assert(cells[i].x >= cells[i - 1U].x + cells[i - 1U].width);
      }
    }
  }

  // Drawing leaves the canvas binary and marks the active field.
  std::vector<uint8_t> pixels(800U * 480U / 4U);
  Canvas canvas(800U, 480U, pixels.data(), pixels.size());
  canvas.clear(GrayLevel::White);
  DigitFields draw(DigitFieldKind::DateTime);
  assert(draw.set_digits("202610101530"));
  digit_fields_draw(canvas, boxes[0], draw, 28);
  prototype_ui::Rect cells[5];
  digit_field_cells(boxes[0], DigitFieldKind::DateTime, 28, cells, 5U);
  assert(canvas.pixel_at(cells[0].x + 1, cells[0].y + 1) == GrayLevel::Black);
  assert(canvas.pixel_at(cells[1].x + 1, cells[1].y + 1) == GrayLevel::White);
  return 0;
}
