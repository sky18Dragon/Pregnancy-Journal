#include <cassert>
#include <cstdint>
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

} // namespace

int main() {
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
         KickAction::Start);

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
         KickAction::Start);

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

  KickTrendSummary empty = {};
  kick_page_render_insights(canvas, {}, empty);
  assert_binary_and_visible(canvas);
  return 0;
}
