#include "pomodoro_app.h"

#include <cstdint>

#include "app_log.h"
#include "canvas.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pomodoro_countdown.h"
#include "pomodoro_pages.h"
#include "pomodoro_render_policy.h"
#include "sticky_buzzer.h"
#include "sticky_display.h"
#include "sticky_touch.h"

namespace {

constexpr char kTag[] = "pomodoro_app";
constexpr uint32_t kDefaultDurationSeconds = 15U * 60U;
constexpr TickType_t kPollInterval = pdMS_TO_TICKS(40);
constexpr uint32_t kTaskStackSize = 6144;
constexpr UBaseType_t kTaskPriority = 3;

Canvas *s_canvas = nullptr;
TaskHandle_t s_app_task = nullptr;

PomodoroPage s_page = PomodoroPage::Setup;
PomodoroPage s_return_page = PomodoroPage::Running;
PomodoroTimeField s_active_field = PomodoroTimeField::Minutes;
uint32_t s_selected_seconds = kDefaultDurationSeconds;
uint32_t s_total_seconds = kDefaultDurationSeconds;
uint8_t s_custom_hours = 0;
uint8_t s_custom_minutes = 15;
uint8_t s_custom_seconds = 0;
bool s_replace_field_on_digit = true;
int64_t s_timer_deadline_us = 0;
int64_t s_paused_remaining_us = 0;
uint32_t s_displayed_remaining_seconds = 0;

void log_refresh_failure(const char *mode, esp_err_t result)
{
    if (result != ESP_OK) {
        STICKY_LOGE(kTag,
                    "pomodoro=display refresh=%s result=%s",
                    mode,
                    esp_err_to_name(result));
    }
}

void render_current_page(
    bool partial_refresh = false,
    PomodoroRenderReason reason = PomodoroRenderReason::InteractiveChange)
{
    switch (s_page) {
    case PomodoroPage::Setup:
        pomodoro_page_render_setup(*s_canvas, s_selected_seconds);
        break;
    case PomodoroPage::CustomTime:
        pomodoro_page_render_custom_time(*s_canvas,
                                         s_custom_hours,
                                         s_custom_minutes,
                                         s_custom_seconds,
                                         s_active_field);
        break;
    case PomodoroPage::Running:
        pomodoro_page_render_timer(*s_canvas,
                                   s_page,
                                   static_cast<uint32_t>(
                                       (s_paused_remaining_us + 999999LL) /
                                       1000000LL),
                                   s_total_seconds);
        break;
    case PomodoroPage::Paused:
        pomodoro_page_render_timer(*s_canvas,
                                   s_page,
                                   static_cast<uint32_t>(
                                       (s_paused_remaining_us + 999999LL) /
                                       1000000LL),
                                   s_total_seconds);
        break;
    case PomodoroPage::EndConfirmation:
        pomodoro_page_render_end_confirmation(
            *s_canvas,
            static_cast<uint32_t>((s_paused_remaining_us + 999999LL) /
                                  1000000LL),
            s_total_seconds);
        break;
    case PomodoroPage::Alarm:
        pomodoro_page_render_alarm(*s_canvas, s_total_seconds);
        break;
    }

    const esp_err_t result = partial_refresh
                                 ? sticky_display_refresh_partial()
                                 : sticky_display_refresh_monochrome();
    log_refresh_failure(partial_refresh ? "partial" : "full", result);
    if (pomodoro_render_clears_pending_touch(reason)) {
        sticky_touch_clear_press();
    }
}

void change_page(PomodoroPage target, const char *reason, bool partial = false)
{
    const PomodoroPage previous = s_page;
    s_page = target;
    STICKY_LOGI(kTag,
                "pomodoro=page from=%s to=%s reason=%s",
                pomodoro_page_name(previous),
                pomodoro_page_name(target),
                reason);
    render_current_page(partial);
}

uint8_t &active_custom_value()
{
    switch (s_active_field) {
    case PomodoroTimeField::Hours:
        return s_custom_hours;
    case PomodoroTimeField::Seconds:
        return s_custom_seconds;
    case PomodoroTimeField::Minutes:
    default:
        return s_custom_minutes;
    }
}

void select_custom_field(PomodoroTimeField field)
{
    s_active_field = field;
    s_replace_field_on_digit = true;
    render_current_page(true);
}

int action_digit(PomodoroAction action)
{
    if (action < PomodoroAction::Digit0 || action > PomodoroAction::Digit9) {
        return -1;
    }
    return static_cast<int>(action) - static_cast<int>(PomodoroAction::Digit0);
}

void enter_custom_digit(uint8_t digit)
{
    uint8_t &value = active_custom_value();
    value = s_replace_field_on_digit
                ? digit
                : static_cast<uint8_t>((value % 10U) * 10U + digit);
    s_replace_field_on_digit = false;
    STICKY_LOGD(kTag,
                "pomodoro=custom_input field=%d value=%u",
                static_cast<int>(s_active_field),
                static_cast<unsigned>(value));
    render_current_page(true);
}

uint32_t custom_duration_seconds()
{
    return static_cast<uint32_t>(s_custom_hours) * 3600U +
           static_cast<uint32_t>(s_custom_minutes) * 60U +
           static_cast<uint32_t>(s_custom_seconds);
}

bool custom_time_is_valid()
{
    return s_custom_minutes < 60U && s_custom_seconds < 60U &&
           custom_duration_seconds() > 0U;
}

int64_t timer_remaining_us(int64_t now_us)
{
    return s_timer_deadline_us > now_us ? s_timer_deadline_us - now_us : 0;
}

uint32_t remaining_seconds_rounded_up(int64_t remaining_us)
{
    return static_cast<uint32_t>((remaining_us + 999999LL) / 1000000LL);
}

void start_timer()
{
    s_total_seconds = s_selected_seconds;
    s_paused_remaining_us = static_cast<int64_t>(s_total_seconds) * 1000000LL;
    s_timer_deadline_us = esp_timer_get_time() + s_paused_remaining_us;
    s_displayed_remaining_seconds = s_total_seconds;
    STICKY_LOGI(kTag,
                "pomodoro=timer state=started duration_s=%lu",
                static_cast<unsigned long>(s_total_seconds));
    change_page(PomodoroPage::Running, "start_focus");
}

void pause_timer(const char *reason)
{
    s_paused_remaining_us = timer_remaining_us(esp_timer_get_time());
    STICKY_LOGI(kTag,
                "pomodoro=timer state=paused remaining_s=%lu reason=%s",
                static_cast<unsigned long>(
                    remaining_seconds_rounded_up(s_paused_remaining_us)),
                reason);
}

void resume_timer()
{
    s_timer_deadline_us = esp_timer_get_time() + s_paused_remaining_us;
    s_displayed_remaining_seconds =
        remaining_seconds_rounded_up(s_paused_remaining_us);
    STICKY_LOGI(kTag,
                "pomodoro=timer state=resumed remaining_s=%lu",
                static_cast<unsigned long>(
                    remaining_seconds_rounded_up(s_paused_remaining_us)));
    change_page(PomodoroPage::Running, "resume");
}

void return_to_setup(const char *reason)
{
    // Keeps the most recently selected duration ready for the next session.
    // 保留最近一次选择的时长，供下一轮专注直接使用。
    s_total_seconds = s_selected_seconds;
    s_timer_deadline_us = 0;
    s_paused_remaining_us = 0;
    s_displayed_remaining_seconds = 0;
    change_page(PomodoroPage::Setup, reason);
}

void enter_alarm()
{
    s_paused_remaining_us = 0;
    STICKY_LOGI(kTag, "pomodoro=timer state=completed");
    const esp_err_t buzzer_result = sticky_buzzer_start_alarm();
    if (buzzer_result != ESP_OK) {
        STICKY_LOGW(kTag,
                    "pomodoro=alarm buzzer=failed result=%s",
                    esp_err_to_name(buzzer_result));
    }
    change_page(PomodoroPage::Alarm, "timer_completed");
}

void handle_setup_action(PomodoroAction action)
{
    uint32_t selected = 0;
    switch (action) {
    case PomodoroAction::Preset10Seconds:
        selected = 10;
        break;
    case PomodoroAction::Preset30Seconds:
        selected = 30;
        break;
    case PomodoroAction::Preset1Minute:
        selected = 60;
        break;
    case PomodoroAction::Preset3Minutes:
        selected = 180;
        break;
    case PomodoroAction::Preset5Minutes:
        selected = 300;
        break;
    case PomodoroAction::Preset15Minutes:
        selected = 900;
        break;
    case PomodoroAction::OpenCustomTime:
        change_page(PomodoroPage::CustomTime, "open_custom_time");
        return;
    case PomodoroAction::StartFocus:
        start_timer();
        return;
    default:
        return;
    }

    s_selected_seconds = selected;
    STICKY_LOGI(kTag,
                "pomodoro=duration source=preset value_s=%lu",
                static_cast<unsigned long>(selected));
    render_current_page(true);
}

void handle_custom_action(PomodoroAction action)
{
    const int digit = action_digit(action);
    if (digit >= 0) {
        enter_custom_digit(static_cast<uint8_t>(digit));
        return;
    }

    switch (action) {
    case PomodoroAction::SelectHours:
        select_custom_field(PomodoroTimeField::Hours);
        break;
    case PomodoroAction::SelectMinutes:
        select_custom_field(PomodoroTimeField::Minutes);
        break;
    case PomodoroAction::SelectSeconds:
        select_custom_field(PomodoroTimeField::Seconds);
        break;
    case PomodoroAction::Clear:
        s_custom_hours = 0;
        s_custom_minutes = 0;
        s_custom_seconds = 0;
        s_active_field = PomodoroTimeField::Minutes;
        s_replace_field_on_digit = true;
        STICKY_LOGI(kTag, "pomodoro=custom_input action=clear");
        render_current_page(true);
        break;
    case PomodoroAction::Delete: {
        uint8_t &value = active_custom_value();
        value = static_cast<uint8_t>(value / 10U);
        s_replace_field_on_digit = false;
        STICKY_LOGD(kTag,
                    "pomodoro=custom_input action=delete field=%d value=%u",
                    static_cast<int>(s_active_field),
                    static_cast<unsigned>(value));
        render_current_page(true);
        break;
    }
    case PomodoroAction::UseCustomTime:
        if (!custom_time_is_valid()) {
            STICKY_LOGW(kTag,
                        "pomodoro=custom_input result=invalid hours=%u minutes=%u seconds=%u",
                        static_cast<unsigned>(s_custom_hours),
                        static_cast<unsigned>(s_custom_minutes),
                        static_cast<unsigned>(s_custom_seconds));
            return;
        }
        s_selected_seconds = custom_duration_seconds();
        STICKY_LOGI(kTag,
                    "pomodoro=duration source=custom value_s=%lu",
                    static_cast<unsigned long>(s_selected_seconds));
        change_page(PomodoroPage::Setup, "custom_time_accepted");
        break;
    case PomodoroAction::Back:
        change_page(PomodoroPage::Setup, "custom_time_back");
        break;
    default:
        break;
    }
}

void handle_action(PomodoroAction action)
{
    if (action == PomodoroAction::None) {
        return;
    }

    if (s_page == PomodoroPage::Setup) {
        handle_setup_action(action);
    } else if (s_page == PomodoroPage::CustomTime) {
        handle_custom_action(action);
    } else if (s_page == PomodoroPage::Running) {
        if (action == PomodoroAction::Pause) {
            pause_timer("pause_button");
            change_page(PomodoroPage::Paused, "pause");
        } else if (action == PomodoroAction::EndSession) {
            pause_timer("end_confirmation");
            s_return_page = PomodoroPage::Running;
            change_page(PomodoroPage::EndConfirmation, "end_session");
        }
    } else if (s_page == PomodoroPage::Paused) {
        if (action == PomodoroAction::Resume) {
            resume_timer();
        } else if (action == PomodoroAction::EndSession) {
            s_return_page = PomodoroPage::Paused;
            change_page(PomodoroPage::EndConfirmation, "end_session");
        }
    } else if (s_page == PomodoroPage::EndConfirmation) {
        if (action == PomodoroAction::KeepSession) {
            if (s_return_page == PomodoroPage::Running) {
                resume_timer();
            } else {
                change_page(PomodoroPage::Paused, "keep_session");
            }
        } else if (action == PomodoroAction::EndNow) {
            STICKY_LOGI(kTag, "pomodoro=timer state=ended_early");
            return_to_setup("end_now");
        }
    } else if (s_page == PomodoroPage::Alarm &&
               action == PomodoroAction::EndAlarm) {
        const esp_err_t buzzer_result = sticky_buzzer_stop();
        if (buzzer_result != ESP_OK) {
            STICKY_LOGW(kTag,
                        "pomodoro=alarm stop=failed result=%s",
                        esp_err_to_name(buzzer_result));
        }
        return_to_setup("alarm_ended");
    }
}

void update_running_timer()
{
    const int64_t remaining_us = timer_remaining_us(esp_timer_get_time());
    if (remaining_us == 0) {
        enter_alarm();
        return;
    }

    s_paused_remaining_us = remaining_us;
    const uint32_t remaining_seconds =
        remaining_seconds_rounded_up(remaining_us);

    // Every duration renders each visible second through partial fast refresh.
    // 所有时长都通过局部快刷显示每一秒的变化。
    if (pomodoro_countdown_should_render_frame(
            s_displayed_remaining_seconds, remaining_seconds)) {
        s_displayed_remaining_seconds = remaining_seconds;
#if STICKY_LOG_TIMER_TICKS_ENABLED
        STICKY_LOGD(kTag,
                    "pomodoro=timer state=checkpoint cadence=second remaining_s=%lu",
                    static_cast<unsigned long>(remaining_seconds));
#endif
        render_current_page(true, PomodoroRenderReason::CountdownTick);
    }
}

void app_task(void *)
{
    render_current_page(false);
    STICKY_LOGI(kTag,
                "pomodoro=ready page=setup default_duration_s=%lu result=ok",
                static_cast<unsigned long>(kDefaultDurationSeconds));

    while (true) {
        if (s_page == PomodoroPage::Running) {
            update_running_timer();
        }

        StickyTouchPress press = {};
        if (sticky_touch_take_press(press)) {
            int logical_x = 0;
            int logical_y = 0;
            s_canvas->physical_to_logical(
                press.x, press.y, logical_x, logical_y);
            const PomodoroAction action =
                pomodoro_page_action_at(s_page, logical_x, logical_y);
            if (action != PomodoroAction::None) {
                STICKY_LOGI(kTag,
                            "pomodoro=touch page=%s action=%s physical_x=%u physical_y=%u logical_x=%d logical_y=%d",
                            pomodoro_page_name(s_page),
                            pomodoro_action_name(action),
                            static_cast<unsigned>(press.x),
                            static_cast<unsigned>(press.y),
                            logical_x,
                            logical_y);
                handle_action(action);
            }
        }

        vTaskDelay(kPollInterval);
    }
}

}  // namespace

esp_err_t pomodoro_app_start(Canvas &canvas)
{
    app_log_register_tag(kTag);
    if (s_app_task != nullptr) {
        return ESP_OK;
    }

    s_canvas = &canvas;
    if (xTaskCreate(app_task,
                    "pomodoro_app",
                    kTaskStackSize,
                    nullptr,
                    kTaskPriority,
                    &s_app_task) != pdPASS) {
        s_canvas = nullptr;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
