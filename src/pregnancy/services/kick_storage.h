#pragma once

#include "esp_err.h"
#include "kick_service.h"

esp_err_t kick_storage_load(KickService &service);
esp_err_t kick_storage_save(const KickService &service);
