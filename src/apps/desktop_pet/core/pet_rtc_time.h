#pragma once

#include <cstdint>

// Calendar conversion is adapted from esp32-artoria-tamagotchi and receives
// validated PCF8563 values from the Sticky hardware adapter.
// 日历换算改编自esp32-artoria-tamagotchi，并接收Sticky适配层校验后的PCF8563数据。

struct PetRtcDateTime {
    uint16_t year = 0U;
    uint8_t month = 0U;
    uint8_t day = 0U;
    uint8_t hour = 0U;
    uint8_t minute = 0U;
    uint8_t second = 0U;
};

bool pet_rtc_time_valid(const PetRtcDateTime &value);
bool pet_rtc_time_to_epoch(const PetRtcDateTime &value,
                           uint32_t &epoch_seconds);

// Converts Unix seconds back into the calendar fields shown by the pet UI.
// 将Unix秒数还原为桌宠界面显示的日历字段。
bool pet_rtc_time_from_epoch(uint32_t epoch_seconds,
                             PetRtcDateTime &value);
