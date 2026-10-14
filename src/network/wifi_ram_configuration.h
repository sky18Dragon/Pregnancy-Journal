#pragma once

#include "esp_err.h"
#include "network_authorization.h"

// Applies station credentials only after esp_wifi_init(). The ESP-IDF Wi-Fi
// driver is forced to RAM storage before config is set, preventing its default
// implicit NVS persistence of SSID/password.
esp_err_t wifi_apply_ram_configuration(
    const NetworkCredentialPlaintext &credentials);
esp_err_t wifi_forget_ram_configuration();
