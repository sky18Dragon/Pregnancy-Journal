#pragma once

#include <cstddef>

#include "kick.h"

class KickService {
public:
  // Roughly 14 weeks (28-42) of up to four records per day; the comparison
  // baseline only needs the last seven days.
  static constexpr size_t kCapacity = 400U;

  bool upsert(KickSession session, uint32_t *session_id = nullptr);
  bool remove(uint32_t id);
  size_t remove_day(const PregnancyDate &date);
  void clear();
  bool set_next_id_floor(uint64_t next_id);
  uint32_t next_id() const { return next_id_; }
  const KickSession *find(uint32_t id) const;
  size_t count() const { return count_; }
  const KickSession *at(size_t index) const;
  bool summarize(const PregnancyDate &date, KickDaySummary &output) const;
  bool summarize_recent(const PregnancyDate &end_date,
                        KickTrendSummary &output) const;

private:
  KickSession sessions_[kCapacity] = {};
  size_t count_ = 0U;
  uint32_t next_id_ = 1U;
};
