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
  assert(!summary.outside_recommended_gain);

  current.weight_tenths_kg = 610U;
  assert(service.upsert(current));
  assert(service.count() == 2U);
  assert(service.latest()->weight_tenths_kg == 610U);

  WeightRecord invalid = {};
  invalid.date = {2026U, 9U, 9U};
  invalid.weight_tenths_kg = 250U;
  assert(!service.upsert(invalid));
  return 0;
}
