#pragma once

#include <cstddef>
#include <cstdint>

#include "kick_service.h"

// Compares a session with the same person's own recent records. Output is
// descriptive data only (direction and size of the difference); it carries
// no thresholds, labels or advice.

// Baseline window: the seven calendar days before the compared session.
constexpr int32_t kKickBaselineDays = 7;
constexpr uint8_t kKickBaselineMinimumSessions = 3U;
// Timed sessions shorter than this are not converted to an hourly rate.
constexpr uint16_t kKickMinimumTimedSeconds = 15U * 60U;
// A count-to-ten session that ran for an hour without reaching ten.
constexpr uint16_t kKickCountToTenCapSeconds = 60U * 60U;
constexpr size_t kKickBaselineSampleLimit = 64U;

enum class KickComparisonStatus : uint8_t {
  Ready,               // Baseline exists; difference fields are filled.
  CollectingBaseline,  // Fewer than kKickBaselineMinimumSessions samples.
  NotComparable,       // Session too short or ended before its target.
  Unsupported,         // Awareness check-ins have no numeric value.
};

enum class KickDirection : uint8_t { Same, Less, More };

struct KickComparison {
  KickComparisonStatus status = KickComparisonStatus::Unsupported;
  KickMode mode = KickMode::Timed;
  KickPeriod period = KickPeriod::Morning;
  // Timed: movements per hour. CountToTen: seconds taken to reach ten.
  uint16_t current = 0U;
  // Median of the baseline samples, in the same unit as current.
  uint16_t baseline = 0U;
  uint16_t difference = 0U;  // |current - baseline|
  uint16_t percent = 0U;     // difference / baseline, rounded
  bool percent_known = false;  // false when the baseline median is zero
  KickDirection direction = KickDirection::Same;
  uint8_t baseline_sessions = 0U;
  // CountToTen ran a full hour without reaching ten; current is a lower bound.
  bool current_capped = false;
};

struct KickAwarenessSummary {
  uint8_t usual = 0U;
  uint8_t weaker = 0U;
  uint8_t fewer = 0U;
  uint8_t changed = 0U;
  uint8_t total = 0U;
  KickFeel latest = KickFeel::None;
  PregnancyDate latest_date = {};
  bool has_latest = false;
};

// Movements per hour, or false when the session is too short to convert.
bool kick_timed_rate_per_hour(const KickSession &session, uint16_t &rate);
// Seconds to ten; capped sessions report kKickCountToTenCapSeconds. False when
// the session ended early without reaching ten.
bool kick_count_to_ten_seconds(const KickSession &session, uint16_t &seconds,
                               bool &capped);

// The session itself (matched by id) never joins its own baseline.
KickComparison kick_compare_session(const KickService &service,
                                    const KickSession &session);

// Latest Timed session per period on date; has[i] is false when none exists.
void kick_compare_day(const KickService &service, const PregnancyDate &date,
                      KickComparison output[3], bool has[3]);

// Most recent CountToTen session within the baseline window ending at date.
bool kick_compare_latest_count_to_ten(const KickService &service,
                                      const PregnancyDate &date,
                                      KickComparison &output);

// Awareness check-ins from the seven days ending at end_date (inclusive).
bool kick_summarize_awareness(const KickService &service,
                              const PregnancyDate &end_date,
                              KickAwarenessSummary &output);
