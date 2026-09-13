#include "pregnancy_app.h"

#include <atomic>
#include <cstdio>

#include "app_log.h"
#include "content_service.h"
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

constexpr char kTag[] = "PREGNANCY";
constexpr TickType_t kPollInterval = pdMS_TO_TICKS(40);
constexpr uint32_t kTaskStackSize = 5120U;
constexpr UBaseType_t kTaskPriority = 3U;
constexpr uint32_t kDashboardSleepTimeoutMs = 60U * 1000U;
constexpr int64_t kRtcPollIntervalUs = 60LL * 1000000LL;

Canvas *s_canvas = nullptr;
TaskHandle_t s_app_task = nullptr;
StickyAppLifecycle s_lifecycle = {};
CanvasRotation s_display_rotation = CanvasRotation::Deg0;
PregnancyPage s_page = PregnancyPage::ClockSetup;
PregnancyDate s_today = {};
PregnancyProfile s_profile = {};
PregnancyProgress s_progress = {};
bool s_configured = false;
bool s_due_date_primary = true;
bool s_input_error = false;
bool s_replace_on_digit = false;
char s_clock_digits[13] = {};
size_t s_clock_length = 0U;
char s_date_digits[9] = {};
size_t s_date_length = 0U;
int32_t s_last_day_index = -1;
int64_t s_next_rtc_poll_us = 0;
std::atomic<bool> s_setup_requested{false};

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

void fill_date_digits(const PregnancyDate &value)
{
    const uint16_t year = static_cast<uint16_t>(value.year % 10000U);
    s_date_digits[0] = static_cast<char>('0' + year / 1000U);
    s_date_digits[1] = static_cast<char>('0' + year / 100U % 10U);
    s_date_digits[2] = static_cast<char>('0' + year / 10U % 10U);
    s_date_digits[3] = static_cast<char>('0' + year % 10U);
    s_date_digits[4] = static_cast<char>('0' + value.month / 10U);
    s_date_digits[5] = static_cast<char>('0' + value.month % 10U);
    s_date_digits[6] = static_cast<char>('0' + value.day / 10U);
    s_date_digits[7] = static_cast<char>('0' + value.day % 10U);
    s_date_length = 8U;
    s_date_digits[s_date_length] = '\0';
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
    if (s_clock_length != 12U) return false;
    value.year = digits_u16(s_clock_digits, 0U, 4U);
    value.month = static_cast<uint8_t>(digits_u16(s_clock_digits, 4U, 2U));
    value.day = static_cast<uint8_t>(digits_u16(s_clock_digits, 6U, 2U));
    value.hour = static_cast<uint8_t>(digits_u16(s_clock_digits, 8U, 2U));
    value.minute = static_cast<uint8_t>(digits_u16(s_clock_digits, 10U, 2U));
    value.second = 0U;
    return pregnancy_date_valid({value.year, value.month, value.day}) &&
           value.hour <= 23U && value.minute <= 59U;
}

bool parse_profile_date(PregnancyDate &value)
{
    if (s_date_length != 8U) return false;
    value.year = digits_u16(s_date_digits, 0U, 4U);
    value.month = static_cast<uint8_t>(digits_u16(s_date_digits, 4U, 2U));
    value.day = static_cast<uint8_t>(digits_u16(s_date_digits, 6U, 2U));
    return pregnancy_date_valid(value);
}

bool read_rtc(StickyRtcDateTime &value)
{
    return sticky_rtc_is_ready() && sticky_rtc_read(value) == ESP_OK;
}

bool update_progress_from_clock()
{
    StickyRtcDateTime clock = {};
    if (!read_rtc(clock)) return false;
    s_today = {clock.year, clock.month, clock.day};
    if (!pregnancy_date_to_day_index(s_today, s_last_day_index)) return false;
    fill_clock_digits(clock);
    return s_configured &&
           pregnancy_progress_calculate(s_today, s_profile, s_progress);
}

void refresh_display(bool partial)
{
    const esp_err_t result = partial ? sticky_display_refresh_partial()
                                     : sticky_display_refresh_monochrome();
    if (result != ESP_OK) {
        STICKY_LOGE(kTag, "display=refresh mode=%s result=%s",
                    partial ? "partial" : "full", esp_err_to_name(result));
    }
}

void render_page(bool partial)
{
    s_canvas->set_rotation(s_display_rotation);
    switch (s_page) {
    case PregnancyPage::ClockSetup:
        pregnancy_page_render_clock_setup(
            *s_canvas, s_clock_digits, s_input_error, s_configured);
        break;
    case PregnancyPage::SourceSetup:
        pregnancy_page_render_source_setup(*s_canvas, s_configured);
        break;
    case PregnancyPage::DateSetup:
        pregnancy_page_render_profile_date_setup(
            *s_canvas, s_date_digits, s_input_error, s_due_date_primary);
        break;
    case PregnancyPage::Dashboard:
        pregnancy_page_render_dashboard(
            *s_canvas, s_profile.estimated_due_date, s_progress);
        break;
    case PregnancyPage::Baby:
    case PregnancyPage::Mom:
        pregnancy_page_render_detail(
            *s_canvas, s_page, s_progress,
            pregnancy_content_for_week(s_progress.weeks));
        break;
    }
    refresh_display(partial);
}

void open_clock_setup()
{
    StickyRtcDateTime clock = {};
    if (read_rtc(clock)) fill_clock_digits(clock);
    else { s_clock_digits[0] = '\0'; s_clock_length = 0U; }
    s_page = PregnancyPage::ClockSetup;
    s_input_error = false;
    s_replace_on_digit = s_clock_length > 0U;
}

void open_source_setup()
{
    s_page = PregnancyPage::SourceSetup;
    s_input_error = false;
    s_replace_on_digit = false;
}

void open_date_setup(bool due_date_primary)
{
    s_due_date_primary = due_date_primary;
    if (s_configured) {
        fill_date_digits(due_date_primary
                             ? s_profile.estimated_due_date
                             : s_profile.last_menstrual_period);
    } else {
        s_date_digits[0] = '\0';
        s_date_length = 0U;
    }
    s_page = PregnancyPage::DateSetup;
    s_input_error = false;
    s_replace_on_digit = s_date_length > 0U;
}

void append_digit(char *buffer, size_t &length, size_t maximum, char digit)
{
    if (s_replace_on_digit) {
        length = 0U;
        buffer[0] = '\0';
        s_replace_on_digit = false;
    }
    if (length < maximum) {
        buffer[length++] = digit;
        buffer[length] = '\0';
    }
}

void delete_digit(char *buffer, size_t &length)
{
    if (s_replace_on_digit) {
        length = 0U;
        buffer[0] = '\0';
        s_replace_on_digit = false;
    } else if (length > 0U) {
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
    pregnancy_date_to_day_index(s_today, s_last_day_index);
    if (s_configured &&
        pregnancy_progress_calculate(s_today, s_profile, s_progress)) {
        s_page = PregnancyPage::Dashboard;
    } else {
        open_source_setup();
    }
    return true;
}

bool apply_profile_date()
{
    PregnancyDate value = {};
    PregnancyProfile profile = {};
    if (!parse_profile_date(value) ||
        !(s_due_date_primary
              ? pregnancy_profile_from_due_date(value, profile)
              : pregnancy_profile_from_lmp(value, profile)) ||
        !pregnancy_progress_calculate(s_today, profile, s_progress) ||
        pregnancy_storage_save(profile) != ESP_OK) {
        s_input_error = true;
        return true;
    }
    s_profile = profile;
    s_configured = true;
    s_page = PregnancyPage::Dashboard;
    s_input_error = false;
    s_next_rtc_poll_us = esp_timer_get_time() + kRtcPollIntervalUs;
    STICKY_LOGI(kTag, "profile=saved source=%s week=%d day=%u result=ok",
                s_due_date_primary ? "due_date" : "lmp",
                static_cast<int>(s_progress.weeks),
                static_cast<unsigned>(s_progress.days));
    return true;
}

bool handle_action(PregnancyAction action)
{
    char digit = '\0';
    if (pregnancy_action_digit(action, digit)) {
        s_input_error = false;
        if (s_page == PregnancyPage::ClockSetup)
            append_digit(s_clock_digits, s_clock_length, 12U, digit);
        else if (s_page == PregnancyPage::DateSetup)
            append_digit(s_date_digits, s_date_length, 8U, digit);
        return true;
    }
    switch (action) {
    case PregnancyAction::Delete:
        s_input_error = false;
        if (s_page == PregnancyPage::ClockSetup)
            delete_digit(s_clock_digits, s_clock_length);
        else if (s_page == PregnancyPage::DateSetup)
            delete_digit(s_date_digits, s_date_length);
        return true;
    case PregnancyAction::Continue:
        return s_page == PregnancyPage::ClockSetup ? apply_clock()
                                                  : apply_profile_date();
    case PregnancyAction::SelectDueDate: open_date_setup(true); return true;
    case PregnancyAction::SelectLmp: open_date_setup(false); return true;
    case PregnancyAction::ShowOverview: s_page = PregnancyPage::Dashboard; return true;
    case PregnancyAction::ShowBaby: s_page = PregnancyPage::Baby; return true;
    case PregnancyAction::ShowMom: s_page = PregnancyPage::Mom; return true;
    case PregnancyAction::Back:
        if (s_page == PregnancyPage::DateSetup) open_source_setup();
        else if (s_configured) {
            update_progress_from_clock();
            s_page = PregnancyPage::Dashboard;
        }
        return true;
    case PregnancyAction::Edit: open_source_setup(); return true;
    default: return false;
    }
}

PregnancyAction action_for_press(const StickyTouchPress &press)
{
    int x = 0, y = 0;
    s_canvas->physical_to_logical(press.x, press.y, x, y);
    return pregnancy_page_action_at(s_page, s_configured, x, y);
}

void initialize_state()
{
    const esp_err_t load = pregnancy_storage_load(s_profile, s_configured);
    if (load != ESP_OK) {
        STICKY_LOGW(kTag, "profile=load result=%s fallback=setup",
                    esp_err_to_name(load));
        s_configured = false;
    }
    StickyRtcDateTime clock = {};
    if (!read_rtc(clock)) {
        open_clock_setup();
        return;
    }
    fill_clock_digits(clock);
    s_today = {clock.year, clock.month, clock.day};
    pregnancy_date_to_day_index(s_today, s_last_day_index);
    if (s_configured &&
        pregnancy_progress_calculate(s_today, s_profile, s_progress)) {
        s_page = PregnancyPage::Dashboard;
        s_next_rtc_poll_us = esp_timer_get_time() + kRtcPollIntervalUs;
    } else {
        open_source_setup();
    }
}

void update_for_date_rollover()
{
    if ((s_page != PregnancyPage::Dashboard &&
         s_page != PregnancyPage::Baby && s_page != PregnancyPage::Mom) ||
        esp_timer_get_time() < s_next_rtc_poll_us) return;
    s_next_rtc_poll_us = esp_timer_get_time() + kRtcPollIntervalUs;
    StickyRtcDateTime clock = {};
    if (!read_rtc(clock)) return;
    const PregnancyDate today = {clock.year, clock.month, clock.day};
    int32_t day_index = 0;
    if (!pregnancy_date_to_day_index(today, day_index) ||
        day_index == s_last_day_index) return;
    s_today = today;
    s_last_day_index = day_index;
    if (pregnancy_progress_calculate(today, s_profile, s_progress)) {
        render_page(true);
        STICKY_LOGI(kTag, "date=rollover week=%d day=%u result=updated",
                    static_cast<int>(s_progress.weeks),
                    static_cast<unsigned>(s_progress.days));
    }
}

void app_task(void *)
{
    sticky_touch_clear_press();
    initialize_state();
    render_page(false);
    while (true) {
        if (sticky_app_lifecycle_checkpoint(s_lifecycle)) {
            sticky_touch_clear_press();
            if (s_setup_requested.exchange(false)) open_source_setup();
            else if (s_configured) update_progress_from_clock();
            render_page(false);
        }
        StickyTouchPress press = {};
        if (sticky_touch_take_press(press)) {
            const PregnancyAction action = action_for_press(press);
            if (handle_action(action)) {
                sticky_touch_clear_press();
                render_page(true);
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
    if (s_app_task != nullptr) return ESP_OK;
    s_canvas = &canvas;
    return xTaskCreate(app_task, "pregnancy_app", kTaskStackSize, nullptr,
                       kTaskPriority, &s_app_task) == pdPASS
               ? ESP_OK : ESP_ERR_NO_MEM;
}

void pregnancy_app_set_display_rotation(CanvasRotation rotation)
{
    s_display_rotation = rotation;
}

void pregnancy_app_request_setup()
{
    s_setup_requested.store(true);
}

esp_err_t pregnancy_app_pause()
{
    return sticky_app_lifecycle_pause(s_lifecycle, s_app_task);
}

esp_err_t pregnancy_app_resume()
{
    if (s_app_task == nullptr) return ESP_ERR_INVALID_STATE;
    sticky_app_lifecycle_resume(s_lifecycle);
    return ESP_OK;
}

esp_err_t pregnancy_app_prepare_power_sleep(uint32_t &current, uint32_t &next)
{
    current = 0U;
    next = 0U;
    const esp_err_t paused = pregnancy_app_pause();
    if (paused != ESP_OK) return paused;
    StickyRtcDateTime clock = {};
    if (!read_rtc(clock)) return ESP_OK;
    const PregnancyDate today = {clock.year, clock.month, clock.day};
    int32_t day_index = 0;
    if (!pregnancy_date_to_day_index(today, day_index)) return ESP_OK;
    current = static_cast<uint32_t>(day_index) * 86400U +
              static_cast<uint32_t>(clock.hour) * 3600U +
              static_cast<uint32_t>(clock.minute) * 60U + clock.second;
    next = static_cast<uint32_t>(day_index + 1) * 86400U + 3U * 3600U;
    return ESP_OK;
}

uint32_t pregnancy_app_power_sleep_timeout_ms()
{
    return s_page == PregnancyPage::Dashboard ||
                   s_page == PregnancyPage::Baby || s_page == PregnancyPage::Mom
               ? kDashboardSleepTimeoutMs : 0U;
}
