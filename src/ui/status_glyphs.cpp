#include "status_glyphs.h"

namespace {

constexpr int kBodyWidth = 44;
constexpr int kNubWidth = 4;
constexpr int kNubHeight = 8;
constexpr int kFrame = 2;
constexpr int kGap = 2;
constexpr int kBoltWidth = 8;
constexpr int kGroupGap = 14; // battery to moon

} // namespace

void battery_bolt_draw(Canvas &canvas, int x, int y, GrayLevel color) {
  canvas.draw_line(x + 5, y, x, y + 7, color);
  canvas.draw_line(x + 6, y, x + 1, y + 7, color);
  canvas.draw_line(x, y + 7, x + 6, y + 7, color);
  canvas.draw_line(x + 1, y + 8, x + 7, y + 8, color);
  canvas.draw_line(x + 6, y + 7, x + 1, y + 15, color);
  canvas.draw_line(x + 7, y + 8, x + 2, y + 15, color);
}

void battery_icon_draw(Canvas &canvas, int x, int y, int percent, bool charging,
                       GrayLevel foreground, GrayLevel background) {
  canvas.draw_rect(x, y, kBodyWidth, kBatteryIconHeight, foreground);
  canvas.draw_rect(x + 1, y + 1, kBodyWidth - 2, kBatteryIconHeight - 2, foreground);
  canvas.fill_rect(x + kBodyWidth, y + (kBatteryIconHeight - kNubHeight) / 2, kNubWidth, kNubHeight,
                   foreground);
  const int inset = kFrame + kGap;
  const int inner_width = kBodyWidth - 2 * inset;
  const int shown = percent > 100 ? 100 : percent;
  if (shown > 0)
    canvas.fill_rect(x + inset, y + inset, (inner_width * shown + 99) / 100,
                     kBatteryIconHeight - 2 * inset, foreground);
  if (charging)
    battery_bolt_draw(canvas, x + (kBodyWidth - kBoltWidth) / 2, y + (kBatteryIconHeight - 16) / 2,
                      shown >= 50 ? background : foreground);
}

void moon_icon_draw(Canvas &canvas, int cx, int cy, GrayLevel foreground, GrayLevel background) {
  (void)background; // only the crescent's own pixels are painted: the surroundings are never touched
  // A disc of radius 11 minus a disc of radius 10 shifted up and to the right.
  for (int y = -11; y <= 11; ++y)
    for (int x = -11; x <= 11; ++x) {
      const bool in_disc = x * x + y * y <= 11 * 11;
      const int dx = x - 5, dy = y + 5;
      const bool in_bite = dx * dx + dy * dy <= 10 * 10;
      if (in_disc && !in_bite) canvas.draw_pixel(cx + x, cy + y, foreground);
    }
}

namespace {

// A header action button is a frame: both vertical edges are solid foreground over the whole slot.
bool slot_holds_button(const Canvas &canvas, int x, int y, int width, int height, GrayLevel foreground) {
  for (int row = y; row < y + height; ++row)
    if (canvas.pixel_at(x, row) != foreground || canvas.pixel_at(x + width - 1, row) != foreground)
      return false;
  return true;
}

} // namespace

void sleep_status_draw(Canvas &canvas, int slot_x, int slot_y, int slot_width, int slot_height,
                       int percent, bool charging, GrayLevel foreground, GrayLevel background) {
  const int group = kBatteryIconWidth + kGroupGap + kMoonIconSize;
  const int left = slot_x + (slot_width - group) / 2;
  const int centre_y = slot_y + slot_height / 2;
  // Replace a button wholesale; on a page without one (the slot may hold the end of a long title)
  // only wipe the space the status itself needs.
  if (slot_holds_button(canvas, slot_x, slot_y, slot_width, slot_height, foreground))
    canvas.fill_rect(slot_x, slot_y, slot_width, slot_height, background);
  else
    canvas.fill_rect(left - 2, centre_y - kBatteryIconHeight / 2 - 2, group + 4, kBatteryIconHeight + 4,
                     background);
  battery_icon_draw(canvas, left, centre_y - kBatteryIconHeight / 2, percent, charging, foreground,
                    background);
  moon_icon_draw(canvas, left + kBatteryIconWidth + kGroupGap + kMoonIconSize / 2, centre_y,
                 foreground, background);
}
