#include "settings_app.h"

#include <cstdio>
#include <cstring>

#include "app_log.h"
#include "canvas.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "settings_pages.h"
#include "settings_store.h"
#include "sticky_app_lifecycle.h"
#include "sticky_display.h"
#include "sticky_rtc.h"
#include "sticky_touch.h"
#include "ui_language.h"

namespace {

constexpr char kTag[] = "settings_app";
constexpr uint32_t kSleepTimeoutMs = 3U * 60U * 1000U;
Canvas *s_canvas = nullptr;
TaskHandle_t s_task = nullptr;
StickyAppLifecycle s_lifecycle = {};
SettingsPage s_page = SettingsPage::Main;
char s_digits[13] = {};
size_t s_length = 0U;
bool s_error = false;
bool s_home_requested = false;

void fill_digits(const StickyRtcDateTime &value)
{
    const uint16_t year = value.year;
    s_digits[0] = static_cast<char>('0' + year / 1000U % 10U);
    s_digits[1] = static_cast<char>('0' + year / 100U % 10U);
    s_digits[2] = static_cast<char>('0' + year / 10U % 10U);
    s_digits[3] = static_cast<char>('0' + year % 10U);
    const uint8_t fields[] = {value.month, value.day, value.hour, value.minute};
    for (size_t index = 0U; index < 4U; ++index) {
        s_digits[4U + index * 2U] =
            static_cast<char>('0' + fields[index] / 10U);
        s_digits[5U + index * 2U] =
            static_cast<char>('0' + fields[index] % 10U);
    }
    s_digits[12] = '\0';
    s_length = 12U;
}

unsigned parse(size_t offset, size_t length)
{
    unsigned value = 0U;
    for (size_t index = 0U; index < length; ++index) {
        value = value * 10U + static_cast<unsigned>(s_digits[offset + index] - '0');
    }
    return value;
}

void render(bool full = false)
{
    if (s_page == SettingsPage::Main) {
        settings_page_render_main(*s_canvas, sticky_rtc_is_ready());
    } else {
        settings_page_render_time_editor(*s_canvas, s_digits, s_error);
    }
    if (full) sticky_display_refresh_monochrome();
    else sticky_display_refresh_partial();
}

void open_editor()
{
    StickyRtcDateTime now = {};
    if (sticky_rtc_read(now) == ESP_OK) fill_digits(now);
    else { s_digits[0] = '\0'; s_length = 0U; }
    s_error = false;
    s_page = SettingsPage::TimeEditor;
}

void save_time()
{
    if (s_length != 12U) { s_error = true; return; }
    StickyRtcDateTime value = {};
    value.year = static_cast<uint16_t>(parse(0U, 4U));
    value.month = static_cast<uint8_t>(parse(4U, 2U));
    value.day = static_cast<uint8_t>(parse(6U, 2U));
    value.hour = static_cast<uint8_t>(parse(8U, 2U));
    value.minute = static_cast<uint8_t>(parse(10U, 2U));
    if (sticky_rtc_write(value) != ESP_OK) { s_error = true; return; }
    s_page = SettingsPage::Main;
    s_error = false;
}

void handle(SettingsAction action)
{
    char digit = 0;
    if (settings_action_digit(action, digit)) {
        if (s_length >= 12U) s_length = 0U;
        s_digits[s_length++] = digit;
        s_digits[s_length] = '\0';
        s_error = false;
        render();
        return;
    }
    switch (action) {
    case SettingsAction::ToggleLanguage: {
        StickyDeviceSettings settings = {};
        sticky_settings_load(settings);
        settings.language = ui_language_is_chinese()
                                ? UiLanguage::English
                                : UiLanguage::ChineseSimplified;
        if (sticky_settings_save(settings) == ESP_OK) {
            ui_language_set(settings.language);
        }
        render(true);
        break;
    }
    case SettingsAction::EditTime: open_editor(); render(); break;
    case SettingsAction::RefreshDisplay: render(true); break;
    case SettingsAction::Back:
        if (s_page == SettingsPage::TimeEditor) { s_page = SettingsPage::Main; render(); }
        else s_home_requested = true;
        break;
    case SettingsAction::Delete:
        if (s_length > 0U) s_digits[--s_length] = '\0';
        s_error = false; render(); break;
    case SettingsAction::Save: save_time(); render(true); break;
    default: break;
    }
}

void task(void *)
{
    render(true);
    while (true) {
        if (sticky_app_lifecycle_checkpoint(s_lifecycle)) render(true);
        StickyTouchPress press = {};
        if (sticky_touch_take_press(press)) {
            int x = 0, y = 0;
            s_canvas->physical_to_logical(press.x, press.y, x, y);
            handle(settings_page_action_at(s_page, x, y));
        }
        vTaskDelay(pdMS_TO_TICKS(40));
    }
}

}  // namespace

esp_err_t settings_app_start(Canvas &canvas)
{
    app_log_register_tag(kTag);
    if (s_task != nullptr) return ESP_OK;
    s_canvas = &canvas;
    return xTaskCreate(task, "settings", 4608U, nullptr, 3U, &s_task) == pdPASS
               ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t settings_app_pause() { return sticky_app_lifecycle_pause(s_lifecycle, s_task); }
esp_err_t settings_app_resume() { sticky_app_lifecycle_resume(s_lifecycle); return ESP_OK; }
esp_err_t settings_app_prepare_power_sleep(uint32_t &current, uint32_t &next)
{ current = 0U; next = 0U; return settings_app_pause(); }
uint32_t settings_app_power_sleep_timeout_ms() { return kSleepTimeoutMs; }
bool settings_app_power_sleep_allowed() { return s_page == SettingsPage::Main; }
bool settings_app_take_home_request()
{ const bool value = s_home_requested; s_home_requested = false; return value; }
