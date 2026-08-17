#include "sticky_app.h"

#include <cstdint>

#include "app_log.h"
#include "app_pages.h"
#include "canvas.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sticky_display.h"
#include "sticky_imu.h"
#include "sticky_touch.h"

namespace {

constexpr char kTag[] = "sticky_app";
constexpr TickType_t kPollInterval = pdMS_TO_TICKS(50);
constexpr TickType_t kConfirmationDuration = pdMS_TO_TICKS(10000);
constexpr TickType_t kPomodoroDuration = pdMS_TO_TICKS(15 * 60 * 1000);
constexpr TickType_t kTimerRefreshInterval = pdMS_TO_TICKS(60 * 1000);
constexpr uint32_t kTaskStackSize = 4096;
constexpr UBaseType_t kTaskPriority = 3;

enum class AppMode {
    Base,
    PomodoroConfirmation,
    PomodoroRunning,
    PomodoroDone,
};

Canvas *s_canvas = nullptr;
TaskHandle_t s_app_task = nullptr;

bool deadline_reached(TickType_t now, TickType_t deadline)
{
    return static_cast<int32_t>(now - deadline) >= 0;
}

bool is_pomodoro_transition(StickyImuOrientation from,
                            StickyImuOrientation to)
{
    // These are the two calibrated clockwise landscape-to-portrait transitions.
    // 这两组是经过真机标定的顺时针横屏转竖屏动作。
    return (from == StickyImuOrientation::Landscape0 &&
            to == StickyImuOrientation::Portrait0) ||
           (from == StickyImuOrientation::Landscape180 &&
            to == StickyImuOrientation::Portrait180);
}

void log_refresh_failure(const char *page, esp_err_t result)
{
    if (result != ESP_OK) {
        STICKY_LOGE(kTag,
                    "page=%s refresh=%s",
                    page,
                    esp_err_to_name(result));
    }
}

void show_base(StickyImuOrientation orientation)
{
    app_page_render_base(*s_canvas, orientation);
    log_refresh_failure("base", sticky_display_refresh_monochrome());
}

void show_confirmation(StickyImuOrientation orientation)
{
    app_page_render_pomodoro_confirmation(*s_canvas, orientation);
    log_refresh_failure("pomodoro_confirmation",
                        sticky_display_refresh_monochrome());
}

void show_running(StickyImuOrientation orientation,
                  uint32_t remaining_seconds,
                  bool full_refresh)
{
    app_page_render_pomodoro_running(
        *s_canvas, orientation, remaining_seconds);
    const esp_err_t result = full_refresh
                                 ? sticky_display_refresh_monochrome()
                                 : sticky_display_refresh_partial();
    log_refresh_failure("pomodoro_running", result);
}

void show_done(StickyImuOrientation orientation)
{
    app_page_render_pomodoro_done(*s_canvas, orientation);
    log_refresh_failure("pomodoro_done", sticky_display_refresh_monochrome());
}

uint32_t remaining_timer_seconds(TickType_t now, TickType_t deadline)
{
    if (deadline_reached(now, deadline)) {
        return 0;
    }
    const uint32_t remaining_ms =
        static_cast<uint32_t>(pdTICKS_TO_MS(deadline - now));
    return (remaining_ms + 999U) / 1000U;
}

void app_task(void *)
{
    AppMode mode = AppMode::Base;
    StickyImuOrientation settled_orientation = StickyImuOrientation::Unknown;
    TickType_t confirmation_deadline = 0;
    TickType_t timer_deadline = 0;
    TickType_t next_timer_refresh = 0;
    bool render_base_after_settle = false;
    bool render_done_after_settle = false;

    while (true) {
        StickyImuState imu_state = {};
        if (sticky_imu_get_state(imu_state) != ESP_OK) {
            vTaskDelay(kPollInterval);
            continue;
        }

        const TickType_t now = xTaskGetTickCount();

        if (mode == AppMode::PomodoroConfirmation && imu_state.moving) {
            mode = AppMode::Base;
            render_base_after_settle = true;
            sticky_touch_clear_press();
            STICKY_LOGI(kTag,
                        "pomodoro=confirmation state=canceled reason=motion");
        }

        if (!imu_state.moving &&
            imu_state.orientation != StickyImuOrientation::Unknown &&
            imu_state.orientation != settled_orientation) {
            const StickyImuOrientation previous = settled_orientation;
            settled_orientation = imu_state.orientation;
            STICKY_LOGI(kTag,
                        "app=orientation from=%s to=%s",
                        sticky_imu_orientation_name(previous),
                        sticky_imu_orientation_name(settled_orientation));

            if (mode == AppMode::Base &&
                is_pomodoro_transition(previous, settled_orientation)) {
                show_confirmation(settled_orientation);
                sticky_touch_clear_press();
                confirmation_deadline =
                    xTaskGetTickCount() + kConfirmationDuration;
                mode = AppMode::PomodoroConfirmation;
                render_base_after_settle = false;
                STICKY_LOGI(kTag,
                            "pomodoro=confirmation state=shown timeout_ms=10000 from=%s to=%s",
                            sticky_imu_orientation_name(previous),
                            sticky_imu_orientation_name(settled_orientation));
            } else if (mode == AppMode::Base) {
                show_base(settled_orientation);
                render_base_after_settle = false;
            } else if (mode == AppMode::PomodoroRunning) {
                show_running(settled_orientation,
                             remaining_timer_seconds(now, timer_deadline),
                             true);
                next_timer_refresh =
                    xTaskGetTickCount() + kTimerRefreshInterval;
            } else if (mode == AppMode::PomodoroDone) {
                show_done(settled_orientation);
                render_done_after_settle = false;
            }
        }

        if (mode == AppMode::Base && render_base_after_settle &&
            !imu_state.moving) {
            show_base(settled_orientation);
            render_base_after_settle = false;
        } else if (mode == AppMode::PomodoroConfirmation &&
                   !imu_state.moving) {
            StickyTouchPress press = {};
            if (deadline_reached(now, confirmation_deadline)) {
                sticky_touch_clear_press();
                mode = AppMode::Base;
                STICKY_LOGI(kTag,
                            "pomodoro=confirmation state=timeout fallback=portrait_base");
                show_base(settled_orientation);
            } else if (sticky_touch_take_press(press)) {
                mode = AppMode::PomodoroRunning;
                const TickType_t timer_started_at = xTaskGetTickCount();
                timer_deadline = timer_started_at + kPomodoroDuration;
                next_timer_refresh = timer_started_at + kTimerRefreshInterval;
                STICKY_LOGI(kTag,
                            "pomodoro=confirmation state=accepted x=%u y=%u",
                            static_cast<unsigned>(press.x),
                            static_cast<unsigned>(press.y));
                STICKY_LOGI(kTag,
                            "pomodoro=timer state=started duration_s=900");
                show_running(settled_orientation, 15U * 60U, true);
            }
        } else if (mode == AppMode::PomodoroRunning) {
            if (deadline_reached(now, timer_deadline)) {
                mode = AppMode::PomodoroDone;
                STICKY_LOGI(kTag, "pomodoro=timer state=completed");
                if (imu_state.moving) {
                    render_done_after_settle = true;
                } else {
                    show_done(settled_orientation);
                }
            } else if (!imu_state.moving &&
                       deadline_reached(now, next_timer_refresh)) {
                show_running(settled_orientation,
                             remaining_timer_seconds(now, timer_deadline),
                             false);
                next_timer_refresh = now + kTimerRefreshInterval;
            }
        } else if (mode == AppMode::PomodoroDone &&
                   render_done_after_settle && !imu_state.moving) {
            show_done(settled_orientation);
            render_done_after_settle = false;
        }

        vTaskDelay(kPollInterval);
    }
}

}  // namespace

esp_err_t sticky_app_start(Canvas &canvas)
{
    app_log_register_tag(kTag);
    if (s_app_task != nullptr) {
        return ESP_OK;
    }

    s_canvas = &canvas;
    if (xTaskCreate(app_task,
                    "sticky_app",
                    kTaskStackSize,
                    nullptr,
                    kTaskPriority,
                    &s_app_task) != pdPASS) {
        s_canvas = nullptr;
        return ESP_ERR_NO_MEM;
    }

    STICKY_LOGI(kTag,
                "app=ready poll_ms=50 confirmation_ms=10000 pomodoro_s=900 result=ok");
    return ESP_OK;
}
