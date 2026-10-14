#include <cassert>

#include "kick_comparison.h"

namespace {

KickSession timed(PregnancyDate date, KickPeriod period, uint16_t count,
                  uint16_t seconds = 3600U, uint8_t hour = 20U) {
  KickSession value = {};
  value.date = date;
  value.hour = hour;
  value.period = period;
  value.count = count;
  value.mode = KickMode::Timed;
  value.elapsed_seconds = seconds;
  value.duration_minutes = static_cast<uint16_t>((seconds + 59U) / 60U);
  return value;
}

KickSession ten(PregnancyDate date, uint16_t seconds, uint16_t count = 10U) {
  KickSession value = {};
  value.date = date;
  value.hour = 21U;
  value.period = KickPeriod::Evening;
  value.count = count;
  value.mode = KickMode::CountToTen;
  value.elapsed_seconds = seconds;
  value.duration_minutes = static_cast<uint16_t>((seconds + 59U) / 60U);
  return value;
}

KickSession feel(PregnancyDate date, KickFeel value, uint8_t hour = 9U) {
  KickSession session = {};
  session.date = date;
  session.hour = hour;
  session.mode = KickMode::Awareness;
  session.feel = value;
  return session;
}

PregnancyDate day(uint8_t value) { return {2026U, 10U, value}; }

void test_units() {
  uint16_t rate = 0U;
  assert(kick_timed_rate_per_hour(timed(day(1), KickPeriod::Evening, 12U), rate));
  assert(rate == 12U);
  // Thirty minutes with seven movements is fourteen per hour.
  assert(kick_timed_rate_per_hour(
      timed(day(1), KickPeriod::Evening, 7U, 1800U), rate));
  assert(rate == 14U);
  // Under fifteen minutes is not extrapolated to an hourly rate.
  assert(!kick_timed_rate_per_hour(
      timed(day(1), KickPeriod::Evening, 5U, 899U), rate));

  uint16_t seconds = 0U;
  bool capped = false;
  assert(kick_count_to_ten_seconds(ten(day(1), 640U), seconds, capped));
  assert(seconds == 640U && !capped);
  assert(kick_count_to_ten_seconds(ten(day(1), 3600U, 7U), seconds, capped));
  assert(seconds == kKickCountToTenCapSeconds && capped);
  // Ended early without reaching ten: no comparable value.
  assert(!kick_count_to_ten_seconds(ten(day(1), 300U, 4U), seconds, capped));
}

void test_timed_baseline() {
  KickService service;
  // Evening baseline 12, 16, 14 -> median 14. A morning record and one
  // outside the seven-day window must not join it.
  assert(service.upsert(timed(day(5), KickPeriod::Evening, 12U)));
  assert(service.upsert(timed(day(6), KickPeriod::Evening, 16U)));
  KickSession current = timed(day(8), KickPeriod::Evening, 7U);
  KickComparison result = kick_compare_session(service, current);
  assert(result.status == KickComparisonStatus::CollectingBaseline);
  assert(result.baseline_sessions == 2U);

  assert(service.upsert(timed(day(7), KickPeriod::Evening, 14U)));
  assert(service.upsert(timed(day(7), KickPeriod::Morning, 40U, 3600U, 9U)));
  // Eight days before day 8: just outside the seven-day window.
  assert(service.upsert(timed({2026U, 9U, 30U}, KickPeriod::Evening, 90U)));
  // A too-short session is excluded from the baseline as well.
  assert(service.upsert(timed(day(6), KickPeriod::Evening, 1U, 600U)));
  uint32_t current_id = 0U;
  assert(service.upsert(current, &current_id));
  current.id = current_id;

  result = kick_compare_session(service, current);
  assert(result.status == KickComparisonStatus::Ready);
  assert(result.baseline_sessions == 3U && result.baseline == 14U);
  assert(result.current == 7U && result.direction == KickDirection::Less);
  assert(result.difference == 7U && result.percent == 50U && result.percent_known);

  current.count = 21U;
  result = kick_compare_session(service, current);
  assert(result.direction == KickDirection::More && result.difference == 7U);
  assert(result.percent == 50U);
  current.count = 14U;
  result = kick_compare_session(service, current);
  assert(result.direction == KickDirection::Same && result.difference == 0U);

  current.elapsed_seconds = 600U;
  result = kick_compare_session(service, current);
  assert(result.status == KickComparisonStatus::NotComparable);
}

void test_even_median_and_zero_baseline() {
  KickService service;
  assert(service.upsert(timed(day(4), KickPeriod::Afternoon, 0U, 3600U, 14U)));
  assert(service.upsert(timed(day(5), KickPeriod::Afternoon, 0U, 3600U, 14U)));
  assert(service.upsert(timed(day(6), KickPeriod::Afternoon, 0U, 3600U, 14U)));
  KickComparison result = kick_compare_session(
      service, timed(day(7), KickPeriod::Afternoon, 3U, 3600U, 14U));
  assert(result.status == KickComparisonStatus::Ready);
  assert(result.baseline == 0U && !result.percent_known);
  assert(result.direction == KickDirection::More && result.difference == 3U);

  assert(service.upsert(timed(day(6), KickPeriod::Afternoon, 9U, 3600U, 15U)));
  result = kick_compare_session(
      service, timed(day(7), KickPeriod::Afternoon, 3U, 3600U, 14U));
  // Samples 0, 0, 0, 9 -> median rounds (0 + 0) / 2 = 0.
  assert(result.baseline == 0U && result.baseline_sessions == 4U);
}

void test_count_to_ten() {
  KickService service;
  assert(service.upsert(ten(day(3), 600U)));
  assert(service.upsert(ten(day(4), 720U)));
  assert(service.upsert(ten(day(5), 3600U, 6U)));  // Capped sample: 3600.
  assert(service.upsert(ten(day(6), 200U, 3U)));   // Ended early: excluded.
  assert(service.upsert(ten(day(6), 660U)));
  KickSession current = ten(day(7), 1080U);
  uint32_t id = 0U;
  assert(service.upsert(current, &id));
  current.id = id;
  KickComparison result = kick_compare_session(service, current);
  // Samples 600, 720, 3600, 660 -> median (660 + 720) / 2 = 690.
  assert(result.status == KickComparisonStatus::Ready);
  assert(result.baseline_sessions == 4U && result.baseline == 690U);
  assert(result.direction == KickDirection::More && result.difference == 390U);
  assert(result.percent == 57U && !result.current_capped);

  KickComparison latest = {};
  assert(kick_compare_latest_count_to_ten(service, day(7), latest));
  assert(latest.current == 1080U && latest.baseline == 690U);
  assert(!kick_compare_latest_count_to_ten(service, day(20), latest));

  const KickComparison capped =
      kick_compare_session(service, ten(day(7), 3600U, 8U));
  assert(capped.current_capped && capped.current == 3600U);
  assert(capped.direction == KickDirection::More);
}

void test_day_and_awareness() {
  KickService service;
  for (uint8_t value = 1U; value <= 3U; ++value)
    assert(service.upsert(timed(day(value), KickPeriod::Morning, 10U, 3600U, 9U)));
  assert(service.upsert(timed(day(4), KickPeriod::Morning, 6U, 3600U, 8U)));
  assert(service.upsert(timed(day(4), KickPeriod::Morning, 15U, 3600U, 10U)));
  KickComparison day_results[3] = {};
  bool has[3] = {};
  kick_compare_day(service, day(4), day_results, has);
  assert(has[0] && !has[1] && !has[2]);
  // The later morning session (10:00, 15 per hour) is the one compared.
  assert(day_results[0].current == 15U && day_results[0].baseline == 10U);
  assert(day_results[0].direction == KickDirection::More);
  assert(day_results[0].percent == 50U);

  assert(service.upsert(feel(day(4), KickFeel::Usual)));
  assert(service.upsert(feel(day(5), KickFeel::Weaker)));
  assert(service.upsert(feel(day(6), KickFeel::Fewer, 8U)));
  assert(service.upsert(feel(day(6), KickFeel::Changed, 21U)));
  assert(service.upsert(feel({2026U, 9U, 30U}, KickFeel::Weaker)));  // Outside window.
  KickAwarenessSummary summary = {};
  assert(kick_summarize_awareness(service, day(7), summary));
  assert(summary.total == 4U && summary.usual == 1U && summary.weaker == 1U);
  assert(summary.fewer == 1U && summary.changed == 1U);
  assert(summary.has_latest && summary.latest == KickFeel::Changed);
  assert(summary.latest_date.day == 6U);
  assert(!kick_summarize_awareness(service, day(30), summary));

  // Awareness check-ins never count as movements or sessions.
  KickDaySummary totals = {};
  assert(!service.summarize(day(5), totals));
  assert(kick_compare_session(service, feel(day(7), KickFeel::Usual)).status ==
         KickComparisonStatus::Unsupported);
}

}  // namespace

int main() {
  test_units();
  test_timed_baseline();
  test_even_median_and_zero_baseline();
  test_count_to_ten();
  test_day_and_awareness();
  return 0;
}
