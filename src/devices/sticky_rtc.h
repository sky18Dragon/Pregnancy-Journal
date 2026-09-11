#pragma once

#include <cstdint>

#include "driver/i2c_master.h"
#include "esp_err.h"

struct StickyRtcDateTime {
    uint16_t year = 0U;
    uint8_t month = 0U;
    uint8_t day = 0U;
    uint8_t hour = 0U;
    uint8_t minute = 0U;
    uint8_t second = 0U;
};

// Attaches the read-only PCF8563 to the shared onboard sensor bus.
// 将只读PCF8563挂载到板载共享传感器总线。
esp_err_t sticky_rtc_init(i2c_master_bus_handle_t bus);

// Returns whether the PCF8563 device has been attached successfully.
// 返回PCF8563设备是否已经成功挂载。
bool sticky_rtc_is_ready();

// Reads and validates one complete calendar snapshot from the PCF8563.
// 从PCF8563读取并校验一份完整日历快照。
esp_err_t sticky_rtc_read(StickyRtcDateTime &date_time);

// Validates and writes a complete user-selected calendar snapshot.
// 校验并写入用户选择的完整日历时间。
esp_err_t sticky_rtc_write(const StickyRtcDateTime &date_time);

// Seeds an invalid clock once from the firmware build date and time.
// 使用固件构建日期和时间为无效RTC执行一次初始校时。
esp_err_t sticky_rtc_seed_from_build_time(StickyRtcDateTime &date_time);
