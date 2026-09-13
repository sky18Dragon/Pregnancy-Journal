#include "sticky_rtc.h"

#include "app_log.h"
#include "pin_config.h"

namespace {

constexpr char kTag[] = "sticky_rtc";
constexpr uint32_t kI2cClockHz = 400000U;
constexpr int kI2cTimeoutMs = 100;
constexpr uint8_t kTimeRegister = 0x02U;
constexpr uint8_t kLowVoltageFlag = 0x80U;

i2c_master_dev_handle_t s_device = nullptr;

bool valid_bcd(uint8_t value)
{
    return (value & 0x0FU) <= 9U && ((value >> 4U) & 0x0FU) <= 9U;
}

uint8_t bcd_to_decimal(uint8_t value)
{
    return static_cast<uint8_t>((value >> 4U) * 10U + (value & 0x0FU));
}

uint8_t decimal_to_bcd(uint8_t value)
{
    return static_cast<uint8_t>((value / 10U) << 4U | (value % 10U));
}

bool leap_year(uint16_t year)
{
    return (year % 4U == 0U && year % 100U != 0U) ||
           year % 400U == 0U;
}

uint8_t days_in_month(uint16_t year, uint8_t month)
{
    constexpr uint8_t kDays[] = {
        31U, 28U, 31U, 30U, 31U, 30U,
        31U, 31U, 30U, 31U, 30U, 31U,
    };
    if (month < 1U || month > 12U) {
        return 0U;
    }
    return month == 2U && leap_year(year) ? 29U : kDays[month - 1U];
}

bool valid_date_time(const StickyRtcDateTime &value)
{
    return value.year >= 1900U && value.year <= 2099U &&
           value.month >= 1U && value.month <= 12U &&
           value.day >= 1U &&
           value.day <= days_in_month(value.year, value.month) &&
           value.hour <= 23U && value.minute <= 59U && value.second <= 59U;
}

uint8_t build_month(const char *date)
{
    constexpr char kMonths[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    for (uint8_t index = 0U; index < 12U; ++index) {
        const size_t offset = static_cast<size_t>(index) * 3U;
        if (date[0] == kMonths[offset] &&
            date[1] == kMonths[offset + 1U] &&
            date[2] == kMonths[offset + 2U]) {
            return static_cast<uint8_t>(index + 1U);
        }
    }
    return 0U;
}

uint8_t two_digits(const char *text)
{
    const uint8_t tens = text[0] == ' '
                             ? 0U
                             : static_cast<uint8_t>(text[0] - '0');
    const uint8_t ones = static_cast<uint8_t>(text[1] - '0');
    return static_cast<uint8_t>(tens * 10U + ones);
}

bool build_date_time(StickyRtcDateTime &value)
{
    constexpr char kBuildDate[] = __DATE__;
    constexpr char kBuildTime[] = __TIME__;
    value.month = build_month(kBuildDate);
    value.day = two_digits(kBuildDate + 4U);
    value.year = static_cast<uint16_t>(
        (kBuildDate[7] - '0') * 1000U +
        (kBuildDate[8] - '0') * 100U +
        (kBuildDate[9] - '0') * 10U +
        (kBuildDate[10] - '0'));
    value.hour = two_digits(kBuildTime);
    value.minute = two_digits(kBuildTime + 3U);
    value.second = two_digits(kBuildTime + 6U);
    return valid_date_time(value);
}

uint8_t weekday(const StickyRtcDateTime &value)
{
    uint32_t days = 0U;
    for (uint16_t year = 1970U; year < value.year; ++year) {
        days += leap_year(year) ? 366U : 365U;
    }
    for (uint8_t month = 1U; month < value.month; ++month) {
        days += days_in_month(value.year, month);
    }
    days += value.day - 1U;
    return static_cast<uint8_t>((days + 4U) % 7U);
}

esp_err_t write_date_time(const StickyRtcDateTime &value,
                          const char *source)
{
    if (s_device == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!valid_date_time(value)) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t payload[] = {
        kTimeRegister,
        decimal_to_bcd(value.second),
        decimal_to_bcd(value.minute),
        decimal_to_bcd(value.hour),
        decimal_to_bcd(value.day),
        weekday(value),
        static_cast<uint8_t>(
            decimal_to_bcd(value.month) |
            (value.year < 2000U ? 0x80U : 0x00U)),
        decimal_to_bcd(static_cast<uint8_t>(value.year % 100U)),
    };
    const esp_err_t result = i2c_master_transmit(
        s_device, payload, sizeof(payload), kI2cTimeoutMs);
    if (result != ESP_OK) {
        STICKY_LOGW(kTag,
                    "rtc=write source=%s result=%s",
                    source,
                    esp_err_to_name(result));
        return result;
    }
    STICKY_LOGI(kTag,
                "rtc=write source=%s time=%04u-%02u-%02uT%02u:%02u:%02u result=ok",
                source,
                static_cast<unsigned>(value.year),
                static_cast<unsigned>(value.month),
                static_cast<unsigned>(value.day),
                static_cast<unsigned>(value.hour),
                static_cast<unsigned>(value.minute),
                static_cast<unsigned>(value.second));
    return ESP_OK;
}

}  // namespace

esp_err_t sticky_rtc_init(i2c_master_bus_handle_t bus)
{
    app_log_register_tag(kTag);
    if (bus == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_device != nullptr) {
        return ESP_OK;
    }

    // Match the address and 400 kHz bus speed used by the hardware demo.
    // 使用与硬件示例一致的地址和400 kHz总线速度。
    i2c_device_config_t config = {};
    config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    config.device_address = PCF8563_I2C_ADDR;
    config.scl_speed_hz = kI2cClockHz;

    const esp_err_t result =
        i2c_master_bus_add_device(bus, &config, &s_device);
    if (result == ESP_OK) {
        STICKY_LOGI(kTag,
                    "rtc=ready address=0x%02X frequency_hz=%u result=ok",
                    PCF8563_I2C_ADDR,
                    static_cast<unsigned>(kI2cClockHz));
    }
    return result;
}

bool sticky_rtc_is_ready()
{
    return s_device != nullptr;
}

esp_err_t sticky_rtc_read(StickyRtcDateTime &date_time)
{
    if (s_device == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t raw[7] = {};
    const esp_err_t result = i2c_master_transmit_receive(
        s_device, &kTimeRegister, 1U, raw, sizeof(raw), kI2cTimeoutMs);
    if (result != ESP_OK) {
        STICKY_LOGW(kTag,
                    "rtc=read result=%s",
                    esp_err_to_name(result));
        return result;
    }

    // The VL flag means the stored calendar cannot be trusted.
    // VL标志表示当前保存的日历时间不可信。
    if ((raw[0] & kLowVoltageFlag) != 0U) {
        STICKY_LOGW(kTag, "rtc=read validity=low_voltage result=invalid");
        return ESP_ERR_INVALID_STATE;
    }

    const uint8_t second = raw[0] & 0x7FU;
    const uint8_t minute = raw[1] & 0x7FU;
    const uint8_t hour = raw[2] & 0x3FU;
    const uint8_t day = raw[3] & 0x3FU;
    const uint8_t month = raw[5] & 0x1FU;
    const uint8_t year = raw[6];
    if (!valid_bcd(second) || !valid_bcd(minute) || !valid_bcd(hour) ||
        !valid_bcd(day) || !valid_bcd(month) || !valid_bcd(year)) {
        STICKY_LOGW(kTag, "rtc=read validity=bcd result=invalid");
        return ESP_ERR_INVALID_RESPONSE;
    }

    StickyRtcDateTime value = {};
    value.second = bcd_to_decimal(second);
    value.minute = bcd_to_decimal(minute);
    value.hour = bcd_to_decimal(hour);
    value.day = bcd_to_decimal(day);
    value.month = bcd_to_decimal(month);
    const uint8_t short_year = bcd_to_decimal(year);
    value.year = static_cast<uint16_t>(
        (raw[5] & 0x80U) != 0U ? 1900U + short_year
                               : 2000U + short_year);
    if (!valid_date_time(value)) {
        STICKY_LOGW(kTag, "rtc=read validity=range result=invalid");
        return ESP_ERR_INVALID_RESPONSE;
    }

    date_time = value;
#if STICKY_LOG_RTC_READS_ENABLED
    STICKY_LOGD(kTag,
                "rtc=read time=%04u-%02u-%02uT%02u:%02u:%02u result=ok",
                static_cast<unsigned>(value.year),
                static_cast<unsigned>(value.month),
                static_cast<unsigned>(value.day),
                static_cast<unsigned>(value.hour),
                static_cast<unsigned>(value.minute),
                static_cast<unsigned>(value.second));
#endif
    return ESP_OK;
}

esp_err_t sticky_rtc_write(const StickyRtcDateTime &date_time)
{
    return write_date_time(date_time, "user");
}

bool sticky_rtc_epoch_seconds(const StickyRtcDateTime &date_time,
                              uint32_t &epoch_seconds)
{
    if (!valid_date_time(date_time) || date_time.year < 1970U) return false;
    uint32_t days = 0U;
    for (uint16_t year = 1970U; year < date_time.year; ++year)
        days += leap_year(year) ? 366U : 365U;
    for (uint8_t month = 1U; month < date_time.month; ++month)
        days += days_in_month(date_time.year, month);
    days += static_cast<uint32_t>(date_time.day - 1U);
    epoch_seconds = days * 86400U +
                    static_cast<uint32_t>(date_time.hour) * 3600U +
                    static_cast<uint32_t>(date_time.minute) * 60U +
                    date_time.second;
    return true;
}

esp_err_t sticky_rtc_seed_from_build_time(StickyRtcDateTime &date_time)
{
    if (s_device == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    StickyRtcDateTime value = {};
    if (!build_date_time(value) || value.year < 2000U) {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_err_t result = write_date_time(value, "firmware_build");
    if (result != ESP_OK) {
        return result;
    }

    date_time = value;
    return ESP_OK;
}
