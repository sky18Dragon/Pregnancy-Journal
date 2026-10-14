#pragma once

#include <cstddef>
#include <cstdint>

#include "weight.h"

class WeightService {
public:
  static constexpr size_t kCapacity = 366U;

  bool upsert(WeightRecord record, uint32_t *record_id = nullptr);
  bool remove(uint32_t id);
  void clear();
  bool set_next_id_floor(uint64_t next_id);
  size_t count() const { return count_; }
  uint32_t next_id() const { return next_id_; }
  const WeightRecord *at(size_t index) const;
  WeightRecord *find(uint32_t id);
  const WeightRecord *find(uint32_t id) const;
  const WeightRecord *latest() const;
  const WeightRecord *find_date(const PregnancyDate &date) const;
  const WeightRecord *history_at(size_t position) const;
  bool trend(const PregnancyDate &end_date, uint8_t days, WeightTrend &output) const;
  bool summary(uint16_t height_cm, uint16_t baseline_tenths_kg,
               WeightSummary &output) const;

private:
  WeightRecord records_[kCapacity] = {};
  size_t count_ = 0U;
  uint32_t next_id_ = 1U;
};
