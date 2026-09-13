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
  assert(summary.sampled_total == 18U);
  assert(summary.estimated_12h == 72U);
  assert(summary.complete);
  assert(summary.change_percent == -40);
  assert(!summary.attention);

  assert(service.remove_day(today) == 3U);
  assert(!service.summarize(today, summary));
  return 0;
}
