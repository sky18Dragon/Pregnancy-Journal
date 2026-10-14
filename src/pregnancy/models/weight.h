#pragma once

#include <cstdint>

#include "pregnancy_state.h"

constexpr uint8_t kWeightRecordSchemaVersion = 1U;

struct WeightRecord {
  uint8_t schema_version = kWeightRecordSchemaVersion;
  uint32_t id = 0U;
  PregnancyDate date = {};
  uint16_t weight_tenths_kg = 0U;
  char note[40] = {};
};

struct WeightProfileSettings {
  uint16_t height_cm = 165U;
  uint16_t baseline_tenths_kg = 0U;
  bool use_pounds = false;
};

struct WeightSummary {
  uint16_t current_tenths_kg = 0U;
  int16_t change_tenths_kg = 0;
  uint16_t bmi_tenths = 0U;
  int16_t weekly_change_tenths_kg = 0;

};

struct WeightTrend {
  PregnancyDate dates[30] = {};
  uint16_t values[30] = {};
  bool present[30] = {};
  uint8_t days = 0U;
  uint8_t count = 0U;
};

bool weight_record_valid(const WeightRecord &record);
