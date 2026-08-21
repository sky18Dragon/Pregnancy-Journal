#include "sticky_battery.h"

#include <algorithm>
#include <cstdint>

#include "app_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pin_config.h"
#include "sticky_battery_protocol.h"

namespace {

constexpr char kTag[] = "sticky_battery";
constexpr uint32_t kI2cClockHz = 400000U;
constexpr int kI2cTimeoutMs = 50;

i2c_master_dev_handle_t s_device = nullptr;

esp_err_t read_word(uint8_t reg, uint16_t &value)
{
    uint8_t raw[2] = {};
    const esp_err_t result = i2c_master_transmit_receive(
        s_device, &reg, sizeof(reg), raw, sizeof(raw), kI2cTimeoutMs);
    if (result == ESP_OK) {
        value = StickyBatteryProtocol::decode_word(raw[0], raw[1]);
    }
    return result;
}

esp_err_t write_word(uint8_t reg, uint16_t value)
{
    const uint8_t payload[] = {
        reg,
        static_cast<uint8_t>(value & 0xFFU),
        static_cast<uint8_t>((value >> 8U) & 0xFFU),
    };
    return i2c_master_transmit(
        s_device, payload, sizeof(payload), kI2cTimeoutMs);
}

esp_err_t probe_device(uint16_t &device_id)
{
    // Requests DeviceType through Control, then reads its response from MAC Data.
    // 通过Control请求DeviceType，随后从MAC Data读取返回值。
    esp_err_t result = write_word(
        StickyBatteryProtocol::kControlRegister,
        StickyBatteryProtocol::kDeviceTypeCommand);
    if (result != ESP_OK) {
        return result;
    }
    vTaskDelay(pdMS_TO_TICKS(15));
    return read_word(StickyBatteryProtocol::kMacDataRegister, device_id);
}

}  // namespace

esp_err_t sticky_battery_init(i2c_master_bus_handle_t bus)
{
    app_log_register_tag(kTag);
    if (bus == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_device != nullptr) {
        return ESP_OK;
    }

    i2c_device_config_t config = {};
    config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    config.device_address = BQ27220_I2C_ADDR;
    config.scl_speed_hz = kI2cClockHz;
    esp_err_t result = i2c_master_bus_add_device(bus, &config, &s_device);
    if (result != ESP_OK) {
        STICKY_LOGE(kTag,
                    "battery=attach address=0x%02X result=%s",
                    BQ27220_I2C_ADDR,
                    esp_err_to_name(result));
        return result;
    }

    uint16_t device_id = 0U;
    result = probe_device(device_id);
    if (result != ESP_OK ||
        device_id != StickyBatteryProtocol::kExpectedDeviceId) {
        STICKY_LOGE(kTag,
                    "battery=probe address=0x%02X device_id=0x%04X result=%s",
                    BQ27220_I2C_ADDR,
                    static_cast<unsigned>(device_id),
                    result == ESP_OK ? "unexpected_device" :
                                       esp_err_to_name(result));
        i2c_master_bus_rm_device(s_device);
        s_device = nullptr;
        return result == ESP_OK ? ESP_ERR_NOT_FOUND : result;
    }

    STICKY_LOGI(kTag,
                "battery=ready address=0x%02X device_id=0x%04X result=ok",
                BQ27220_I2C_ADDR,
                static_cast<unsigned>(device_id));
    return ESP_OK;
}

esp_err_t sticky_battery_read(StickyBatteryReading &reading)
{
    if (s_device == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    uint16_t percent = 0U;
    const esp_err_t result = read_word(
        StickyBatteryProtocol::kStateOfChargeRegister, percent);
    if (result != ESP_OK) {
        STICKY_LOGW(kTag,
                    "battery=read register=state_of_charge result=%s",
                    esp_err_to_name(result));
        return result;
    }
    reading.percent = std::clamp<int>(percent, 0, 100);
    return ESP_OK;
}
