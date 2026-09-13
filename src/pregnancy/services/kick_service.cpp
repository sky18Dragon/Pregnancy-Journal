#include "kick_service.h"

#include <cstdlib>

namespace {
bool same_date(const PregnancyDate &a, const PregnancyDate &b) {
  return a.year == b.year && a.month == b.month && a.day == b.day;
}

void add_period(KickDaySummary &output, KickPeriod period, uint16_t value) {
  if (period == KickPeriod::Morning)
    output.morning += value;
  else if (period == KickPeriod::Afternoon)
    output.afternoon += value;
  else
    output.evening += value;
}
} // namespace

bool kick_session_valid(const KickSession &session) {
  return session.schema_version == kKickSessionSchemaVersion &&
         session.id != 0U && pregnancy_date_valid(session.date) &&
         session.hour <= 23U && session.minute <= 59U &&
         session.duration_minutes <= 120U && session.count <= 500U &&
         session.period <= KickPeriod::Evening;
}

const char *kick_period_name(KickPeriod period) {
  switch (period) {
  case KickPeriod::Morning:
    return "MORNING";
  case KickPeriod::Afternoon:
    return "AFTERNOON";
  case KickPeriod::Evening:
    return "EVENING";
  }
  return "MORNING";
}

bool KickService::upsert(KickSession session, uint32_t *session_id) {
  if (session.id != 0U) {
    for (size_t i = 0U; i < count_; ++i) {
      if (sessions_[i].id != session.id)
        continue;
      if (!kick_session_valid(session))
        return false;
      sessions_[i] = session;
      if (session_id != nullptr)
        *session_id = session.id;
      return true;
    }
  }
  if (count_ >= kCapacity)
    return false;
  if (session.id == 0U)
    session.id = next_id_++;
  if (!kick_session_valid(session))
    return false;
  if (session.id >= next_id_)
    next_id_ = session.id + 1U;
  sessions_[count_++] = session;
  if (session_id != nullptr)
    *session_id = session.id;
  return true;
}

bool KickService::remove(uint32_t id) {
  for (size_t i = 0U; i < count_; ++i) {
    if (sessions_[i].id != id)
      continue;
    for (size_t move = i + 1U; move < count_; ++move)
      sessions_[move - 1U] = sessions_[move];
    sessions_[--count_] = {};
    return true;
  }
  return false;
}

size_t KickService::remove_day(const PregnancyDate &date) {
  size_t removed = 0U;
  for (size_t i = 0U; i < count_;) {
    if (same_date(sessions_[i].date, date)) {
      remove(sessions_[i].id);
      ++removed;
    } else
      ++i;
  }
  return removed;
}

void KickService::clear() {
  for (KickSession &session : sessions_)
    session = {};
  count_ = 0U;
  next_id_ = 1U;
}

const KickSession *KickService::at(size_t index) const {
  return index < count_ ? &sessions_[index] : nullptr;
}

bool KickService::summarize(const PregnancyDate &date,
                            KickDaySummary &output) const {
  output = {};
  bool found = false;
  for (size_t i = 0U; i < count_; ++i) {
    if (!same_date(sessions_[i].date, date))
      continue;
    found = true;
    add_period(output, sessions_[i].period, sessions_[i].count);
  }
  output.sampled_total = output.morning + output.afternoon + output.evening;
  output.estimated_12h = output.sampled_total * 4U;
  output.complete =
      output.morning > 0U && output.afternoon > 0U && output.evening > 0U;

  int32_t day = 0;
  PregnancyDate previous_date = {};
  if (pregnancy_date_to_day_index(date, day) &&
      pregnancy_date_from_day_index(day - 1, previous_date)) {
    uint16_t previous_total = 0U;
    uint16_t previous_periods[3] = {};
    for (size_t i = 0U; i < count_; ++i) {
      if (same_date(sessions_[i].date, previous_date)) {
        previous_total += sessions_[i].count;
        previous_periods[static_cast<size_t>(sessions_[i].period)] +=
            sessions_[i].count;
      }
    }
    if (previous_total > 0U) {
      output.change_percent = static_cast<int16_t>(
          (static_cast<int32_t>(output.sampled_total) - previous_total) * 100 /
          previous_total);
    }
    const uint16_t current_periods[] = {
        output.morning,
        output.afternoon,
        output.evening,
    };
    for (size_t period = 0U; period < 3U; ++period) {
      if (previous_periods[period] == 0U)
        continue;
      const int change = (static_cast<int>(current_periods[period]) -
                          static_cast<int>(previous_periods[period])) *
                         100 / previous_periods[period];
      if (std::abs(change) > 50)
        output.attention = true;
    }
  }
  output.attention =
      output.attention || (output.complete && output.estimated_12h < 20U);
  return found;
}
