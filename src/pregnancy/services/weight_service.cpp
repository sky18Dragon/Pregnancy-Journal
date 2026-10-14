#include "weight_service.h"

#include <cstring>

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
         record.weight_tenths_kg >= 300U && record.weight_tenths_kg <= 2000U &&
         std::memchr(record.note, '\0', sizeof(record.note)) != nullptr;
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
  if (record.id == 0U) {
    if (next_id_ == 0U)
      return false;
    record.id = next_id_;
  }
  if (!weight_record_valid(record))
    return false;
  if (next_id_ != 0U && record.id >= next_id_)
    next_id_ = record.id == UINT32_MAX ? 0U : record.id + 1U;
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

bool WeightService::set_next_id_floor(uint64_t next_id) {
  if (next_id == 0U || next_id > static_cast<uint64_t>(UINT32_MAX) + 1U)
    return false;
  if (next_id > UINT32_MAX) {
    next_id_ = 0U;
  } else if (next_id_ != 0U && next_id_ < next_id) {
    next_id_ = static_cast<uint32_t>(next_id);
  }
  return true;
}

const WeightRecord *WeightService::at(size_t index) const {
  return index < count_ ? &records_[index] : nullptr;
}

WeightRecord *WeightService::find(uint32_t id) {
  for (size_t index = 0U; index < count_; ++index)
    if (records_[index].id == id)
      return &records_[index];
  return nullptr;
}

const WeightRecord *WeightService::find(uint32_t id) const {
  for (size_t index = 0U; index < count_; ++index)
    if (records_[index].id == id)
      return &records_[index];
  return nullptr;
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

const WeightRecord *WeightService::history_at(size_t position) const {
  if (position >= count_) return nullptr;
  // Bounded rank selection, independent of repository insertion order.
  for (size_t candidate = 0U; candidate < count_; ++candidate) {
    int32_t candidate_day = 0;
    date_index(records_[candidate].date, candidate_day);
    size_t newer = 0U;
    for (size_t other = 0U; other < count_; ++other) {
      int32_t day = 0;
      date_index(records_[other].date, day);
      if (day > candidate_day) ++newer;
    }
    if (newer == position) return &records_[candidate];
  }
  return nullptr;
}

bool WeightService::trend(const PregnancyDate &end_date, uint8_t days, WeightTrend &output) const {
  output = {};
  int32_t end = 0;
  if ((days != 7U && days != 30U) || !date_index(end_date, end)) return false;
  output.days = days;
  for (uint8_t index = 0U; index < days; ++index) {
    if (!pregnancy_date_from_day_index(end - (days - 1U - index), output.dates[index])) continue;
    const auto *record = find_date(output.dates[index]);
    if (record == nullptr) continue;
    output.present[index] = true;
    output.values[index] = record->weight_tenths_kg;
    ++output.count;
  }
  return true;
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

  return true;
}
