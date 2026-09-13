#pragma once

#include "esp_err.h"
#include "pregnancy_state.h"

esp_err_t pregnancy_storage_load(PregnancyProfile &profile, bool &found);
esp_err_t pregnancy_storage_save(const PregnancyProfile &profile);

// Compatibility wrappers retained while the UI migrates to PregnancyProfile.
esp_err_t pregnancy_storage_load(PregnancyDate &due_date, bool &found);
esp_err_t pregnancy_storage_save(const PregnancyDate &due_date);
