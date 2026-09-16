#include <cassert>

#include "kick_service.h"

namespace {
KickSession session(PregnancyDate date, KickPeriod period, uint16_t count) {
  KickSession value = {};
  value.date = date;
  value.hour = period == KickPeriod::Morning     ? 8U
               : period == KickPeriod::Afternoon ? 14U
                                                 : 20U;
  value.duration_minutes = 60U;
  value.count = count;
  value.period = period;
  return value;
}
} // namespace

int main() {
  KickService service;
  const PregnancyDate yesterday = {2026U, 9U, 12U};
  const PregnancyDate today = {2026U, 9U, 13U};
  assert(service.upsert(session(yesterday, KickPeriod::Morning, 10U)));
  assert(service.upsert(session(yesterday, KickPeriod::Afternoon, 10U)));
  assert(service.upsert(session(yesterday, KickPeriod::Evening, 10U)));
  assert(service.upsert(session(today, KickPeriod::Morning, 5U)));
  assert(service.upsert(session(today, KickPeriod::Afternoon, 6U)));
  assert(service.upsert(session(today, KickPeriod::Evening, 7U)));

  KickDaySummary summary = {};
  assert(service.summarize(today, summary));
  assert(summary.total == 18U);
  assert(summary.session_count == 3U);
  assert(summary.duration_minutes == 180U);
  assert(summary.morning == 5U);
  assert(summary.afternoon == 6U);
  assert(summary.evening == 7U);
  assert(summary.has_latest);
  assert(summary.latest_hour == 20U);
  assert(summary.latest_count == 7U);

  KickTrendSummary trend = {};
  assert(service.summarize_recent(today, trend));
  assert(trend.has_data);
  assert(trend.recorded_days == 2U);
  assert(trend.session_count == 6U);
  assert(trend.total == 48U);
  assert(trend.average_per_recorded_day == 24U);
  assert(trend.totals[5] == 30U);
  assert(trend.totals[6] == 18U);
  assert(trend.busiest_period == KickPeriod::Evening);

  assert(service.remove_day(today) == 3U);
  assert(!service.summarize(today, summary));
  assert(service.summarize_recent(today, trend));
  assert(trend.recorded_days == 1U);
  assert(trend.total == 30U);
  assert(trend.totals[6] == 0U);
  return 0;
}
