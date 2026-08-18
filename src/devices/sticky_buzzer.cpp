#include "sticky_buzzer.h"

#include "app_log.h"
#include "driver/ledc.h"
#include "esp_check.h"
#include "pin_config.h"

namespace {

constexpr char kTag[] = "sticky_buzzer";
constexpr ledc_mode_t kSpeedMode = LEDC_LOW_SPEED_MODE;
constexpr ledc_timer_t kTimer = LEDC_TIMER_0;
constexpr ledc_channel_t kChannel = LEDC_CHANNEL_0;
constexpr uint32_t kAlarmFrequencyHz = 2400;
constexpr uint32_t kAlarmDuty = 256;

esp_err_t set_duty(uint32_t duty)
{
    ESP_RETURN_ON_ERROR(
        ledc_set_duty(kSpeedMode, kChannel, duty), kTag, "set duty");
    return ledc_update_duty(kSpeedMode, kChannel);
}

}  // namespace

esp_err_t sticky_buzzer_init()
{
    app_log_register_tag(kTag);

    // Matches the verified dashboard demo: GPIO48, 2400 Hz, 10-bit LEDC.
    // 与已验证示例保持一致：GPIO48、2400Hz、10位LEDC。
    ledc_timer_config_t timer_config = {};
    timer_config.speed_mode = kSpeedMode;
    timer_config.timer_num = kTimer;
    timer_config.duty_resolution = LEDC_TIMER_10_BIT;
    timer_config.freq_hz = kAlarmFrequencyHz;
    timer_config.clk_cfg = LEDC_AUTO_CLK;
    ESP_RETURN_ON_ERROR(
        ledc_timer_config(&timer_config), kTag, "configure timer");

    ledc_channel_config_t channel_config = {};
    channel_config.gpio_num = PIN_BUZZER;
    channel_config.speed_mode = kSpeedMode;
    channel_config.channel = kChannel;
    channel_config.intr_type = LEDC_INTR_DISABLE;
    channel_config.timer_sel = kTimer;
    channel_config.duty = 0;
    channel_config.hpoint = 0;
    ESP_RETURN_ON_ERROR(
        ledc_channel_config(&channel_config), kTag, "configure channel");

    STICKY_LOGI(kTag,
                "buzzer=ready pin=%d frequency_hz=%u result=ok",
                PIN_BUZZER,
                static_cast<unsigned>(kAlarmFrequencyHz));
    return ESP_OK;
}

esp_err_t sticky_buzzer_start_alarm()
{
    ESP_RETURN_ON_ERROR(
        ledc_set_freq(kSpeedMode, kTimer, kAlarmFrequencyHz),
        kTag,
        "set alarm frequency");
    ESP_RETURN_ON_ERROR(set_duty(kAlarmDuty), kTag, "start alarm");
    STICKY_LOGI(kTag, "buzzer=alarm state=started");
    return ESP_OK;
}

esp_err_t sticky_buzzer_stop()
{
    ESP_RETURN_ON_ERROR(set_duty(0), kTag, "stop alarm");
    STICKY_LOGI(kTag, "buzzer=alarm state=stopped");
    return ESP_OK;
}
