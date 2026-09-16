#pragma once

#include <cstddef>
#include <cstdint>

#include "pregnancy_state.h"

constexpr uint8_t kKickSessionSchemaVersion = 1U;

enum class KickPeriod : uint8_t { Morning, Afternoon, Evening };

struct KickSession {
  uint8_t schema_version = kKickSessionSchemaVersion;
  uint32_t id = 0U;
  PregnancyDate date = {};
  uint8_t hour = 0U;
  uint8_t minute = 0U;
  uint16_t duration_minutes = 0U;
  uint16_t count = 0U;
  KickPeriod period = KickPeriod::Morning;
  char note[40] = {};
};

struct KickDaySummary {
  uint16_t morning = 0U;
  uint16_t afternoon = 0U;
  uint16_t evening = 0U;
  uint16_t total = 0U;
  uint16_t session_count = 0U;
  uint16_t duration_minutes = 0U;
  uint16_t latest_count = 0U;
  uint16_t latest_duration_minutes = 0U;
  uint8_t latest_hour = 0U;
  uint8_t latest_minute = 0U;
  bool has_latest = false;
};

constexpr size_t kKickTrendDays = 7U;

struct KickTrendSummary {
  PregnancyDate dates[kKickTrendDays] = {};
  uint16_t totals[kKickTrendDays] = {};
  uint16_t session_counts[kKickTrendDays] = {};
  uint16_t duration_minutes[kKickTrendDays] = {};
  uint16_t period_totals[3] = {};
  uint16_t total = 0U;
  uint16_t session_count = 0U;
  uint16_t average_per_recorded_day = 0U;
  uint8_t recorded_days = 0U;
  KickPeriod busiest_period = KickPeriod::Morning;
  bool has_data = false;
};

bool kick_session_valid(const KickSession &session);
const char *kick_period_name(KickPeriod period);
