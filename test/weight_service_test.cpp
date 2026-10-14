#include <cassert>

#include "weight_service.h"

int main() {
  WeightService service;
  WeightRecord first = {};
  first.date = {2026U, 9U, 1U};
  first.weight_tenths_kg = 600U;
  assert(service.upsert(first));
  WeightRecord current = {};
  current.date = {2026U, 9U, 8U};
  current.weight_tenths_kg = 605U;
  assert(service.upsert(current));
  assert(service.count() == 2U);

  WeightSummary summary = {};
  assert(service.summary(165U, 600U, summary));
  assert(summary.current_tenths_kg == 605U);
  assert(summary.change_tenths_kg == 5);
  assert(summary.bmi_tenths == 222U);
  assert(summary.weekly_change_tenths_kg == 5);


  current.weight_tenths_kg = 610U;
  assert(service.upsert(current));
  assert(service.count() == 2U);
  assert(service.latest()->weight_tenths_kg == 610U);

  WeightRecord invalid = {};
  invalid.date = {2026U, 9U, 9U};
  invalid.weight_tenths_kg = 250U;
  assert(!service.upsert(invalid));
  assert(service.set_next_id_floor(42U));
  WeightRecord later = {};
  later.date = {2026U, 9U, 10U};
  later.weight_tenths_kg = 611U;
  uint32_t later_id = 0U;
  assert(service.upsert(later, &later_id));
  assert(later_id == 42U);
  WeightService annual;
  int32_t start = 0;
  assert(pregnancy_date_to_day_index({2024U, 1U, 1U}, start));
  for (size_t day = 0U; day < WeightService::kCapacity; ++day) {
    WeightRecord record = {};
    assert(pregnancy_date_from_day_index(start + static_cast<int32_t>(day), record.date));
    record.weight_tenths_kg = 600U + day % 50U;
    assert(annual.upsert(record));
  }
  assert(annual.count() == 366U);
  assert(annual.history_at(0U)->date.month == 12U);
  assert(annual.history_at(365U)->date.day == 1U);
  WeightTrend trend = {};
  assert(annual.trend({2024U, 3U, 1U}, 7U, trend));
  assert(trend.count == 7U && trend.dates[5].month == 2U && trend.dates[5].day == 29U);
  const auto *february = annual.find_date({2024U, 2U, 29U});
  assert(february && annual.remove(february->id));
  assert(annual.trend({2024U, 3U, 1U}, 7U, trend));
  assert(trend.count == 6U && !trend.present[5] && trend.values[5] == 0U);
  assert(!annual.trend({2024U, 3U, 1U}, 8U, trend));
  WeightRecord extra = {};
  extra.date = {2025U, 1U, 1U}; extra.weight_tenths_kg = 1000U;
  assert(annual.upsert(extra));
  extra.date = {2025U, 1U, 2U};
  assert(!annual.upsert(extra));
  extra = *annual.latest(); extra.weight_tenths_kg = 1100U;
  assert(annual.upsert(extra));  // Same-day replacement works at full capacity.
  assert(annual.latest()->weight_tenths_kg == 1100U);
  assert(annual.summary(165U, 600U, summary));  // Numerical change, no medical threshold.
  assert(summary.change_tenths_kg == 500);
  return 0;
}
