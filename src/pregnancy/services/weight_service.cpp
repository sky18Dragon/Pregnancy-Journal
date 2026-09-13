#include "weight_service.h"

namespace {
bool same_date(const PregnancyDate &a, const PregnancyDate &b) {
  return a.year == b.year && a.month == b.month && a.day == b.day;
}

bool date_index(const PregnancyDate &date, int32_t &value) {
  return pregnancy_date_to_day_index(date, value);
}
} // namespace

bool weight_record_valid(const WeightRecord &record) {
  return record.schema_version == kWeightRecordSchemaVersion &&
         record.id != 0U && pregnancy_date_valid(record.date) &&
         record.weight_tenths_kg >= 300U && record.weight_tenths_kg <= 2000U;
}

bool WeightService::upsert(WeightRecord record, uint32_t *record_id) {
  for (size_t i = 0U; i < count_; ++i) {
    if (!same_date(records_[i].date, record.date))
      continue;
    record.id = records_[i].id;
    if (!weight_record_valid(record))
      return false;
    records_[i] = record;
    if (record_id != nullptr)
      *record_id = record.id;
    return true;
  }
  if (count_ >= kCapacity)
    return false;
  if (record.id == 0U)
    record.id = next_id_++;
  if (!weight_record_valid(record))
    return false;
  if (record.id >= next_id_)
    next_id_ = record.id + 1U;
  records_[count_++] = record;
  if (record_id != nullptr)
    *record_id = record.id;
  return true;
}

bool WeightService::remove(uint32_t id) {
  for (size_t i = 0U; i < count_; ++i) {
    if (records_[i].id != id)
      continue;
    for (size_t move = i + 1U; move < count_; ++move)
      records_[move - 1U] = records_[move];
    records_[--count_] = {};
    return true;
  }
  return false;
}

void WeightService::clear() {
  for (WeightRecord &record : records_)
    record = {};
  count_ = 0U;
  next_id_ = 1U;
}

const WeightRecord *WeightService::at(size_t index) const {
  return index < count_ ? &records_[index] : nullptr;
}

const WeightRecord *WeightService::latest() const {
  const WeightRecord *result = nullptr;
  int32_t latest_day = 0;
  for (size_t i = 0U; i < count_; ++i) {
    int32_t day = 0;
    if (!date_index(records_[i].date, day) ||
        (result != nullptr && day <= latest_day))
      continue;
    result = &records_[i];
    latest_day = day;
  }
  return result;
}

const WeightRecord *WeightService::find_date(const PregnancyDate &date) const {
  for (size_t i = 0U; i < count_; ++i)
    if (same_date(records_[i].date, date))
      return &records_[i];
  return nullptr;
}

bool WeightService::summary(uint16_t height_cm, uint16_t baseline_tenths_kg,
                            WeightSummary &output) const {
  const WeightRecord *current = latest();
  if (current == nullptr || height_cm < 100U || height_cm > 220U ||
      baseline_tenths_kg < 300U)
    return false;
  output = {};
  output.current_tenths_kg = current->weight_tenths_kg;
  output.change_tenths_kg = static_cast<int16_t>(current->weight_tenths_kg) -
                            static_cast<int16_t>(baseline_tenths_kg);
  const uint32_t height_squared = static_cast<uint32_t>(height_cm) * height_cm;
  output.bmi_tenths = static_cast<uint16_t>(
      (static_cast<uint32_t>(current->weight_tenths_kg) * 10000U +
       height_squared / 2U) /
      height_squared);

  const WeightRecord *prior = nullptr;
  int32_t current_day = 0;
  date_index(current->date, current_day);
  int32_t best_delta = 100000;
  for (size_t i = 0U; i < count_; ++i) {
    int32_t day = 0;
    if (!date_index(records_[i].date, day))
      continue;
    const int32_t delta = current_day - day;
    if (delta >= 5 && delta <= 14 && delta < best_delta) {
      prior = &records_[i];
      best_delta = delta;
    }
  }
  if (prior != nullptr) {
    output.weekly_change_tenths_kg =
        static_cast<int16_t>((static_cast<int32_t>(current->weight_tenths_kg) -
                              static_cast<int32_t>(prior->weight_tenths_kg)) *
                             7 / best_delta);
  }

  const uint32_t baseline_bmi_tenths =
      (static_cast<uint32_t>(baseline_tenths_kg) * 10000U +
       height_squared / 2U) /
      height_squared;
  int16_t maximum_gain_tenths = 160;
  if (baseline_bmi_tenths < 185U)
    maximum_gain_tenths = 180;
  else if (baseline_bmi_tenths >= 250U && baseline_bmi_tenths < 300U)
    maximum_gain_tenths = 115;
  else if (baseline_bmi_tenths >= 300U)
    maximum_gain_tenths = 90;
  output.outside_recommended_gain =
      output.change_tenths_kg > maximum_gain_tenths ||
      output.weekly_change_tenths_kg > 5;
  return true;
}
