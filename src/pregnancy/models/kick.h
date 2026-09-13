#pragma once

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
  uint16_t sampled_total = 0U;
  uint16_t estimated_12h = 0U;
  int16_t change_percent = 0;
  bool complete = false;
  bool attention = false;
};

bool kick_session_valid(const KickSession &session);
const char *kick_period_name(KickPeriod period);
