#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include "canvas.h"
#include "prototype_ui.h"
#include "status_glyphs.h"

namespace {

// The canvas points into `pixels`, so a Surface must never be copied or moved.
struct Surface {
  std::vector<uint8_t> pixels;
  Canvas canvas;
  Surface(const Surface &) = delete;
  Surface &operator=(const Surface &) = delete;
  Surface(uint16_t width, uint16_t height)
      : pixels(static_cast<size_t>((width + 3U) / 4U) * height, 0xFFU), // 4 px per byte, rows byte-aligned
        canvas(width, height, pixels.data(), pixels.size()) {
    canvas.clear(GrayLevel::White);
  }
};

struct Bounds {
  int left = 1 << 20, top = 1 << 20, right = -1, bottom = -1;
  bool empty() const { return right < 0; }
  int centre_x() const { return (left + right) / 2; }
  int centre_y() const { return (top + bottom) / 2; }
};

Bounds ink_bounds(const Canvas &canvas, int x, int y, int width, int height) {
  Bounds b;
  for (int row = y; row < y + height; ++row)
    for (int col = x; col < x + width; ++col)
      if (canvas.pixel_at(col, row) == GrayLevel::Black) {
        b.left = std::min(b.left, col);
        b.top = std::min(b.top, row);
        b.right = std::max(b.right, col);
        b.bottom = std::max(b.bottom, row);
      }
  return b;
}

size_t black_count(const Canvas &canvas, int x, int y, int width, int height) {
  size_t count = 0;
  for (int row = y; row < y + height; ++row)
    for (int col = x; col < x + width; ++col)
      count += canvas.pixel_at(col, row) == GrayLevel::Black ? 1U : 0U;
  return count;
}

void test_battery_icon() {
  constexpr int x = 6, y = 5;
  const auto draw = [&](Surface &s, int percent, bool charging) {
    battery_icon_draw(s.canvas, x, y, percent, charging, GrayLevel::Black, GrayLevel::White);
  };
  const auto count = [&](int percent, bool charging) {
    Surface s(70, 40);
    draw(s, percent, charging);
    return black_count(s.canvas, 0, 0, 70, 40);
  };
  // Everything stays inside the 48 x 22 footprint.
  Surface full(70, 40);
  draw(full, 100, false);
  const Bounds b = ink_bounds(full.canvas, 0, 0, 70, 40);
  assert(b.left == x && b.top == y && b.right == x + kBatteryIconWidth - 1 &&
         b.bottom == y + kBatteryIconHeight - 1);
  // The fill alone shows the charge: inner area is 36 px wide starting 4 px inside the frame.
  const int mid = y + kBatteryIconHeight / 2;
  assert(full.canvas.pixel_at(x + 4, mid) == GrayLevel::Black);
  assert(full.canvas.pixel_at(x + 39, mid) == GrayLevel::Black);
  Surface half(70, 40);
  draw(half, 50, false);
  assert(half.canvas.pixel_at(x + 4 + 17, mid) == GrayLevel::Black);
  assert(half.canvas.pixel_at(x + 4 + 20, mid) == GrayLevel::White);
  for (const int empty : {0, -1}) {
    Surface s(70, 40);
    draw(s, empty, false);
    assert(s.canvas.pixel_at(x + 4, mid) == GrayLevel::White);   // frame only
    assert(s.canvas.pixel_at(x + 1, mid) == GrayLevel::Black);   // 2 px frame
    assert(s.canvas.pixel_at(x + 3, mid) == GrayLevel::White);   // 2 px gap
  }
  // Fill grows monotonically with the charge.
  size_t previous = 0;
  for (const int percent : {0, 10, 30, 60, 100}) {
    const size_t c = count(percent, false);
    assert(c >= previous);
    previous = c;
  }
  // The bolt shows when charging: white over a full battery, black over a low one.
  assert(count(100, true) < count(100, false));
  assert(count(10, true) > count(10, false));
}

void test_moon() {
  Surface s(60, 60);
  moon_icon_draw(s.canvas, 30, 30, GrayLevel::Black, GrayLevel::White);
  const Bounds b = ink_bounds(s.canvas, 0, 0, 60, 60);
  assert(!b.empty());
  assert(b.left >= 30 - kMoonIconSize / 2 - 1 && b.right <= 30 + kMoonIconSize / 2 + 1);
  assert(b.top >= 30 - kMoonIconSize / 2 - 1 && b.bottom <= 30 + kMoonIconSize / 2 + 1);
  assert(black_count(s.canvas, 0, 0, 60, 60) > 100U);
}

// The sleep status replaces the header action button and touches nothing else.
void test_sleep_status(uint16_t width, uint16_t height) {
  const prototype_ui::Rect slot = prototype_ui::header_action_rect(width, height);
  Surface before(width, height), after(width, height);
  for (Surface *s : {&before, &after}) {
    prototype_ui::begin_page(s->canvas);
    prototype_ui::draw_header(s->canvas, "WEIGHT", "TRACK YOUR WEIGHT", "APPS");
  }
  assert(black_count(before.canvas, slot.x, slot.y, slot.width, slot.height) > 200U); // the button
  sleep_status_draw(after.canvas, slot.x, slot.y, slot.width, slot.height, 70, false,
                    GrayLevel::Black, GrayLevel::White);

  // Nothing outside the slot changed.
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x) {
      const bool inside = slot.contains(x, y);
      if (!inside) assert(before.canvas.pixel_at(x, y) == after.canvas.pixel_at(x, y));
    }
  // The button is gone: its frame and label no longer show.
  assert(after.canvas.pixel_at(slot.x, slot.y + slot.height / 2) == GrayLevel::White);
  assert(after.canvas.pixel_at(slot.x + slot.width - 1, slot.y + slot.height / 2) == GrayLevel::White);
  // The battery and the moon are centred as one group in the slot, with a clear gap between them.
  const Bounds group = ink_bounds(after.canvas, slot.x, slot.y, slot.width, slot.height);
  assert(!group.empty());
  assert(std::abs(group.centre_x() - (slot.x + slot.width / 2)) <= 3);
  assert(std::abs(group.centre_y() - (slot.y + slot.height / 2)) <= 3);
  assert(group.left >= slot.x + 8 && group.right <= slot.x + slot.width - 8);
  const int battery_right = group.left + kBatteryIconWidth - 1;
  const Bounds moon = ink_bounds(after.canvas, battery_right + 1, slot.y, slot.x + slot.width - battery_right - 1,
                                 slot.height);
  assert(!moon.empty() && moon.left - battery_right >= 8);
}

// A page without a header button: a long title may run under the slot and must survive, apart from
// the footprint the status itself needs.
void test_sleep_status_without_button() {
  constexpr uint16_t width = 480, height = 800;
  const prototype_ui::Rect slot = prototype_ui::header_action_rect(width, height);
  Surface before(width, height), after(width, height);
  for (Surface *s : {&before, &after}) {
    prototype_ui::begin_page(s->canvas);
    s->canvas.draw_text_pixels(24, 22, "PREGNANCY SETUP PAGE", 28, GrayLevel::Black, FontWeight::Medium);
  }
  assert(black_count(before.canvas, slot.x, slot.y, slot.width, slot.height) > 0U); // title under the slot
  sleep_status_draw(after.canvas, slot.x, slot.y, slot.width, slot.height, 70, false, GrayLevel::Black,
                    GrayLevel::White);
  const int group = kBatteryIconWidth + 14 + kMoonIconSize;
  const int left = slot.x + (slot.width - group) / 2 - 2;
  const int right = left + group + 4;
  const int top = slot.y + slot.height / 2 - kBatteryIconHeight / 2 - 2;
  const int bottom = top + kBatteryIconHeight + 4;
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x)
      if (!(x >= left && x < right && y >= top && y < bottom))
        assert(before.canvas.pixel_at(x, y) == after.canvas.pixel_at(x, y));
  assert(black_count(after.canvas, left, top, right - left, bottom - top) > 100U); // the status is there
}

} // namespace

int main() {
  test_battery_icon();
  test_moon();
  test_sleep_status(800, 480); // landscape
  test_sleep_status(480, 800); // portrait
  test_sleep_status_without_button();
  return 0;
}
