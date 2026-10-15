#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>

#include "canvas.h"
#include "kick_pages.h"
#include "ui_language.h"

namespace {

void write_preview(Canvas &canvas, const char *path) {
  std::ofstream output(path, std::ios::binary);
  output << "P6\n" << canvas.width() << " " << canvas.height() << "\n255\n";
  for (int y = 0; y < canvas.height(); ++y) {
    for (int x = 0; x < canvas.width(); ++x) {
      const uint8_t value =
          canvas.pixel_at(x, y) == GrayLevel::Black ? 0U : 255U;
      output.write(reinterpret_cast<const char *>(&value), 1);
      output.write(reinterpret_cast<const char *>(&value), 1);
      output.write(reinterpret_cast<const char *>(&value), 1);
    }
  }
}

void assert_binary_and_visible(Canvas &canvas) {
  size_t black = 0U;
  for (int y = 0; y < canvas.height(); ++y) {
    for (int x = 0; x < canvas.width(); ++x) {
      const GrayLevel level = canvas.pixel_at(x, y);
      assert(level == GrayLevel::Black || level == GrayLevel::White);
      if (level == GrayLevel::Black)
        ++black;
    }
  }
  assert(black > 5000U);
}

KickDaySummary populated_today() {
  KickDaySummary value = {};
  value.morning = 12U;
  value.afternoon = 8U;
  value.evening = 15U;
  value.total = 35U;
  value.session_count = 4U;
  value.duration_minutes = 46U;
  value.latest_count = 9U;
  value.latest_duration_minutes = 12U;
  value.latest_hour = 18U;
  value.latest_minute = 26U;
  value.has_latest = true;
  return value;
}

KickTrendSummary populated_trend() {
  KickTrendSummary value = {};
  for (size_t index = 0U; index < kKickTrendDays; ++index) {
    value.dates[index] = {2026U, 9U,
                          static_cast<uint8_t>(9U + index)};
  }
  const uint16_t totals[kKickTrendDays] = {24U, 30U, 0U, 28U,
                                           35U, 31U, 35U};
  for (size_t index = 0U; index < kKickTrendDays; ++index)
    value.totals[index] = totals[index];
  value.period_totals[0] = 66U;
  value.period_totals[1] = 50U;
  value.period_totals[2] = 67U;
  value.total = 183U;
  value.session_count = 19U;
  value.average_per_recorded_day = 31U;
  value.recorded_days = 6U;
  value.busiest_period = KickPeriod::Evening;
  value.has_data = true;
  return value;
}

KickComparison ready_timed() {
  KickComparison value = {};
  value.status = KickComparisonStatus::Ready;
  value.mode = KickMode::Timed;
  value.period = KickPeriod::Evening;
  value.current = 7U;
  value.baseline = 14U;
  value.difference = 7U;
  value.percent = 50U;
  value.percent_known = true;
  value.direction = KickDirection::Less;
  value.baseline_sessions = 5U;
  return value;
}

KickComparison ready_ten() {
  KickComparison value = {};
  value.status = KickComparisonStatus::Ready;
  value.mode = KickMode::CountToTen;
  value.current = 1080U;
  value.baseline = 690U;
  value.difference = 390U;
  value.percent = 57U;
  value.percent_known = true;
  value.direction = KickDirection::More;
  value.baseline_sessions = 4U;
  return value;
}

void test_formatting() {
  ui_language_set(UiLanguage::ChineseSimplified);
  KickComparisonText text = {};
  kick_format_comparison(ready_timed(), text);
  assert(std::strcmp(text.value, "每小时 7 次") == 0);
  assert(std::strcmp(text.baseline,
                     "过去 7 天同时段中位数：每小时 14 次（5 条记录）") == 0);
  assert(std::strcmp(text.difference, "比平时少 7 次/小时，少 50%") == 0);

  kick_format_comparison(ready_ten(), text);
  assert(std::strcmp(text.value, "数到 10 用时 18分00秒") == 0);
  assert(std::strcmp(text.baseline, "过去 7 天中位数：11分30秒（4 条记录）") == 0);
  assert(std::strcmp(text.difference, "比平时多用 6分30秒，多 57%") == 0);

  KickComparison capped = ready_ten();
  capped.current = 3600U;
  capped.difference = 2910U;
  capped.current_capped = true;
  kick_format_comparison(capped, text);
  assert(std::strcmp(text.value, "60 分钟内未数满 10 次") == 0);
  assert(std::strcmp(text.difference, "比平时至少多用 48分30秒") == 0);

  KickComparison same = ready_timed();
  same.direction = KickDirection::Same;
  same.difference = 0U;
  same.current = 14U;
  kick_format_comparison(same, text);
  assert(std::strcmp(text.difference, "与平时持平") == 0);

  KickComparison zero = ready_timed();
  zero.baseline = 0U;
  zero.percent_known = false;
  zero.direction = KickDirection::More;
  kick_format_comparison(zero, text);
  assert(std::strcmp(text.difference, "比平时多 7 次/小时") == 0);

  KickComparison collecting = ready_timed();
  collecting.status = KickComparisonStatus::CollectingBaseline;
  collecting.baseline_sessions = 2U;
  kick_format_comparison(collecting, text);
  assert(std::strcmp(text.baseline, "基线积累中：过去 7 天已有 2 条，需 3 条") == 0);
  assert(text.difference[0] == '\0');

  KickComparison short_session = ready_timed();
  short_session.status = KickComparisonStatus::NotComparable;
  kick_format_comparison(short_session, text);
  assert(std::strcmp(text.value, "本次不足 15 分钟") == 0);

  char line[160] = {};
  kick_format_comparison_compact(ready_timed(), line, sizeof(line));
  assert(std::strcmp(line, "每小时 7 次 · 比平时少 7 次/小时，少 50%") == 0);
  KickAwarenessSummary awareness = {};
  awareness.usual = 4U;
  awareness.weaker = 1U;
  awareness.fewer = 1U;
  kick_format_awareness(awareness, line, sizeof(line));
  assert(std::strcmp(line, "一样 4 · 变弱 1 · 变少 1 · 规律变了 0") == 0);

  ui_language_set(UiLanguage::English);
  kick_format_comparison(ready_timed(), text);
  assert(std::strcmp(text.difference, "7 PER HOUR FEWER THAN USUAL (-50%)") == 0);
  kick_format_comparison(ready_ten(), text);
  assert(std::strcmp(text.value, "TEN IN 18:00") == 0);
  assert(std::strcmp(text.difference, "6:30 LONGER THAN USUAL (+57%)") == 0);
  ui_language_set(UiLanguage::ChineseSimplified);
}

void render_new_pages(Canvas &canvas, const char *suffix) {
  const int width = canvas.width();
  const int height = canvas.height();
  const bool portrait = height > width;
  char path[96] = {};

  kick_page_render_mode_select(canvas);
  assert_binary_and_visible(canvas);
  std::snprintf(path, sizeof(path), "/tmp/kick_modes_%s_zh.ppm", suffix);
  write_preview(canvas, path);
  assert(kick_page_action_at(KickPage::ModeSelect, width, height, 40,
                             portrait ? 130 : 110) == KickAction::StartTimed);
  assert(kick_page_action_at(KickPage::ModeSelect, width, height, 40,
                             portrait ? 300 : 200) ==
         KickAction::StartCountToTen);
  assert(kick_page_action_at(KickPage::ModeSelect, width, height, 40,
                             portrait ? 480 : 300) ==
         KickAction::StartAwareness);
  assert(kick_page_action_at(KickPage::ModeSelect, width, height, 40,
                             portrait ? 700 : 440) == KickAction::None);

  kick_page_render_feel(canvas);
  assert_binary_and_visible(canvas);
  std::snprintf(path, sizeof(path), "/tmp/kick_feel_%s_zh.ppm", suffix);
  write_preview(canvas, path);
  assert(kick_page_action_at(KickPage::Feel, width, height, 40,
                             portrait ? 130 : 120) == KickAction::FeelUsual);
  assert(kick_page_action_at(KickPage::Feel, width, height,
                             portrait ? 40 : 420, portrait ? 280 : 120) ==
         KickAction::FeelWeaker);
  assert(kick_page_action_at(KickPage::Feel, width, height, 40,
                             portrait ? 420 : 270) == KickAction::FeelFewer);
  assert(kick_page_action_at(KickPage::Feel, width, height,
                             portrait ? 40 : 420, portrait ? 570 : 270) ==
         KickAction::FeelChanged);

  KickSession session = {};
  session.hour = 20U;
  session.minute = 5U;
  session.count = 7U;
  session.elapsed_seconds = 3600U;
  session.mode = KickMode::Timed;
  KickAwarenessSummary awareness = {};
  awareness.usual = 4U;
  awareness.weaker = 1U;
  kick_page_render_result(canvas, session, ready_timed(), awareness);
  assert_binary_and_visible(canvas);
  std::snprintf(path, sizeof(path), "/tmp/kick_result_%s_zh.ppm", suffix);
  write_preview(canvas, path);
  assert(kick_page_action_at(KickPage::Result, width, height, 30,
                             portrait ? 730 : 410) == KickAction::Done);
  assert(kick_page_action_at(KickPage::Result, width, height, width - 30,
                             portrait ? 730 : 410) == KickAction::ShowCompare);

  KickSession feel = {};
  feel.mode = KickMode::Awareness;
  feel.feel = KickFeel::Weaker;
  kick_page_render_result(canvas, feel, {}, awareness);
  assert_binary_and_visible(canvas);

  const KickComparison periods[3] = {ready_timed(), {}, ready_timed()};
  KickComparison collecting = ready_timed();
  collecting.status = KickComparisonStatus::CollectingBaseline;
  collecting.baseline_sessions = 1U;
  const KickComparison mixed[3] = {periods[0], collecting, periods[2]};
  const bool has[3] = {true, true, false};
  kick_page_render_compare(canvas, mixed, has, true, ready_ten(), true,
                           awareness);
  assert_binary_and_visible(canvas);
  std::snprintf(path, sizeof(path), "/tmp/kick_compare_%s_zh.ppm", suffix);
  write_preview(canvas, path);
  assert(kick_page_action_at(KickPage::Compare, width, height, 30,
                             portrait ? 730 : 410) == KickAction::Start);
  assert(kick_page_action_at(KickPage::Compare, width, height, width - 30,
                             portrait ? 30 : 30) == KickAction::Back);

  kick_page_render_counter(canvas, KickPeriod::Evening, 7U, 5U,
                           KickMode::CountToTen);
  assert_binary_and_visible(canvas);
  std::snprintf(path, sizeof(path), "/tmp/kick_counter_ten_%s_zh.ppm", suffix);
  write_preview(canvas, path);
}

} // namespace

// Geometry contract: every target is on screen, at least 48 px each way, does
// not overlap another target's rectangle, and the hit test returns the target
// under its centre. The counter circle is hit-tested as a circle.
void test_target_contract() {
  const KickPage pages[] = {KickPage::Today,      KickPage::Insights, KickPage::Counter,
                            KickPage::ModeSelect, KickPage::Feel,     KickPage::Result,
                            KickPage::Compare};
  const int sizes[2][2] = {{800, 480}, {480, 800}};
  for (const auto &size : sizes) {
    for (KickPage page : pages) {
      KickTarget targets[16];
      const size_t count = kick_page_targets(page, size[0], size[1], targets, 16U);
      assert(count >= 2U);
      for (size_t a = 0U; a < count; ++a) {
        const auto &r = targets[a].rect;
        assert(r.x >= 0 && r.y >= 0 && r.x + r.width <= size[0] && r.y + r.height <= size[1]);
        assert(r.height >= 48 && r.width >= 48);
        assert(kick_page_action_at(page, size[0], size[1], r.x + r.width / 2,
                                   r.y + r.height / 2) == targets[a].action);
        if (targets[a].circle) {
          // The bounding square's corner is outside the circle: no hit.
          assert(kick_page_action_at(page, size[0], size[1], r.x, r.y) == KickAction::None);
        }
        for (size_t b = a + 1U; b < count; ++b) {
          const auto &o = targets[b].rect;
          assert(!(r.x < o.x + o.width && o.x < r.x + r.width && r.y < o.y + o.height &&
                   o.y < r.y + r.height));
        }
      }
    }
  }
}

int main() {
  test_target_contract();
  test_formatting();
  std::vector<uint8_t> framebuffer(800U * 480U / 4U, 0xFFU);
  Canvas canvas(800U, 480U, framebuffer.data(), framebuffer.size());
  ui_language_set(UiLanguage::ChineseSimplified);
  const KickDaySummary today = populated_today();
  const KickTrendSummary trend = populated_trend();

  kick_page_render_today(canvas, today);
  assert_binary_and_visible(canvas);
  write_preview(canvas, "/tmp/kick_today_landscape_zh.ppm");
  assert(kick_page_action_at(KickPage::Today, 800, 480, 24, 400) ==
         KickAction::Start);
  assert(kick_page_action_at(KickPage::Today, 800, 480, 391, 455) ==
         KickAction::Start);
  assert(kick_page_action_at(KickPage::Today, 800, 480, 408, 400) ==
         KickAction::ShowInsights);
  assert(kick_page_action_at(KickPage::Today, 800, 480, 775, 455) ==
         KickAction::ShowInsights);
  assert(kick_page_action_at(KickPage::Today, 800, 480, 400, 430) ==
         KickAction::None);

  kick_page_render_insights(canvas, today, trend);
  assert_binary_and_visible(canvas);
  write_preview(canvas, "/tmp/kick_insights_landscape_zh.ppm");
  assert(kick_page_action_at(KickPage::Insights, 800, 480, 24, 400) ==
         KickAction::Start);
  assert(kick_page_action_at(KickPage::Insights, 800, 480, 775, 455) ==
         KickAction::ShowCompare);

  kick_page_render_counter(canvas, KickPeriod::Afternoon, 12U, 8U);
  assert_binary_and_visible(canvas);
  write_preview(canvas, "/tmp/kick_counter_landscape_zh.ppm");
  assert(kick_page_action_at(KickPage::Counter, 800, 480, 240, 234) ==
         KickAction::Count);
  assert(kick_page_action_at(KickPage::Counter, 800, 480, 72, 104) ==
         KickAction::None);
  assert(kick_page_action_at(KickPage::Counter, 800, 480, 24, 400) ==
         KickAction::Undo);
  assert(kick_page_action_at(KickPage::Counter, 800, 480, 775, 455) ==
         KickAction::Finish);
  assert(kick_page_action_at(KickPage::Counter, 800, 480, 775, 63) ==
         KickAction::Back);

  canvas.set_rotation(CanvasRotation::Deg90Clockwise);
  kick_page_render_today(canvas, today);
  assert_binary_and_visible(canvas);
  write_preview(canvas, "/tmp/kick_today_portrait_zh.ppm");
  assert(kick_page_action_at(KickPage::Today, 480, 800, 24, 720) ==
         KickAction::Start);
  assert(kick_page_action_at(KickPage::Today, 480, 800, 455, 775) ==
         KickAction::ShowInsights);

  kick_page_render_insights(canvas, today, trend);
  assert_binary_and_visible(canvas);
  write_preview(canvas, "/tmp/kick_insights_portrait_zh.ppm");
  assert(kick_page_action_at(KickPage::Insights, 480, 800, 24, 720) ==
         KickAction::Start);
  assert(kick_page_action_at(KickPage::Insights, 480, 800, 455, 775) ==
         KickAction::ShowCompare);

  kick_page_render_counter(canvas, KickPeriod::Evening, 7U, 3U);
  assert_binary_and_visible(canvas);
  write_preview(canvas, "/tmp/kick_counter_portrait_zh.ppm");
  assert(kick_page_action_at(KickPage::Counter, 480, 800, 240, 332) ==
         KickAction::Count);
  assert(kick_page_action_at(KickPage::Counter, 480, 800, 84, 176) ==
         KickAction::None);
  assert(kick_page_action_at(KickPage::Counter, 480, 800, 24, 720) ==
         KickAction::Undo);
  assert(kick_page_action_at(KickPage::Counter, 480, 800, 455, 775) ==
         KickAction::Finish);

  render_new_pages(canvas, "portrait");
  canvas.set_rotation(CanvasRotation::Deg0);
  render_new_pages(canvas, "landscape");
  canvas.set_rotation(CanvasRotation::Deg90Clockwise);

  KickTrendSummary empty = {};
  kick_page_render_insights(canvas, {}, empty);
  assert_binary_and_visible(canvas);
  return 0;
}
