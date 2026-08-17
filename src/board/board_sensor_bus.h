#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"

// Creates I2C1 for the onboard sensor devices.
// 为板载传感器创建I2C1总线。
esp_err_t board_sensor_bus_init();

// Returns the shared I2C1 handle after initialization.
// 初始化完成后返回共享I2C1句柄。
i2c_master_bus_handle_t board_sensor_i2c_bus();
