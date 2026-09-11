#pragma once

#include "esp_err.h"
#include "persistent_state.h"

// Loads the newest valid slot, falling back to safe defaults when both slots
// are absent or corrupt.
esp_err_t sticky_settings_load(StickyDeviceSettings &settings);

// Writes a complete versioned record to the older of two NVS slots.
esp_err_t sticky_settings_save(const StickyDeviceSettings &settings);
