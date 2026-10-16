#pragma once

#include <cstddef>
#include <cstdint>

#include "kick.h"
#include "kick_comparison.h"
#include "key_policy.h"
#include "prototype_ui.h"

class Canvas;

enum class KickPage : uint8_t {
  Today,
  Insights,
  Counter,
  ModeSelect,
  Feel,
  Result,
  Compare,
};

// How the hardware keys act on `page`. On the counter the top key records one
// movement, exactly like tapping the circle; the other keys are ignored so a
// stray press cannot leave a running session.
// 各页对硬件按键的反应；计数页的顶键等同点计数圆，其余键忽略，避免误触离开进行中的记录。
inline KeyPolicy kick_page_key_policy(KickPage page) {
  return page == KickPage::Counter ? KeyPolicy::Capture : KeyPolicy::Free;
}

enum class KickAction : uint8_t {
  None,
  Back,
  Start,
  ShowInsights,
  Count,
  Undo,
  Finish,
  StartTimed,
  StartCountToTen,
  StartAwareness,
  FeelUsual,
  FeelWeaker,
  FeelFewer,
  FeelChanged,
  ShowCompare,
  Done,
  ChangeMethod, // Today: pick the record method again
};

// The record method the user picked last; unset until they choose one.
// 用户上次选择的记录方式；未选择过时 set 为 false。
struct KickMethod {
  bool set = false;
  KickMode mode = KickMode::Timed;
};

// Descriptive comparison text; never contains advice or labels.
struct KickComparisonText {
  char value[64] = {};
  char baseline[96] = {};
  char difference[96] = {};
};

void kick_format_duration(uint16_t seconds, char *output, size_t size);
void kick_format_comparison(const KickComparison &comparison,
                            KickComparisonText &output);
// One-line form used by the comparison overview rows.
void kick_format_comparison_compact(const KickComparison &comparison,
                                    char *output, size_t size);
void kick_format_awareness(const KickAwarenessSummary &summary, char *output,
                           size_t size);
const char *kick_feel_label(KickFeel feel);
const char *kick_mode_label(KickMode mode);

void kick_page_render_today(Canvas &canvas, const KickDaySummary &today,
                            bool save_failed = false, KickMethod method = {});
void kick_page_render_insights(Canvas &canvas, const KickDaySummary &today,
                               const KickTrendSummary &trend);
void kick_page_render_counter(Canvas &canvas, KickPeriod period,
                              uint16_t count, uint16_t elapsed_minutes,
                              KickMode mode = KickMode::Timed);
// `method` (when set) is drawn inverted as the current choice.
void kick_page_render_mode_select(Canvas &canvas, KickMethod method = {});
void kick_page_render_feel(Canvas &canvas);
void kick_page_render_result(Canvas &canvas, const KickSession &session,
                             const KickComparison &comparison,
                             const KickAwarenessSummary &awareness);
void kick_page_render_compare(Canvas &canvas, const KickComparison periods[3],
                              const bool has_period[3], bool has_ten,
                              const KickComparison &ten, bool has_awareness,
                              const KickAwarenessSummary &awareness);

// Where the header action (BACK / CANCEL) of `page` goes inside the app.
// Returns false when it leaves the app for the launcher (the root page).
// `compare_origin` is the page the Compare page was opened from (Insights or
// Result). Every page reaches the root in a few steps.
// 页头返回键在应用内的去向；根页面返回启动器时返回 false。
bool kick_page_back_target(KickPage page, KickPage compare_origin,
                           KickPage &target);

// One tappable region. `rect` is the region itself, or the bounding square of
// the counter circle when `circle` is set (hit-tested as a circle).
struct KickTarget {
  prototype_ui::Rect rect;
  KickAction action;
  bool circle = false;

  bool contains(int x, int y) const {
    if (!circle) return rect.contains(x, y);
    const int radius = rect.width / 2;
    const int dx = x - (rect.x + radius);
    const int dy = y - (rect.y + radius);
    return dx * dx + dy * dy <= radius * radius;
  }
};

// Lists the page's hit targets in hit-test order (at most `capacity` written;
// returns the count written). The hit test is the first target containing the
// point, and the renderers draw the same rectangles.
size_t kick_page_targets(KickPage page, int width, int height, KickTarget *out,
                         size_t capacity);

KickAction kick_page_action_at(KickPage page, int width, int height, int x,
                               int y);
