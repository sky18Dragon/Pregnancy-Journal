#pragma once

#include <cstddef>

#include "kick.h"

class KickService {
public:
  static constexpr size_t kCapacity = 42U;

  bool upsert(KickSession session, uint32_t *session_id = nullptr);
  bool remove(uint32_t id);
  size_t remove_day(const PregnancyDate &date);
  void clear();
  size_t count() const { return count_; }
  const KickSession *at(size_t index) const;
  bool summarize(const PregnancyDate &date, KickDaySummary &output) const;

private:
  KickSession sessions_[kCapacity] = {};
  size_t count_ = 0U;
  uint32_t next_id_ = 1U;
};
