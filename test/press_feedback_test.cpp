#include <cassert>
#include <vector>

#include "canvas.h"
#include "press_feedback.h"
#include "refresh_policy.h"

int main() {
  using press_feedback::Rect;
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
