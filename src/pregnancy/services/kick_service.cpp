#include "kick_service.h"

#include <cstring>

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
  if (session.schema_version != kKickSessionSchemaVersion ||
      session.id == 0U || !pregnancy_date_valid(session.date) ||
      session.hour > 23U || session.minute > 59U ||
      session.duration_minutes > 120U || session.count > 500U ||
      session.period > KickPeriod::Evening ||
      session.mode > KickMode::Awareness ||
      session.feel > KickFeel::Changed ||
      session.elapsed_seconds > kKickMaximumElapsedSeconds ||
      std::memchr(session.note, '\0', sizeof(session.note)) == nullptr)
    return false;
  // Awareness check-ins carry a feeling and no count; counted sessions carry
  // no feeling. Count-to-ten stops at its target.
  if (session.mode == KickMode::Awareness)
    return session.count == 0U && session.feel != KickFeel::None;
  if (session.feel != KickFeel::None)
    return false;
  return session.mode != KickMode::CountToTen ||
         session.count <= kKickCountToTenTarget;
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
  if (session.id == 0U) {
    if (next_id_ == 0U) return false;
    session.id = next_id_;
  }
  if (!kick_session_valid(session))
    return false;
  if (next_id_ != 0U && session.id >= next_id_)
    next_id_ = session.id == UINT32_MAX ? 0U : session.id + 1U;
  sessions_[count_++] = session;
  if (session_id != nullptr)
    *session_id = session.id;
  return true;
}

bool KickService::set_next_id_floor(uint64_t next_id) {
  if (next_id == 0U || next_id > static_cast<uint64_t>(UINT32_MAX) + 1U)
    return false;
  if (next_id_ != 0U && next_id > next_id_)
    next_id_ = next_id > UINT32_MAX ? 0U : static_cast<uint32_t>(next_id);
  return true;
}

const KickSession *KickService::find(uint32_t id) const {
  for (size_t i = 0U; i < count_; ++i)
    if (sessions_[i].id == id) return &sessions_[i];
  return nullptr;
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
  int latest_minute_of_day = -1;
  for (size_t i = 0U; i < count_; ++i) {
    // Awareness check-ins have no count and stay out of movement totals.
    if (!same_date(sessions_[i].date, date) ||
        sessions_[i].mode == KickMode::Awareness)
      continue;
    found = true;
    add_period(output, sessions_[i].period, sessions_[i].count);
    output.total += sessions_[i].count;
    ++output.session_count;
    output.duration_minutes += sessions_[i].duration_minutes;
    const int minute_of_day = static_cast<int>(sessions_[i].hour) * 60 +
                              static_cast<int>(sessions_[i].minute);
    if (minute_of_day >= latest_minute_of_day) {
      latest_minute_of_day = minute_of_day;
      output.latest_hour = sessions_[i].hour;
      output.latest_minute = sessions_[i].minute;
      output.latest_count = sessions_[i].count;
      output.latest_duration_minutes = sessions_[i].duration_minutes;
      output.has_latest = true;
    }
  }
  return found;
}

bool KickService::summarize_recent(const PregnancyDate &end_date,
                                   KickTrendSummary &output) const {
  output = {};
  int32_t end_day = 0;
  if (!pregnancy_date_to_day_index(end_date, end_day))
    return false;

  for (size_t index = 0U; index < kKickTrendDays; ++index) {
    const int32_t offset = static_cast<int32_t>(kKickTrendDays - 1U - index);
    if (!pregnancy_date_from_day_index(end_day - offset,
                                       output.dates[index]))
      continue;
    KickDaySummary day = {};
    if (!summarize(output.dates[index], day))
      continue;
    output.has_data = true;
    output.totals[index] = day.total;
    output.session_counts[index] = day.session_count;
    output.duration_minutes[index] = day.duration_minutes;
    output.total += day.total;
    output.session_count += day.session_count;
    ++output.recorded_days;
    output.period_totals[0] += day.morning;
    output.period_totals[1] += day.afternoon;
    output.period_totals[2] += day.evening;
  }

  if (output.recorded_days > 0U) {
    output.average_per_recorded_day = static_cast<uint16_t>(
        (output.total + output.recorded_days / 2U) / output.recorded_days);
  }
  size_t busiest = 0U;
  for (size_t index = 1U; index < 3U; ++index) {
    if (output.period_totals[index] > output.period_totals[busiest])
      busiest = index;
  }
  output.busiest_period = static_cast<KickPeriod>(busiest);
  return output.has_data;
}
