#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"

struct StickyBatteryReading {
    int percent = 0;
};

// Attaches the BQ27220 fuel gauge to the shared sensor I2C bus.
// 将BQ27220电量计挂载到共享传感器I2C总线。
esp_err_t sticky_battery_init(i2c_master_bus_handle_t bus);

// Reads the fuel gauge state of charge as a clamped 0-100 percentage.
// 读取电量计的荷电状态，并限制在0到100的百分比范围内。
esp_err_t sticky_battery_read(StickyBatteryReading &reading);
