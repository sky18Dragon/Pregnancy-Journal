#pragma once

#include "esp_err.h"
#include "pregnancy_state.h"

esp_err_t pregnancy_storage_load(PregnancyDate &due_date, bool &found);
esp_err_t pregnancy_storage_save(const PregnancyDate &due_date);
