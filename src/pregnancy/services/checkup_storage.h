#pragma once

#include "checkup_service.h"
#include "esp_err.h"

esp_err_t checkup_storage_load(CheckupService &service);
esp_err_t checkup_storage_save(const CheckupService &service);
bool checkup_storage_next_event(uint32_t now, uint32_t &epoch);
