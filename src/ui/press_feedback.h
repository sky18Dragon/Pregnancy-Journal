#pragma once

#include "canvas.h"
#include "prototype_ui.h"

// Visible acknowledgement of a tap on e-paper.
//
// A page change costs a ~2 s full refresh during which nothing on the panel
// moves. Before that refresh the app draws the tapped target pressed (small
// target: inverted; large target: 3 px frame) and sends one partial refresh,
// so the user sees the touch was received. In-page taps do not use this: their
// own partial refresh is the feedback.
// 点按反馈：换页的全刷约 2 秒，期间屏幕无变化；先把被点的目标画成按下态并局刷一次。
namespace press_feedback {

using prototype_ui::Rect;

constexpr int kMaxInvertHeight = 80;
constexpr int kFrameStroke = 3;

// Draws the pressed state on top of whatever is already in the canvas.
inline void draw(Canvas &canvas, const Rect &rect) {
  if (rect.height <= kMaxInvertHeight) {
    const int x_end = rect.x + rect.width;
    const int y_end = rect.y + rect.height;
    for (int y = rect.y < 0 ? 0 : rect.y; y < y_end && y < canvas.height(); ++y) {
      for (int x = rect.x < 0 ? 0 : rect.x; x < x_end && x < canvas.width(); ++x) {
        canvas.draw_pixel(x, y,
                          canvas.pixel_at(x, y) >= GrayLevel::LightGray
                              ? GrayLevel::Black
                              : GrayLevel::White);
      }
    }
    return;
  }
  for (int inset = 0; inset < kFrameStroke; ++inset) {
    canvas.draw_rect(rect.x + inset, rect.y + inset, rect.width - 2 * inset,
                     rect.height - 2 * inset, GrayLevel::Black);
  }
}

// Remembers the tapped target between hit-testing and the next render.
class Pending {
public:
  void set(const Rect &rect) { rect_ = rect; pending_ = true; }
  void clear() { pending_ = false; }
  bool take(Rect &rect) {
    const bool had = pending_;
    rect = rect_;
    pending_ = false;
    return had;
  }

private:
  Rect rect_ = {0, 0, 0, 0};
  bool pending_ = false;
};

} // namespace press_feedback
