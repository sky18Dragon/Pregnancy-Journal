#pragma once

#include <cstddef>
#include <cstdint>

#include "weight.h"

class WeightService {
public:
  static constexpr size_t kCapacity = 32U;

  bool upsert(WeightRecord record, uint32_t *record_id = nullptr);
  bool remove(uint32_t id);
  void clear();
  size_t count() const { return count_; }
  const WeightRecord *at(size_t index) const;
  const WeightRecord *latest() const;
  const WeightRecord *find_date(const PregnancyDate &date) const;
  bool summary(uint16_t height_cm, uint16_t baseline_tenths_kg,
               WeightSummary &output) const;

private:
  WeightRecord records_[kCapacity] = {};
  size_t count_ = 0U;
  uint32_t next_id_ = 1U;
};
