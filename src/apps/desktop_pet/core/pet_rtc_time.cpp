#include "pet_rtc_time.h"

namespace {

constexpr uint8_t kDaysInMonth[] = {
    31U, 28U, 31U, 30U, 31U, 30U,
    31U, 31U, 30U, 31U, 30U, 31U,
};

bool leap_year(uint16_t year)
{
    return (year % 4U == 0U && year % 100U != 0U) ||
           year % 400U == 0U;
}

uint16_t days_in_year(uint16_t year)
{
    return leap_year(year) ? 366U : 365U;
}

uint8_t days_in_month(uint16_t year, uint8_t month)
{
    if (month == 2U && leap_year(year)) {
        return 29U;
    }
    return month >= 1U && month <= 12U ? kDaysInMonth[month - 1U] : 0U;
}

}  // namespace

bool pet_rtc_time_valid(const PetRtcDateTime &value)
{
    return value.year >= 1970U && value.year <= 2099U &&
           value.month >= 1U && value.month <= 12U &&
           value.day >= 1U &&
           value.day <= days_in_month(value.year, value.month) &&
           value.hour <= 23U && value.minute <= 59U && value.second <= 59U;
}

bool pet_rtc_time_to_epoch(const PetRtcDateTime &value,
                           uint32_t &epoch_seconds)
{
    if (!pet_rtc_time_valid(value)) {
        return false;
    }
    uint32_t total_days = 0U;
    for (uint16_t year = 1970U; year < value.year; ++year) {
        total_days += days_in_year(year);
    }
    for (uint8_t month = 1U; month < value.month; ++month) {
        total_days += days_in_month(value.year, month);
    }
    total_days += value.day - 1U;
    epoch_seconds = total_days * 86400U +
                    static_cast<uint32_t>(value.hour) * 3600U +
                    static_cast<uint32_t>(value.minute) * 60U +
                    value.second;
    return true;
}
