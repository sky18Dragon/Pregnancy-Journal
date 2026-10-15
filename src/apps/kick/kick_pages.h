#pragma once

#include <cstddef>
#include <cstdint>

#include "kick.h"
#include "kick_comparison.h"
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
                            bool save_failed = false);
void kick_page_render_insights(Canvas &canvas, const KickDaySummary &today,
                               const KickTrendSummary &trend);
void kick_page_render_counter(Canvas &canvas, KickPeriod period,
                              uint16_t count, uint16_t elapsed_minutes,
                              KickMode mode = KickMode::Timed);
void kick_page_render_mode_select(Canvas &canvas);
void kick_page_render_feel(Canvas &canvas);
void kick_page_render_result(Canvas &canvas, const KickSession &session,
                             const KickComparison &comparison,
                             const KickAwarenessSummary &awareness);
void kick_page_render_compare(Canvas &canvas, const KickComparison periods[3],
                              const bool has_period[3], bool has_ten,
                              const KickComparison &ten, bool has_awareness,
                              const KickAwarenessSummary &awareness);

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
