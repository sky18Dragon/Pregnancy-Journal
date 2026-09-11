#include "pregnancy_app.h"

#include <cstdio>
#include <cstring>

#include "app_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pregnancy_pages.h"
#include "pregnancy_state.h"
#include "pregnancy_storage.h"
#include "sticky_app_lifecycle.h"
#include "sticky_display.h"
#include "sticky_rtc.h"
#include "sticky_touch.h"

namespace {

constexpr char kTag[] = "pregnancy_app";
constexpr TickType_t kPollInterval = pdMS_TO_TICKS(40);
constexpr uint32_t kTaskStackSize = 4608U;
constexpr UBaseType_t kTaskPriority = 3U;
constexpr uint32_t kDashboardSleepTimeoutMs = 60U * 1000U;
constexpr int64_t kRtcPollIntervalUs = 60LL * 1000000LL;

Canvas *s_canvas = nullptr;
TaskHandle_t s_app_task = nullptr;
StickyAppLifecycle s_lifecycle = {};
CanvasRotation s_display_rotation = CanvasRotation::Deg180;
PregnancyPage s_page = PregnancyPage::ClockSetup;
PregnancyDate s_today = {};
PregnancyDate s_due_date = {};
PregnancyProgress s_progress = {};
bool s_configured = false;
bool s_input_error = false;
bool s_replace_on_digit = false;
char s_clock_digits[13] = {};
size_t s_clock_length = 0U;
char s_due_digits[9] = {};
size_t s_due_length = 0U;
int32_t s_last_day_index = -1;
int64_t s_next_rtc_poll_us = 0;

void fill_clock_digits(const StickyRtcDateTime &value)
{
    const uint16_t year = static_cast<uint16_t>(value.year % 10000U);
    s_clock_digits[0] = static_cast<char>('0' + year / 1000U);
    s_clock_digits[1] = static_cast<char>('0' + year / 100U % 10U);
    s_clock_digits[2] = static_cast<char>('0' + year / 10U % 10U);
    s_clock_digits[3] = static_cast<char>('0' + year % 10U);
    const uint8_t fields[] = {
        value.month, value.day, value.hour, value.minute,
    };
    for (size_t index = 0U; index < 4U; ++index) {
        s_clock_digits[4U + index * 2U] =
            static_cast<char>('0' + fields[index] / 10U);
        s_clock_digits[5U + index * 2U] =
            static_cast<char>('0' + fields[index] % 10U);
    }
    s_clock_length = 12U;
    s_clock_digits[s_clock_length] = '\0';
}

void fill_due_digits(const PregnancyDate &value)
{
    const uint16_t year = static_cast<uint16_t>(value.year % 10000U);
    s_due_digits[0] = static_cast<char>('0' + year / 1000U);
    s_due_digits[1] = static_cast<char>('0' + year / 100U % 10U);
    s_due_digits[2] = static_cast<char>('0' + year / 10U % 10U);
    s_due_digits[3] = static_cast<char>('0' + year % 10U);
    s_due_digits[4] = static_cast<char>('0' + value.month / 10U);
    s_due_digits[5] = static_cast<char>('0' + value.month % 10U);
    s_due_digits[6] = static_cast<char>('0' + value.day / 10U);
    s_due_digits[7] = static_cast<char>('0' + value.day % 10U);
    s_due_length = 8U;
    s_due_digits[s_due_length] = '\0';
}

uint16_t digits_u16(const char *digits, size_t offset, size_t count)
{
    uint16_t value = 0U;
    for (size_t index = 0U; index < count; ++index) {
        value = static_cast<uint16_t>(
            value * 10U + static_cast<uint16_t>(digits[offset + index] - '0'));
    }
    return value;
}

bool parse_clock(StickyRtcDateTime &value)
{
    if (s_clock_length != 12U) {
        return false;
    }
    value.year = digits_u16(s_clock_digits, 0U, 4U);
    value.month = static_cast<uint8_t>(digits_u16(s_clock_digits, 4U, 2U));
    value.day = static_cast<uint8_t>(digits_u16(s_clock_digits, 6U, 2U));
    value.hour = static_cast<uint8_t>(digits_u16(s_clock_digits, 8U, 2U));
    value.minute = static_cast<uint8_t>(digits_u16(s_clock_digits, 10U, 2U));
    value.second = 0U;
    return value.year >= 1970U && value.year <= 2099U &&
           value.month >= 1U && value.month <= 12U &&
           value.day >= 1U && value.day <= 31U &&
           value.hour <= 23U && value.minute <= 59U;
}

bool parse_due_date(PregnancyDate &value)
{
    if (s_due_length != 8U) {
        return false;
    }
    value.year = digits_u16(s_due_digits, 0U, 4U);
    value.month = static_cast<uint8_t>(digits_u16(s_due_digits, 4U, 2U));
    value.day = static_cast<uint8_t>(digits_u16(s_due_digits, 6U, 2U));
    return pregnancy_date_valid(value);
}

bool read_rtc(StickyRtcDateTime &value)
{
    return sticky_rtc_is_ready() && sticky_rtc_read(value) == ESP_OK;
}

bool update_progress_from_clock(bool require_in_range)
{
    StickyRtcDateTime clock = {};
    if (!read_rtc(clock)) {
        return false;
    }
    s_today = {clock.year, clock.month, clock.day};
    int32_t day_index = 0;
    if (!pregnancy_date_to_day_index(s_today, day_index)) {
        return false;
    }
    PregnancyProgress next = {};
    const bool calculated = pregnancy_progress_calculate(
        s_today, s_due_date, next);
    if (!calculated && require_in_range) {
        return false;
    }
    if (calculated) {
        s_progress = next;
    }
    s_last_day_index = day_index;
    fill_clock_digits(clock);
    return calculated;
}

esp_err_t refresh_display(bool partial_refresh)
{
    const esp_err_t result = partial_refresh
                                 ? sticky_display_refresh_partial()
                                 : sticky_display_refresh_monochrome();
    if (result != ESP_OK) {
        STICKY_LOGE(kTag,
                    "pregnancy=display refresh=%s result=%s",
                    partial_refresh ? "partial" : "full",
                    esp_err_to_name(result));
    }
    return result;
}

void render_page(bool partial_refresh)
{
    s_canvas->set_rotation(s_display_rotation);
    switch (s_page) {
    case PregnancyPage::ClockSetup:
        pregnancy_page_render_clock_setup(
            *s_canvas, s_clock_digits, s_input_error, s_configured);
        break;
    case PregnancyPage::DueDateSetup:
        pregnancy_page_render_due_date_setup(
            *s_canvas, s_due_digits, s_input_error, true);
        break;
    case PregnancyPage::Dashboard:
        pregnancy_page_render_dashboard(
            *s_canvas, s_due_date, s_progress);
        break;
    }
    refresh_display(partial_refresh);
}

void open_clock_setup()
{
    StickyRtcDateTime clock = {};
    if (read_rtc(clock)) {
        fill_clock_digits(clock);
    } else {
        s_clock_digits[0] = '\0';
        s_clock_length = 0U;
    }
    s_page = PregnancyPage::ClockSetup;
    s_input_error = false;
    s_replace_on_digit = s_clock_length > 0U;
}

void open_due_date_setup()
{
    if (s_configured) {
        fill_due_digits(s_due_date);
    } else {
        s_due_digits[0] = '\0';
        s_due_length = 0U;
    }
    s_page = PregnancyPage::DueDateSetup;
    s_input_error = false;
    s_replace_on_digit = s_due_length > 0U;
}

void append_digit(char *buffer,
                  size_t &length,
                  size_t maximum,
                  char digit)
{
    if (s_replace_on_digit) {
        length = 0U;
        buffer[0] = '\0';
        s_replace_on_digit = false;
    }
    if (length >= maximum) {
        return;
    }
    buffer[length++] = digit;
    buffer[length] = '\0';
}

void delete_digit(char *buffer, size_t &length)
{
    if (s_replace_on_digit) {
        length = 0U;
        buffer[0] = '\0';
        s_replace_on_digit = false;
        return;
    }
    if (length > 0U) {
        buffer[--length] = '\0';
    }
}

bool apply_clock()
{
    StickyRtcDateTime value = {};
    if (!parse_clock(value) || sticky_rtc_write(value) != ESP_OK) {
        s_input_error = true;
        return true;
    }
    s_today = {value.year, value.month, value.day};
    int32_t day_index = 0;
    pregnancy_date_to_day_index(s_today, day_index);
    s_last_day_index = day_index;
    open_due_date_setup();
    return true;
}

bool apply_due_date()
{
    PregnancyDate value = {};
    PregnancyProgress progress = {};
    if (!parse_due_date(value) ||
        !pregnancy_progress_calculate(s_today, value, progress)) {
        s_input_error = true;
        return true;
    }
    const esp_err_t save_result = pregnancy_storage_save(value);
    if (save_result != ESP_OK) {
        s_input_error = true;
        STICKY_LOGE(kTag,
                    "pregnancy=setup save=%s result=failed",
                    esp_err_to_name(save_result));
        return true;
    }
    s_due_date = value;
    s_progress = progress;
    s_configured = true;
    s_page = PregnancyPage::Dashboard;
    s_input_error = false;
    s_next_rtc_poll_us = esp_timer_get_time() + kRtcPollIntervalUs;
    return true;
}

bool handle_action(PregnancyAction action)
{
    char digit = '\0';
    if (pregnancy_action_digit(action, digit)) {
        s_input_error = false;
        if (s_page == PregnancyPage::ClockSetup) {
            append_digit(s_clock_digits, s_clock_length, 12U, digit);
        } else if (s_page == PregnancyPage::DueDateSetup) {
            append_digit(s_due_digits, s_due_length, 8U, digit);
        }
        return true;
    }

    switch (action) {
    case PregnancyAction::Delete:
        s_input_error = false;
        if (s_page == PregnancyPage::ClockSetup) {
            delete_digit(s_clock_digits, s_clock_length);
        } else if (s_page == PregnancyPage::DueDateSetup) {
            delete_digit(s_due_digits, s_due_length);
        }
        return true;
    case PregnancyAction::Continue:
        return s_page == PregnancyPage::ClockSetup
                   ? apply_clock()
                   : apply_due_date();
    case PregnancyAction::Back:
        if (s_page == PregnancyPage::DueDateSetup) {
            open_clock_setup();
        } else if (s_configured) {
            update_progress_from_clock(false);
            s_page = PregnancyPage::Dashboard;
        }
        return true;
    case PregnancyAction::Edit:
        open_clock_setup();
        return true;
    case PregnancyAction::None:
    case PregnancyAction::Digit0:
    case PregnancyAction::Digit1:
    case PregnancyAction::Digit2:
    case PregnancyAction::Digit3:
    case PregnancyAction::Digit4:
    case PregnancyAction::Digit5:
    case PregnancyAction::Digit6:
    case PregnancyAction::Digit7:
    case PregnancyAction::Digit8:
    case PregnancyAction::Digit9:
        return false;
    }
    return false;
}

PregnancyAction action_for_press(const StickyTouchPress &press)
{
    int logical_x = 0;
    int logical_y = 0;
    s_canvas->physical_to_logical(
        press.x, press.y, logical_x, logical_y);
    return pregnancy_page_action_at(
        s_page,
        s_page == PregnancyPage::DueDateSetup || s_configured,
        logical_x,
        logical_y);
}

void initialize_state()
{
    const esp_err_t load_result = pregnancy_storage_load(
        s_due_date, s_configured);
    if (load_result != ESP_OK) {
        STICKY_LOGW(kTag,
                    "pregnancy=storage load=%s result=fallback_setup",
                    esp_err_to_name(load_result));
        s_configured = false;
    }

    if (s_configured && update_progress_from_clock(true)) {
        fill_due_digits(s_due_date);
        s_page = PregnancyPage::Dashboard;
        s_next_rtc_poll_us = esp_timer_get_time() + kRtcPollIntervalUs;
        return;
    }
    open_clock_setup();
}

void update_for_date_rollover()
{
    if (s_page != PregnancyPage::Dashboard ||
        esp_timer_get_time() < s_next_rtc_poll_us) {
        return;
    }
    s_next_rtc_poll_us = esp_timer_get_time() + kRtcPollIntervalUs;
    StickyRtcDateTime clock = {};
    if (!read_rtc(clock)) {
        return;
    }
    const PregnancyDate today = {clock.year, clock.month, clock.day};
    int32_t day_index = 0;
    if (!pregnancy_date_to_day_index(today, day_index) ||
        day_index == s_last_day_index) {
        return;
    }
    PregnancyProgress progress = {};
    if (!pregnancy_progress_calculate(today, s_due_date, progress)) {
        return;
    }
    s_today = today;
    s_progress = progress;
    s_last_day_index = day_index;
    render_page(true);
    STICKY_LOGI(kTag,
                "pregnancy=date_rollover week=%u day=%u percent=%u result=updated",
                static_cast<unsigned>(progress.weeks),
                static_cast<unsigned>(progress.days),
                static_cast<unsigned>(progress.percent));
}

void app_task(void *)
{
    sticky_touch_clear_press();
    initialize_state();
    render_page(false);
    STICKY_LOGI(kTag,
                "pregnancy=ready orientation=landscape page=%s configured=%u result=ok",
                pregnancy_page_name(s_page),
                static_cast<unsigned>(s_configured));

    while (true) {
        if (sticky_app_lifecycle_checkpoint(s_lifecycle)) {
            sticky_touch_clear_press();
            if (s_page == PregnancyPage::Dashboard) {
                update_progress_from_clock(false);
            }
            render_page(false);
        }

        StickyTouchPress press = {};
        if (sticky_touch_take_press(press)) {
            const PregnancyAction action = action_for_press(press);
            if (action != PregnancyAction::None) {
                STICKY_LOGI(kTag,
                            "pregnancy=touch page=%s action=%s result=accepted",
                            pregnancy_page_name(s_page),
                            pregnancy_action_name(action));
                if (handle_action(action)) {
                    sticky_touch_clear_press();
                    render_page(true);
                }
            }
        }
        update_for_date_rollover();
        vTaskDelay(kPollInterval);
    }
}

}  // namespace

esp_err_t pregnancy_app_start(Canvas &canvas)
{
    app_log_register_tag(kTag);
    if (s_app_task != nullptr) {
        return ESP_OK;
    }
    s_canvas = &canvas;
    if (xTaskCreate(app_task,
                    "pregnancy_app",
                    kTaskStackSize,
                    nullptr,
                    kTaskPriority,
                    &s_app_task) != pdPASS) {
        s_canvas = nullptr;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void pregnancy_app_set_display_rotation(CanvasRotation rotation)
{
    s_display_rotation = rotation;
}

esp_err_t pregnancy_app_pause()
{
    return sticky_app_lifecycle_pause(s_lifecycle, s_app_task);
}

esp_err_t pregnancy_app_resume()
{
    if (s_app_task == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    sticky_app_lifecycle_resume(s_lifecycle);
    return ESP_OK;
}

esp_err_t pregnancy_app_prepare_power_sleep(
    uint32_t &current_epoch_seconds,
    uint32_t &next_event_epoch_seconds)
{
    current_epoch_seconds = 0U;
    next_event_epoch_seconds = 0U;
    const esp_err_t pause_result = pregnancy_app_pause();
    if (pause_result != ESP_OK) {
        return pause_result;
    }

    StickyRtcDateTime clock = {};
    if (!read_rtc(clock)) {
        return ESP_OK;
    }
    const PregnancyDate today = {clock.year, clock.month, clock.day};
    int32_t day_index = 0;
    if (!pregnancy_date_to_day_index(today, day_index)) {
        return ESP_OK;
    }
    current_epoch_seconds =
        static_cast<uint32_t>(day_index) * 86400U +
        static_cast<uint32_t>(clock.hour) * 3600U +
        static_cast<uint32_t>(clock.minute) * 60U + clock.second;
    next_event_epoch_seconds =
        static_cast<uint32_t>(day_index + 1) * 86400U + 60U;
    return ESP_OK;
}

uint32_t pregnancy_app_power_sleep_timeout_ms()
{
    return s_page == PregnancyPage::Dashboard
               ? kDashboardSleepTimeoutMs
               : 0U;
}
