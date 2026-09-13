#pragma once

#include "esp_err.h"
#include "weight_service.h"

struct WeightProfileSettings {
  uint16_t height_cm = 165U;
  uint16_t baseline_tenths_kg = 0U;
  bool use_pounds = false;
};

esp_err_t weight_storage_load(WeightService &service,
                              WeightProfileSettings &settings);
esp_err_t weight_storage_save(const WeightService &service,
                              const WeightProfileSettings &settings);
