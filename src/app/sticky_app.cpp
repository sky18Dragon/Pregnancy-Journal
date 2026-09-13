#include "sticky_app.h"

#include <cstdint>

#include "app_log.h"
#include "app_manager.h"
#include "app_pages.h"
#include "app_registry.h"
#include "board_charger.h"
#include "board_power.h"
#include "canvas.h"
#include "checkup_storage.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "scheduler.h"
#include "reminder_storage.h"
#include "pregnancy_app.h"
#include "settings_app.h"
#include "settings_store.h"
#include "sticky_app_gesture.h"
#include "sticky_buzzer.h"
#include "sticky_button.h"
#include "sticky_display.h"
#include "sticky_touch.h"
#include "sticky_rtc.h"
#include "ui_language.h"

namespace {

constexpr char kTag[] = "sticky_app";
constexpr TickType_t kPollInterval = pdMS_TO_TICKS(40);
constexpr uint32_t kTaskStackSize = 4608U;
constexpr UBaseType_t kTaskPriority = 4U;
constexpr uint32_t kSleepContextMagic = 0x53434657U;
constexpr uint32_t kLauncherIdleCloseMs = 30000U;
constexpr uint32_t kScheduledWakeLeadSeconds = 15U;

struct StickySleepContext {
    uint32_t magic;
    StickyAppId app;
    CanvasRotation rotation;
};

RTC_NOINIT_ATTR StickySleepContext s_sleep_context;
Canvas *s_canvas = nullptr;
TaskHandle_t s_task = nullptr;
StickyAppManager s_manager(sticky_app_registry_data(),
                           sticky_app_registry_count(),
                           StickyAppId::Pregnancy);
StickyScheduler s_scheduler;
bool s_launcher_open = false;
uint32_t s_last_activity_ms = 0U;

bool scheduled_alert_due(uint32_t now)
{
    const uint32_t lower = now > 60U ? now - 60U : 0U;
    const uint32_t upper = now + 30U;
    ReminderService reminders;
    if (reminder_storage_load(reminders) == ESP_OK) {
        for (size_t index = 0U; index < reminders.count(); ++index) {
            const Reminder *item = reminders.at(index);
            uint32_t epoch = 0U;
            if (item != nullptr && !item->completed &&
                reminder_epoch(*item, epoch) && epoch >= lower && epoch <= upper)
                return true;
        }
    }
    CheckupService checkups;
    if (checkup_storage_load(checkups) == ESP_OK) {
        for (size_t index = 0U; index < checkups.count(); ++index) {
            const Checkup *item = checkups.at(index);
            uint32_t epoch = 0U;
            if (item != nullptr && !item->completed &&
                checkup_epoch(*item, epoch) && epoch >= lower && epoch <= upper)
                return true;
        }
    }
    return false;
}

void persist_current_app()
{
    StickyDeviceSettings settings = {};
    if (sticky_settings_load(settings) != ESP_OK) return;
    settings.last_app = s_manager.current_id();
    sticky_settings_save(settings);
}

void render_launcher()
{
    app_page_render_launcher(*s_canvas, s_manager.current_id());
    const esp_err_t result = sticky_display_refresh_partial();
    if (result != ESP_OK) {
        STICKY_LOGE(kTag, "launcher=render result=%s", esp_err_to_name(result));
    }
}

bool open_launcher()
{
    if (s_launcher_open) return true;
    const esp_err_t result = s_manager.pause_current();
    if (result != ESP_OK) return false;
    sticky_touch_clear_press();
    sticky_touch_clear_interaction();
    s_launcher_open = true;
    render_launcher();
    STICKY_LOGI(kTag, "launcher=open current=%s result=ok",
                sticky_app_id_name(s_manager.current_id()));
    return true;
}

void close_launcher()
{
    if (!s_launcher_open) return;
    s_launcher_open = false;
    sticky_touch_clear_press();
    sticky_touch_clear_interaction();
    const esp_err_t result = s_manager.resume_current();
    STICKY_LOGI(kTag, "launcher=close current=%s result=%s",
                sticky_app_id_name(s_manager.current_id()),
                esp_err_to_name(result));
}

void select_app(StickyAppId app, const char *source)
{
    const StickyAppId previous = s_manager.current_id();
    const esp_err_t result = s_launcher_open
                                 ? s_manager.switch_from_paused_to(app)
                                 : s_manager.switch_to(app);
    if (result != ESP_OK) {
        STICKY_LOGE(kTag, "app=switch from=%s to=%s source=%s result=%s",
                    sticky_app_id_name(previous), sticky_app_id_name(app),
                    source, esp_err_to_name(result));
        return;
    }
    s_launcher_open = false;
    sticky_touch_clear_press();
    sticky_touch_clear_interaction();
    persist_current_app();
    STICKY_LOGI(kTag, "app=switch from=%s to=%s source=%s result=ok",
                sticky_app_id_name(previous),
                sticky_app_id_name(s_manager.current_id()), source);
}

void draw_sleep_indicator()
{
    const int left = static_cast<int>(s_canvas->width()) - 45;
    constexpr int top = 10;
    s_canvas->fill_rect(left, top, 35, 35, GrayLevel::White);
    s_canvas->fill_circle(left + 16, top + 17, 11, GrayLevel::Black);
    s_canvas->fill_circle(left + 21, top + 12, 10, GrayLevel::White);
}

void enter_sleep(const char *source)
{
    const StickyAppDescriptor *app = s_manager.current();
    if (app == nullptr || !app->sleep_allowed()) return;
    if (s_launcher_open) {
        s_launcher_open = false;
    }
    uint32_t now = 0U;
    uint32_t next = 0U;
    const esp_err_t prepare = app->prepare_sleep(now, next);
    if (prepare != ESP_OK) return;

    if (now == 0U) {
        StickyRtcDateTime clock = {};
        if (sticky_rtc_read(clock) != ESP_OK ||
            !sticky_rtc_epoch_seconds(clock, now)) return;
    }

    s_scheduler.cancel(1U);
    s_scheduler.cancel(2U);
    s_scheduler.cancel(3U);
    s_scheduler.cancel(4U);
    const uint32_t day_start = now / 86400U * 86400U;
    uint32_t daily_refresh = day_start + 3U * 3600U;
    if (daily_refresh <= now) daily_refresh += 86400U;
    s_scheduler.schedule(1U, daily_refresh,
                         StickyScheduledEventType::DailyRefresh);
    if (next > now)
        s_scheduler.schedule(4U, next, StickyScheduledEventType::System);
    uint32_t reminder_epoch = 0U;
    if (reminder_storage_next_event(now, reminder_epoch)) {
        s_scheduler.schedule(2U, reminder_epoch,
                             StickyScheduledEventType::Reminder);
    }
    uint32_t checkup_epoch = 0U;
    if (checkup_storage_next_event(now, checkup_epoch)) {
        s_scheduler.schedule(3U, checkup_epoch,
                             StickyScheduledEventType::Checkup);
    }
    StickyScheduledEvent event = {};
    uint64_t wake_us = 0U;
    if (s_scheduler.next(now, event)) {
        uint32_t wake_epoch = event.epoch_seconds > kScheduledWakeLeadSeconds
                                  ? event.epoch_seconds - kScheduledWakeLeadSeconds
                                  : event.epoch_seconds;
        if (wake_epoch <= now) wake_epoch = now + 1U;
        wake_us = static_cast<uint64_t>(wake_epoch - now) * 1000000ULL;
    }

    s_sleep_context = {kSleepContextMagic, s_manager.current_id(),
                       s_canvas->rotation()};
    sticky_display_set_battery_overlay_sleep_layout(true);
    draw_sleep_indicator();
    sticky_display_cancel_app_transition_refresh();
    sticky_display_refresh_monochrome();
    sticky_buzzer_stop();
    sticky_touch_stop();
    sticky_display_sleep();
    STICKY_LOGI(kTag, "power=sleep source=%s app=%s wake_us=%llu result=ready",
                source, sticky_app_id_name(s_manager.current_id()),
                static_cast<unsigned long long>(wake_us));
    board_power_enter_deep_sleep(wake_us);
}

void handle_interaction(const StickyTouchInteraction &interaction)
{
    int sx = 0, sy = 0, ex = 0, ey = 0;
    s_canvas->physical_to_logical(interaction.start_x, interaction.start_y, sx, sy);
    s_canvas->physical_to_logical(interaction.end_x, interaction.end_y, ex, ey);
    const StickyAppGestureSample sample = {
        static_cast<int>(s_canvas->width()), static_cast<int>(s_canvas->height()),
        sx, sy, ex, ey, interaction.ended_at_ms - interaction.started_at_ms,
        s_launcher_open,
    };
    const StickyAppGestureAction action = sticky_app_gesture_classify(sample);
    if (action == StickyAppGestureAction::OpenLauncher) open_launcher();
    else if (action == StickyAppGestureAction::CloseLauncher) close_launcher();
}

void handle_launcher_press(const StickyTouchPress &press)
{
    int x = 0, y = 0;
    s_canvas->physical_to_logical(press.x, press.y, x, y);
    if (app_page_launcher_language_at(s_canvas->width(), s_canvas->height(), x, y)) {
        StickyDeviceSettings settings = {};
        if (sticky_settings_load(settings) == ESP_OK) {
            settings.language = ui_language_is_chinese()
                                    ? UiLanguage::English
                                    : UiLanguage::ChineseSimplified;
            if (sticky_settings_save(settings) == ESP_OK) {
                ui_language_set(settings.language);
                render_launcher();
            }
        }
        return;
    }
    StickyAppId selected = StickyAppId::Pregnancy;
    if (app_page_launcher_app_at(s_canvas->width(), s_canvas->height(),
                                 x, y, selected)) {
        select_app(selected, "touch");
    }
}

void app_task(void *)
{
    while (true) {
        StickyButtonEvent event = StickyButtonEvent::None;
        if (sticky_button_take_event(event)) {
            s_last_activity_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000LL);
            sticky_buzzer_stop();
            if (event == StickyButtonEvent::SingleClick) {
                if (s_launcher_open) close_launcher(); else open_launcher();
            } else if (event == StickyButtonEvent::DoubleClick) {
                select_app(StickyAppId::Pregnancy, "double_click");
            } else if (event == StickyButtonEvent::SleepChord) {
                enter_sleep("side_button_chord");
            }
        }

        StickyTouchInteraction interaction = {};
        while (sticky_touch_take_interaction(interaction)) {
            s_last_activity_ms = interaction.ended_at_ms;
            sticky_buzzer_stop();
            handle_interaction(interaction);
        }
        if (s_launcher_open) {
            StickyTouchPress press = {};
            if (sticky_touch_take_press(press)) {
                s_last_activity_ms = press.captured_at_ms;
                handle_launcher_press(press);
            }
        } else if (s_manager.current_id() == StickyAppId::Settings &&
                   settings_app_take_home_request()) {
            select_app(StickyAppId::Pregnancy, "settings");
        } else if (s_manager.current_id() == StickyAppId::Settings &&
                   settings_app_take_pregnancy_request()) {
            pregnancy_app_request_setup();
            select_app(StickyAppId::Pregnancy, "settings");
        }

        const uint32_t now_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000LL);
        if (s_launcher_open && now_ms - s_last_activity_ms >= kLauncherIdleCloseMs) {
            close_launcher();
        }
        const StickyAppDescriptor *app = s_manager.current();
        if (!s_launcher_open && app != nullptr && app->sleep_timeout_ms() > 0U &&
            app->sleep_allowed() && !board_charger_external_power_present() &&
            now_ms - s_last_activity_ms >= app->sleep_timeout_ms()) {
            enter_sleep("idle_timeout");
        }
        vTaskDelay(kPollInterval);
    }
}

}  // namespace

esp_err_t sticky_app_start(Canvas &canvas)
{
    app_log_register_tag(kTag);
    if (s_task != nullptr) return ESP_OK;
    s_canvas = &canvas;
    esp_err_t result = sticky_button_init();
    if (result != ESP_OK) return result;

    const bool valid_context = s_sleep_context.magic == kSleepContextMagic &&
        sticky_app_registry_find(s_sleep_context.app) != nullptr;
    StickyAppId initial = valid_context ? s_sleep_context.app
                                        : StickyAppId::Pregnancy;
    bool timer_alert_due = false;
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
        StickyRtcDateTime clock = {};
        uint32_t now = 0U;
        if (sticky_rtc_read(clock) == ESP_OK &&
            sticky_rtc_epoch_seconds(clock, now)) {
            timer_alert_due = scheduled_alert_due(now);
            const bool daily_refresh_window =
                (clock.hour == 2U && clock.minute >= 59U) ||
                (clock.hour == 3U && clock.minute <= 2U);
            if (!timer_alert_due && daily_refresh_window)
                initial = StickyAppId::Pregnancy;
        }
    }
    s_sleep_context.magic = 0U;
    result = s_manager.start(canvas, initial);
    if (result != ESP_OK) return result;
    if (timer_alert_due) {
        sticky_buzzer_start_alarm();
        STICKY_LOGI(kTag, "scheduler=wake alert=due result=started");
    }
    s_last_activity_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000LL);
    if (xTaskCreate(app_task, "app_coordinator", kTaskStackSize, nullptr,
                    kTaskPriority, &s_task) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    STICKY_LOGI(kTag, "runtime=ready apps=%u default=pregnancy current=%s result=ok",
                static_cast<unsigned>(sticky_app_registry_count()),
                sticky_app_id_name(initial));
    return ESP_OK;
}
