#include "wifi_ram_configuration.h"

#include <cstring>

#include "esp_wifi.h"

namespace {

void secure_clear(void *value, size_t size)
{
    volatile uint8_t *bytes = static_cast<volatile uint8_t *>(value);
    while (size-- > 0U) *bytes++ = 0U;
}

}  // namespace

esp_err_t wifi_apply_ram_configuration(
    const NetworkCredentialPlaintext &credentials)
{
    if (!network_credential_plaintext_valid(credentials))
        return ESP_ERR_INVALID_ARG;
    esp_err_t result = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (result != ESP_OK) return result;
    wifi_config_t config = {};
    std::memcpy(config.sta.ssid, credentials.ssid,
                std::strlen(credentials.ssid));
    std::memcpy(config.sta.password, credentials.password,
                std::strlen(credentials.password));
    config.sta.threshold.authmode = credentials.password[0] == '\0'
                                        ? WIFI_AUTH_OPEN
                                        : WIFI_AUTH_WPA2_PSK;
    result = esp_wifi_set_config(WIFI_IF_STA, &config);
    secure_clear(&config, sizeof(config));
    return result;
}

esp_err_t wifi_forget_ram_configuration()
{
    esp_err_t result = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (result != ESP_OK) return result;
    wifi_config_t config = {};
    result = esp_wifi_set_config(WIFI_IF_STA, &config);
    secure_clear(&config, sizeof(config));
    return result;
}
