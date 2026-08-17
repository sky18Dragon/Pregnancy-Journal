#include "board_sensor_bus.h"

#include "app_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pin_config.h"

namespace {

constexpr char kTag[] = "sticky_sensor_bus";
i2c_master_bus_handle_t s_sensor_i2c_bus = nullptr;

}  // namespace

esp_err_t board_sensor_bus_init()
{
    app_log_register_tag(kTag);
    if (s_sensor_i2c_bus != nullptr) {
        return ESP_OK;
    }

    // Matches the I2C1 configuration used by the hardware reference project.
    // 使用与硬件参考工程相同的I2C1配置。
    i2c_master_bus_config_t sensor_bus_config = {};
    sensor_bus_config.i2c_port = I2C_NUM_1;
    sensor_bus_config.sda_io_num = static_cast<gpio_num_t>(PIN_SENSOR_SDA);
    sensor_bus_config.scl_io_num = static_cast<gpio_num_t>(PIN_SENSOR_SCL);
    sensor_bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    sensor_bus_config.glitch_ignore_cnt = 7;
    sensor_bus_config.flags.enable_internal_pullup = 1;
    const esp_err_t result =
        i2c_new_master_bus(&sensor_bus_config, &s_sensor_i2c_bus);
    if (result != ESP_OK) {
        STICKY_LOGE(kTag,
                    "sensor_bus=init result=%s",
                    esp_err_to_name(result));
        return result;
    }

    vTaskDelay(pdMS_TO_TICKS(100));
    STICKY_LOGI(kTag,
                "sensor_bus=ready port=1 sda=%d scl=%d result=ok",
                PIN_SENSOR_SDA,
                PIN_SENSOR_SCL);
    return ESP_OK;
}

i2c_master_bus_handle_t board_sensor_i2c_bus()
{
    return s_sensor_i2c_bus;
}
