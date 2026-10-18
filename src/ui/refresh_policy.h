#pragma once

#include <cstdint>

// Page-level e-paper refresh rule shared by every app.
//
// A page change replaces most of the panel, so it needs a full monochrome
// refresh; a change inside the page that was just drawn may use the partial
// waveform. The display layer still enforces its own ceiling on consecutive
// partial refreshes, so apps must not keep a second counter.
// 页面级墨水屏刷新规则：换页用全刷，页内小变化用局刷；连续局刷上限仍由显示层兜底。
class PageRefreshTracker {
public:
  // Records `page_id` as the page now on screen and reports whether this
  // refresh must be a full one. `force_full` covers entry, resume, orientation
  // change and other whole-screen replacements decided by the caller.
  bool needs_full(int page_id, bool force_full = false) {
    const bool full = force_full || page_id != last_page_id_;
    last_page_id_ = page_id;
    return full;
  }

  // True when drawing `page_id` next would be a page change (does not record it).
  bool page_changes(int page_id) const { return page_id != last_page_id_; }

  // The page recorded by the last needs_full(); -1 when none is on screen.
  int last_page() const { return last_page_id_; }

  // Forgets the page on screen so the next refresh is a full one.
  void reset() { last_page_id_ = kNoPage; }

private:
  static constexpr int kNoPage = -1;
  int last_page_id_ = kNoPage;
};

// A card read that is still pending is only worth a "loading" page (a screen
// refresh of its own, and a black flash when it is a full one) once it has been
// waiting this long; most reads finish before it and go straight to the content
// page. Entering an app queues the "current app" preference write in the same
// storage queue, which alone adds about 0.7 s, so a first entry takes about 1 s.
// 读卡还在进行时，等待超过这个时间才值得画一页“加载中”（它自己就是一次整屏刷新）；多数读取更快，直接画内容页。
// 进入应用时会在同一个存储队列里写“当前应用”偏好（约 0.7 s），所以首次进入约需 1 s。
constexpr int64_t kLoadingPageDelayUs = 1500000;

inline bool loading_page_due(bool read_pending, int64_t waited_us) {
  return read_pending && waited_us >= kLoadingPageDelayUs;
}
