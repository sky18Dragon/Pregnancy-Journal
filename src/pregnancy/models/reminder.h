#pragma once

#include <cstdint>

#include "pregnancy_state.h"

constexpr uint8_t kReminderSchemaVersion = 2U;

enum class ReminderType : uint8_t {
  General,
  Supplement,
  Checkup,
  Water,
  Activity,
  Rest,
};

enum class RepeatRule : uint8_t {
  None,
  Daily,
  Weekly,
};

struct Reminder {
  uint8_t schema_version = kReminderSchemaVersion;
  uint32_t id = 0U;
  ReminderType type = ReminderType::General;
  PregnancyDate date = {};
  uint8_t hour = 0U;
  uint8_t minute = 0U;
  RepeatRule repeat_rule = RepeatRule::None;
  bool enabled = true;
  bool completed = false;
  char title[40] = {};
  char note[64] = {};
};

bool reminder_valid(const Reminder &reminder);
bool reminder_epoch(const Reminder &reminder, uint32_t &epoch_seconds);
const char *reminder_type_name(ReminderType type);
