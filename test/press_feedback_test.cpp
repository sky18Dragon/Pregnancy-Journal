#include <cassert>
#include <vector>

#include "canvas.h"
#include "press_feedback.h"
#include "refresh_policy.h"

namespace {
// Two buttons: A (id 1) at 40,40 200x56 and B (id 2) at 300,100 300x160.
int action_at(int x, int y) {
  if (x >= 40 && x < 240 && y >= 40 && y < 96) return 1;
  if (x >= 300 && x < 600 && y >= 100 && y < 260) return 2;
  return 0;
}
} // namespace

int main() {
  using press_feedback::Rect;
  Rect bounds = {};
  assert(press_feedback::hit_bounds(800, 480, 100, 60, action_at, bounds));
  assert(bounds.x == 40 && bounds.y == 40 && bounds.width == 200 && bounds.height == 56);
  assert(press_feedback::hit_bounds(800, 480, 599, 259, action_at, bounds));
  assert(bounds.x == 300 && bounds.y == 100 && bounds.width == 300 && bounds.height == 160);
  assert(!press_feedback::hit_bounds(800, 480, 10, 10, action_at, bounds));
  assert(!press_feedback::hit_bounds(800, 480, -1, 10, action_at, bounds));

  std::vector<uint8_t> pixels(800U * 480U / 4U);
  Canvas canvas(800U, 480U, pixels.data(), pixels.size());
  for (int pass = 0; pass < 2; ++pass) {
    canvas.set_rotation(pass == 0 ? CanvasRotation::Deg0 : CanvasRotation::Deg90Clockwise);
    canvas.clear(GrayLevel::White);
    canvas.draw_rect(40, 40, 200, 56, GrayLevel::Black);  // button outline
    press_feedback::Rect small = {40, 40, 200, 56};
    press_feedback::draw(canvas, small);
    assert(canvas.pixel_at(41, 41) == GrayLevel::Black);   // interior inverted
    assert(canvas.pixel_at(40, 40) == GrayLevel::White);   // outline inverted
    assert(canvas.pixel_at(39, 39) == GrayLevel::White);   // outside untouched
    press_feedback::draw(canvas, small);                   // pressing twice restores
    assert(canvas.pixel_at(41, 41) == GrayLevel::White);
    assert(canvas.pixel_at(40, 40) == GrayLevel::Black);

    canvas.clear(GrayLevel::White);
    press_feedback::Rect large = {300, 100, 300, 160};     // too tall to invert
    press_feedback::draw(canvas, large);
    assert(canvas.pixel_at(300, 100) == GrayLevel::Black);
    assert(canvas.pixel_at(302, 102) == GrayLevel::Black); // 3 px frame
    assert(canvas.pixel_at(303, 103) == GrayLevel::White);
    assert(canvas.pixel_at(450, 180) == GrayLevel::White); // interior untouched
  }

  press_feedback::Pending pending;
  Rect taken = {};
  assert(!pending.take(taken));
  pending.set({1, 2, 3, 4});
  assert(pending.take(taken) && taken.x == 1 && taken.height == 4);
  assert(!pending.take(taken));

  PageRefreshTracker tracker;
  assert(tracker.page_changes(0));
  tracker.needs_full(0);
  assert(!tracker.page_changes(0));
  assert(tracker.page_changes(1));
  return 0;
}
