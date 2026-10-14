#include "kick_comparison.h"

namespace {

bool day_index(const PregnancyDate &date, int32_t &index) {
  return pregnancy_date_to_day_index(date, index);
}

// Minute-of-day plus id gives a stable "later than" order within one day.
bool later(const KickSession &left, const KickSession &right) {
  const int left_minute = left.hour * 60 + left.minute;
  const int right_minute = right.hour * 60 + right.minute;
  if (left_minute != right_minute) return left_minute > right_minute;
  return left.id > right.id;
}

uint16_t median(uint16_t *values, size_t count) {
  for (size_t i = 1U; i < count; ++i) {
    const uint16_t value = values[i];
    size_t position = i;
    while (position > 0U && values[position - 1U] > value) {
      values[position] = values[position - 1U];
      --position;
    }
    values[position] = value;
  }
  if (count % 2U == 1U) return values[count / 2U];
  const uint32_t sum = static_cast<uint32_t>(values[count / 2U - 1U]) +
                       values[count / 2U];
  return static_cast<uint16_t>((sum + 1U) / 2U);
}

// Numeric value of a session in its comparison unit.
bool session_value(const KickSession &session, uint16_t &value, bool &capped) {
  capped = false;
  if (session.mode == KickMode::Timed)
    return kick_timed_rate_per_hour(session, value);
  if (session.mode == KickMode::CountToTen)
    return kick_count_to_ten_seconds(session, value, capped);
  return false;
}

bool same_group(const KickSession &candidate, const KickSession &session) {
  if (candidate.mode != session.mode) return false;
  // Timed hourly rates differ through the day, so compare like periods.
  return session.mode != KickMode::Timed || candidate.period == session.period;
}

}  // namespace

bool kick_timed_rate_per_hour(const KickSession &session, uint16_t &rate) {
  rate = 0U;
  if (session.mode != KickMode::Timed ||
      session.elapsed_seconds < kKickMinimumTimedSeconds)
    return false;
  const uint32_t scaled = static_cast<uint32_t>(session.count) * 3600U +
                          session.elapsed_seconds / 2U;
  rate = static_cast<uint16_t>(scaled / session.elapsed_seconds);
  return true;
}

bool kick_count_to_ten_seconds(const KickSession &session, uint16_t &seconds,
                               bool &capped) {
  seconds = 0U;
  capped = false;
  if (session.mode != KickMode::CountToTen) return false;
  if (session.count >= kKickCountToTenTarget) {
    seconds = session.elapsed_seconds;
    return true;
  }
  if (session.elapsed_seconds >= kKickCountToTenCapSeconds) {
    seconds = kKickCountToTenCapSeconds;
    capped = true;
    return true;
  }
  return false;
}

KickComparison kick_compare_session(const KickService &service,
                                    const KickSession &session) {
  KickComparison result = {};
  result.mode = session.mode;
  result.period = session.period;
  if (session.mode == KickMode::Awareness) return result;

  int32_t session_day = 0;
  if (!day_index(session.date, session_day) ||
      !session_value(session, result.current, result.current_capped)) {
    result.status = KickComparisonStatus::NotComparable;
    return result;
  }

  uint16_t samples[kKickBaselineSampleLimit] = {};
  size_t sample_count = 0U;
  for (size_t index = 0U; index < service.count(); ++index) {
    const KickSession *candidate = service.at(index);
    int32_t candidate_day = 0;
    if (candidate == nullptr || candidate->id == session.id ||
        !same_group(*candidate, session) ||
        !day_index(candidate->date, candidate_day) ||
        candidate_day >= session_day ||
        candidate_day < session_day - kKickBaselineDays)
      continue;
    uint16_t value = 0U;
    bool capped = false;
    if (!session_value(*candidate, value, capped)) continue;
    if (sample_count < kKickBaselineSampleLimit) samples[sample_count++] = value;
  }

  result.baseline_sessions = static_cast<uint8_t>(sample_count);
  if (sample_count < kKickBaselineMinimumSessions) {
    result.status = KickComparisonStatus::CollectingBaseline;
    return result;
  }
  result.status = KickComparisonStatus::Ready;
  result.baseline = median(samples, sample_count);
  if (result.current < result.baseline) {
    result.direction = KickDirection::Less;
    result.difference = static_cast<uint16_t>(result.baseline - result.current);
  } else if (result.current > result.baseline) {
    result.direction = KickDirection::More;
    result.difference = static_cast<uint16_t>(result.current - result.baseline);
  }
  if (result.baseline != 0U) {
    result.percent_known = true;
    const uint32_t scaled = static_cast<uint32_t>(result.difference) * 100U +
                            result.baseline / 2U;
    const uint32_t percent = scaled / result.baseline;
    result.percent = static_cast<uint16_t>(percent > UINT16_MAX ? UINT16_MAX
                                                                : percent);
  }
  return result;
}

void kick_compare_day(const KickService &service, const PregnancyDate &date,
                      KickComparison output[3], bool has[3]) {
  const KickSession *latest[3] = {};
  for (size_t index = 0U; index < service.count(); ++index) {
    const KickSession *session = service.at(index);
    if (session == nullptr || session->mode != KickMode::Timed ||
        session->date.year != date.year || session->date.month != date.month ||
        session->date.day != date.day)
      continue;
    const size_t period = static_cast<size_t>(session->period);
    if (latest[period] == nullptr || later(*session, *latest[period]))
      latest[period] = session;
  }
  for (size_t period = 0U; period < 3U; ++period) {
    has[period] = latest[period] != nullptr;
    output[period] = has[period] ? kick_compare_session(service, *latest[period])
                                 : KickComparison{};
  }
}

bool kick_compare_latest_count_to_ten(const KickService &service,
                                      const PregnancyDate &date,
                                      KickComparison &output) {
  output = {};
  int32_t end_day = 0;
  if (!day_index(date, end_day)) return false;
  const KickSession *latest = nullptr;
  int32_t latest_day = 0;
  for (size_t index = 0U; index < service.count(); ++index) {
    const KickSession *session = service.at(index);
    int32_t session_day = 0;
    if (session == nullptr || session->mode != KickMode::CountToTen ||
        !day_index(session->date, session_day) || session_day > end_day ||
        session_day < end_day - kKickBaselineDays)
      continue;
    if (latest == nullptr || session_day > latest_day ||
        (session_day == latest_day && later(*session, *latest))) {
      latest = session;
      latest_day = session_day;
    }
  }
  if (latest == nullptr) return false;
  output = kick_compare_session(service, *latest);
  return true;
}

bool kick_summarize_awareness(const KickService &service,
                              const PregnancyDate &end_date,
                              KickAwarenessSummary &output) {
  output = {};
  int32_t end_day = 0;
  if (!day_index(end_date, end_day)) return false;
  const KickSession *latest = nullptr;
  int32_t latest_day = 0;
  for (size_t index = 0U; index < service.count(); ++index) {
    const KickSession *session = service.at(index);
    int32_t session_day = 0;
    if (session == nullptr || session->mode != KickMode::Awareness ||
        !day_index(session->date, session_day) || session_day > end_day ||
        session_day <= end_day - kKickBaselineDays ||
        output.total == UINT8_MAX)
      continue;
    if (session->feel == KickFeel::Usual) ++output.usual;
    else if (session->feel == KickFeel::Weaker) ++output.weaker;
    else if (session->feel == KickFeel::Fewer) ++output.fewer;
    else if (session->feel == KickFeel::Changed) ++output.changed;
    ++output.total;
    if (latest == nullptr || session_day > latest_day ||
        (session_day == latest_day && later(*session, *latest))) {
      latest = session;
      latest_day = session_day;
    }
  }
  if (latest == nullptr) return false;
  output.has_latest = true;
  output.latest = latest->feel;
  output.latest_date = latest->date;
  return true;
}
