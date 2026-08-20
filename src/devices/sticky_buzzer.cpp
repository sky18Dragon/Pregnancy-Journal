#include "sticky_buzzer.h"

#include <atomic>

#include "app_log.h"
#include "driver/ledc.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pin_config.h"

namespace {

constexpr char kTag[] = "sticky_buzzer";
constexpr ledc_mode_t kSpeedMode = LEDC_LOW_SPEED_MODE;
constexpr ledc_timer_t kTimer = LEDC_TIMER_0;
constexpr ledc_channel_t kChannel = LEDC_CHANNEL_0;
constexpr uint32_t kAlarmDuty = 256;
constexpr uint32_t kDefaultFrequencyHz = 1047;
constexpr TickType_t kCheckInterval = pdMS_TO_TICKS(20);
constexpr uint32_t kTaskStackSize = 3072;
constexpr UBaseType_t kTaskPriority = 4;

struct AlarmNote {
    uint32_t frequency_hz;
    uint32_t duration_ms;
    uint32_t gap_ms;
    uint32_t duty;
};

constexpr AlarmNote kAlarmNotes[] = {
    {1047, 140, 60, kAlarmDuty},
    {1319, 140, 60, kAlarmDuty},
    {1568, 220, 1200, kAlarmDuty},
};

constexpr AlarmNote kHatchNotes[] = {
    {784, 110, 70, 140},
    {988, 130, 70, 140},
    {1319, 260, 0, 160},
};

enum class BuzzerMode : uint8_t {
    Silent,
    Alarm,
    HatchChime,
};

TaskHandle_t s_alarm_task = nullptr;
std::atomic<BuzzerMode> s_mode = BuzzerMode::Silent;

esp_err_t set_duty(uint32_t duty)
{
    ESP_RETURN_ON_ERROR(
        ledc_set_duty(kSpeedMode, kChannel, duty), kTag, "set duty");
    return ledc_update_duty(kSpeedMode, kChannel);
}

bool wait_while_mode(BuzzerMode mode, uint32_t duration_ms)
{
    TickType_t remaining = pdMS_TO_TICKS(duration_ms);
    while (remaining > 0 && s_mode.load() == mode) {
        const TickType_t slice = remaining > kCheckInterval
                                     ? kCheckInterval
                                     : remaining;
        vTaskDelay(slice);
        remaining -= slice;
    }
    return s_mode.load() == mode;
}

esp_err_t play_note(const AlarmNote &note, BuzzerMode mode)
{
    ESP_RETURN_ON_ERROR(
        ledc_set_freq(kSpeedMode, kTimer, note.frequency_hz),
        kTag,
        "set note frequency");
    if (s_mode.load() != mode) {
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(set_duty(note.duty), kTag, "start note");
    const bool keep_playing = wait_while_mode(mode, note.duration_ms);
    ESP_RETURN_ON_ERROR(set_duty(0), kTag, "finish note");
    if (keep_playing) {
        wait_while_mode(mode, note.gap_ms);
    }
    return ESP_OK;
}

void alarm_task(void *)
{
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        while (s_mode.load() == BuzzerMode::Alarm) {
            for (const AlarmNote &note : kAlarmNotes) {
                if (s_mode.load() != BuzzerMode::Alarm) {
                    break;
                }
                const esp_err_t result = play_note(note, BuzzerMode::Alarm);
                if (result != ESP_OK) {
                    s_mode.store(BuzzerMode::Silent);
                    set_duty(0);
                    STICKY_LOGW(kTag,
                                "buzzer=alarm state=failed result=%s",
                                esp_err_to_name(result));
                    break;
                }
            }
        }
        if (s_mode.load() == BuzzerMode::HatchChime) {
            esp_err_t result = ESP_OK;
            for (const AlarmNote &note : kHatchNotes) {
                if (s_mode.load() != BuzzerMode::HatchChime) {
                    break;
                }
                result = play_note(note, BuzzerMode::HatchChime);
                if (result != ESP_OK) {
                    break;
                }
            }
            BuzzerMode expected = BuzzerMode::HatchChime;
            const bool completed = s_mode.compare_exchange_strong(
                expected, BuzzerMode::Silent);
            if (result == ESP_OK && completed) {
                STICKY_LOGI(kTag,
                            "buzzer=hatch_chime state=finished result=ok");
            } else if (result != ESP_OK) {
                STICKY_LOGW(kTag,
                            "buzzer=hatch_chime state=failed result=%s",
                            esp_err_to_name(result));
            }
        }
        set_duty(0);
    }
}

}  // namespace

esp_err_t sticky_buzzer_init()
{
    app_log_register_tag(kTag);

    // Uses the reference driver's GPIO48, low-speed LEDC, timer and 10-bit duty.
    // 沿用参考驱动的GPIO48、低速LEDC、定时器和10位占空比配置。
    ledc_timer_config_t timer_config = {};
    timer_config.speed_mode = kSpeedMode;
    timer_config.timer_num = kTimer;
    timer_config.duty_resolution = LEDC_TIMER_10_BIT;
    timer_config.freq_hz = kDefaultFrequencyHz;
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

    if (xTaskCreate(alarm_task,
                    "sticky_buzzer",
                    kTaskStackSize,
                    nullptr,
                    kTaskPriority,
                    &s_alarm_task) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    STICKY_LOGI(kTag,
                "buzzer=ready pin=%d patterns=alarm,hatch_chime result=ok",
                PIN_BUZZER);
    return ESP_OK;
}

esp_err_t sticky_buzzer_start_alarm()
{
    if (s_alarm_task == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_mode.exchange(BuzzerMode::Alarm) != BuzzerMode::Alarm) {
        xTaskNotifyGive(s_alarm_task);
        STICKY_LOGI(kTag, "buzzer=alarm state=started pattern=three_note");
    }
    return ESP_OK;
}

esp_err_t sticky_buzzer_play_hatch_chime()
{
    if (s_alarm_task == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    s_mode.store(BuzzerMode::HatchChime);
    xTaskNotifyGive(s_alarm_task);
    STICKY_LOGI(kTag,
                "buzzer=hatch_chime state=started pattern=gentle_three_note");
    return ESP_OK;
}

esp_err_t sticky_buzzer_stop()
{
    const BuzzerMode previous = s_mode.exchange(BuzzerMode::Silent);
    ESP_RETURN_ON_ERROR(set_duty(0), kTag, "stop alarm");
    if (previous != BuzzerMode::Silent) {
        STICKY_LOGI(kTag,
                    "buzzer=stopped previous=%s",
                    previous == BuzzerMode::Alarm ? "alarm" : "hatch_chime");
    }
    return ESP_OK;
}
