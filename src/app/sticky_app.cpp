#include "sticky_app.h"

#include <cstdint>

#include "app_log.h"
#include "app_pages.h"
#include "book_of_answers_app.h"
#include "canvas.h"
#include "desktop_pet_app.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pomodoro_app.h"
#include "status_board_app.h"
#include "sticky_app_display_orientation.h"
#include "sticky_app_id.h"
#include "sticky_app_router.h"
#include "sticky_buzzer.h"
#include "sticky_button.h"
#include "sticky_display.h"
#include "sticky_imu.h"
#include "sticky_touch.h"

namespace {

constexpr char kTag[] = "sticky_app";
constexpr TickType_t kPollInterval = pdMS_TO_TICKS(40);
constexpr uint8_t kInitialImuSampleAttempts = 7U;
constexpr TickType_t kInitialImuSampleRetry = pdMS_TO_TICKS(10);
constexpr uint32_t kTaskStackSize = 5120U;
constexpr UBaseType_t kTaskPriority = 4U;

Canvas *s_canvas = nullptr;
TaskHandle_t s_app_task = nullptr;
StickyAppId s_current_app = StickyAppId::DesktopPet;
bool s_pet_started = false;
bool s_status_started = false;
bool s_pomodoro_started = false;
bool s_book_started = false;

struct LauncherImuPrestart {
    bool active = false;
    bool restarted = false;
    int64_t started_at_us = 0;
    StickyImuOrientation baseline = StickyImuOrientation::Unknown;
    StickyImuOrientation last_settled = StickyImuOrientation::Unknown;
};

bool app_started(StickyAppId app)
{
    switch (app) {
    case StickyAppId::DesktopPet:
        return s_pet_started;
    case StickyAppId::Pomodoro:
        return s_pomodoro_started;
    case StickyAppId::StatusBoard:
        return s_status_started;
    case StickyAppId::BookOfAnswers:
        return s_book_started;
    }
    return false;
}

void mark_app_started(StickyAppId app)
{
    switch (app) {
    case StickyAppId::DesktopPet:
        s_pet_started = true;
        break;
    case StickyAppId::Pomodoro:
        s_pomodoro_started = true;
        break;
    case StickyAppId::StatusBoard:
        s_status_started = true;
        break;
    case StickyAppId::BookOfAnswers:
        s_book_started = true;
        break;
    }
}

esp_err_t pause_app(StickyAppId app)
{
    switch (app) {
    case StickyAppId::DesktopPet:
        return desktop_pet_app_pause();
    case StickyAppId::Pomodoro:
        return pomodoro_app_pause();
    case StickyAppId::StatusBoard:
        return status_board_app_pause();
    case StickyAppId::BookOfAnswers:
        return book_of_answers_app_pause();
    }
    return ESP_ERR_INVALID_ARG;
}

esp_err_t start_app(StickyAppId app)
{
    switch (app) {
    case StickyAppId::DesktopPet:
        return desktop_pet_app_start(*s_canvas);
    case StickyAppId::Pomodoro:
        return pomodoro_app_start(*s_canvas);
    case StickyAppId::StatusBoard:
        return status_board_app_start(*s_canvas);
    case StickyAppId::BookOfAnswers:
        return book_of_answers_app_start(*s_canvas);
    }
    return ESP_ERR_INVALID_ARG;
}

esp_err_t resume_app(StickyAppId app)
{
    switch (app) {
    case StickyAppId::DesktopPet:
        return desktop_pet_app_resume();
    case StickyAppId::Pomodoro:
        return pomodoro_app_resume();
    case StickyAppId::StatusBoard:
        return status_board_app_resume();
    case StickyAppId::BookOfAnswers:
        return book_of_answers_app_resume();
    }
    return ESP_ERR_INVALID_ARG;
}

esp_err_t activate_app(StickyAppId app)
{
    sticky_touch_clear_press();
    if (!app_started(app)) {
        const esp_err_t result = start_app(app);
        if (result == ESP_OK) {
            mark_app_started(app);
        }
        return result;
    }
    return resume_app(app);
}

esp_err_t set_imu_running(bool running)
{
    return running ? sticky_imu_start_monitoring()
                   : sticky_imu_stop_monitoring();
}

void render_launcher()
{
    sticky_touch_clear_press();
    app_page_render_launcher(*s_canvas, s_current_app);
    const esp_err_t result = sticky_display_refresh_partial();
    if (result != ESP_OK) {
        STICKY_LOGE(kTag,
                    "launcher=display refresh=partial result=%s",
                    esp_err_to_name(result));
    }
}

StickyImuState read_initial_launcher_imu_state()
{
    // Gives the new monitor task a short scheduling window and keeps the first
    // physical pose as the route baseline before display work begins.
    // 给新IMU任务留出很短的调度时间，并在屏幕工作前保存最初实际姿态。
    StickyImuState state = {};
    for (uint8_t attempt = 0U;
         attempt < kInitialImuSampleAttempts;
         ++attempt) {
        if (sticky_imu_get_state(state) == ESP_OK) {
            return state;
        }
        vTaskDelay(kInitialImuSampleRetry);
    }
    return {};
}

// Starts one launcher-owned IMU session on the physical press event and keeps
// its first pose until the click type has been resolved.
// 在物理按下事件中启动选择器专属IMU会话，并保留起始姿态直到完成点击类型判定。
bool prestart_launcher_imu(LauncherImuPrestart &prestart)
{
    if (prestart.active) {
        return true;
    }

    prestart = {};
    prestart.started_at_us = esp_timer_get_time();
    prestart.restarted =
        s_current_app == StickyAppId::BookOfAnswers;
    if (prestart.restarted) {
        const esp_err_t stop_result = set_imu_running(false);
        if (stop_result != ESP_OK) {
            STICKY_LOGE(kTag,
                        "launcher=imu phase=press_start app=book_of_answers stop=%s result=failed",
                        esp_err_to_name(stop_result));
            prestart = {};
            return false;
        }
    }

    const esp_err_t start_result = set_imu_running(true);
    if (start_result != ESP_OK) {
        STICKY_LOGE(kTag,
                    "launcher=imu phase=press_start start=%s result=failed",
                    esp_err_to_name(start_result));
        prestart = {};
        return false;
    }

    const StickyImuState imu_state = read_initial_launcher_imu_state();
    if (imu_state.valid) {
        prestart.baseline =
            imu_state.orientation != StickyImuOrientation::Unknown
                ? imu_state.orientation
                : imu_state.observed_orientation;
        prestart.last_settled = imu_state.orientation;
    }
    prestart.active = true;
    STICKY_LOGI(kTag,
                "launcher=imu phase=started_on_press current_app=%s baseline=%s restart=%d result=ok",
                sticky_app_id_name(s_current_app),
                sticky_imu_orientation_name(prestart.baseline),
                prestart.restarted);
    return true;
}

bool open_launcher(StickyAppRouterState &router,
                   StickyImuOrientation &last_settled,
                   LauncherImuPrestart &prestart)
{
    if (!prestart_launcher_imu(prestart)) {
        return false;
    }

    sticky_app_router_open(router, prestart.baseline);
    last_settled = prestart.last_settled;

    const int64_t pause_started_at_us = esp_timer_get_time();
    const esp_err_t pause_result = pause_app(s_current_app);
    if (pause_result != ESP_OK) {
        sticky_app_router_close(router);
        if (!prestart.restarted) {
            set_imu_running(false);
        }
        resume_app(s_current_app);
        STICKY_LOGE(kTag,
                    "launcher=open app=%s pause=%s imu=%s result=failed",
                    sticky_app_id_name(s_current_app),
                    esp_err_to_name(pause_result),
                    prestart.restarted ? "retained" : "stopped");
        prestart = {};
        return false;
    }

    const int64_t display_started_at_us = esp_timer_get_time();
    render_launcher();
    STICKY_LOGI(kTag,
                "launcher=opened current_app=%s input=touch,rotation,shake baseline=%s imu=started pause_ms=%lld display_ms=%lld total_ms=%lld result=ok",
                sticky_app_id_name(s_current_app),
                sticky_imu_orientation_name(
                    router.baseline_orientation),
                static_cast<long long>(
                    (display_started_at_us - pause_started_at_us) /
                    1000LL),
                static_cast<long long>(
                    (esp_timer_get_time() - display_started_at_us) /
                    1000LL),
                static_cast<long long>(
                    (esp_timer_get_time() - prestart.started_at_us) /
                    1000LL));
    prestart = {};
    return true;
}

void cancel_launcher(StickyAppRouterState &router)
{
    sticky_app_router_close(router);
    esp_err_t imu_result = set_imu_running(false);
    if (s_current_app == StickyAppId::BookOfAnswers) {
        if (imu_result == ESP_OK) {
            imu_result = set_imu_running(true);
        }
    }
    sticky_touch_clear_press();
    const esp_err_t resume_result = resume_app(s_current_app);
    STICKY_LOGI(kTag,
                "launcher=cancelled app=%s imu=%s resume=%s result=%s",
                sticky_app_id_name(s_current_app),
                s_current_app == StickyAppId::BookOfAnswers
                    ? esp_err_to_name(imu_result)
                    : "stopped",
                esp_err_to_name(resume_result),
                imu_result == ESP_OK && resume_result == ESP_OK
                    ? "ok"
                    : "failed");
}

void complete_selection(StickyAppId selected_app,
                        const char *source,
                        bool preserve_shake_session,
                        StickyImuOrientation final_orientation)
{
    const StickyAppId previous_app = s_current_app;
    esp_err_t imu_result = ESP_OK;
    if (selected_app == StickyAppId::BookOfAnswers) {
        if (!preserve_shake_session) {
            imu_result = set_imu_running(false);
            if (imu_result == ESP_OK) {
                imu_result = set_imu_running(true);
            }
        }
    } else {
        imu_result = set_imu_running(false);
    }

    CanvasRotation display_rotation = CanvasRotation::Deg0;
    if (sticky_app_display_rotation(selected_app,
                                    final_orientation,
                                    display_rotation)) {
        if (selected_app == StickyAppId::Pomodoro) {
            pomodoro_app_set_display_rotation(display_rotation);
        } else if (selected_app == StickyAppId::StatusBoard) {
            status_board_app_set_display_rotation(display_rotation);
        }
        STICKY_LOGI(kTag,
                    "launcher=orientation app=%s imu=%s display_rotation=%s result=applied",
                    sticky_app_id_name(selected_app),
                    sticky_imu_orientation_name(final_orientation),
                    sticky_app_display_rotation_name(display_rotation));
    }

    if (imu_result == ESP_OK) {
        sticky_display_prepare_app_transition_refresh();
    }
    const esp_err_t activation_result =
        imu_result == ESP_OK ? activate_app(selected_app) : imu_result;
    if (activation_result == ESP_OK) {
        s_current_app = selected_app;
        STICKY_LOGI(
            kTag,
            "launcher=selected app_from=%s app_to=%s input=%s imu=%s result=ok",
            sticky_app_id_name(previous_app),
            sticky_app_id_name(s_current_app),
            source,
            s_current_app == StickyAppId::BookOfAnswers
                ? "started"
                : "stopped");
        return;
    }

    sticky_display_cancel_app_transition_refresh();
    if (selected_app == StickyAppId::BookOfAnswers) {
        set_imu_running(false);
    }
    if (previous_app == StickyAppId::BookOfAnswers) {
        set_imu_running(true);
    }
    resume_app(previous_app);
    STICKY_LOGE(kTag,
                "launcher=selected app=%s activation=%s fallback=%s result=failed",
                sticky_app_id_name(selected_app),
                esp_err_to_name(activation_result),
                sticky_app_id_name(previous_app));
}

void return_to_desktop_pet(StickyAppRouterState &router)
{
    const esp_err_t buzzer_result = sticky_buzzer_stop();
    if (buzzer_result != ESP_OK) {
        STICKY_LOGW(kTag,
                    "launcher=home input=button_double_click buzzer=%s",
                    esp_err_to_name(buzzer_result));
    }

    if (s_current_app == StickyAppId::DesktopPet) {
        if (router.launcher_open) {
            sticky_app_router_close(router);
        }
        const esp_err_t imu_result = set_imu_running(false);
        sticky_display_prepare_app_transition_refresh();
        const esp_err_t home_result = desktop_pet_app_return_home();
        if (home_result != ESP_OK) {
            sticky_display_cancel_app_transition_refresh();
            resume_app(StickyAppId::DesktopPet);
        }
        STICKY_LOGI(kTag,
                    "launcher=home input=button_double_click app_from=desktop_pet app_to=desktop_pet page=root imu=%s navigation=%s result=%s",
                    esp_err_to_name(imu_result),
                    esp_err_to_name(home_result),
                    imu_result == ESP_OK && home_result == ESP_OK
                        ? "ok"
                        : "failed");
        return;
    }

    if (router.launcher_open) {
        sticky_app_router_close(router);
    } else {
        const esp_err_t pause_result = pause_app(s_current_app);
        if (pause_result != ESP_OK) {
            STICKY_LOGE(kTag,
                        "launcher=home input=button_double_click app_from=%s pause=%s result=failed",
                        sticky_app_id_name(s_current_app),
                        esp_err_to_name(pause_result));
            return;
        }
    }

    complete_selection(StickyAppId::DesktopPet,
                       "button_double_click",
                       false,
                       StickyImuOrientation::Unknown);
}

void handle_launcher_touch(const StickyTouchPress &press,
                           StickyAppRouterState &router)
{
    int logical_x = 0;
    int logical_y = 0;
    s_canvas->physical_to_logical(
        press.x, press.y, logical_x, logical_y);
    StickyAppId selected_app = StickyAppId::DesktopPet;
    if (!app_page_launcher_app_at(s_canvas->width(),
                                  s_canvas->height(),
                                  logical_x,
                                  logical_y,
                                  selected_app)) {
        STICKY_LOGD(kTag,
                    "launcher=touch action=none physical_x=%u physical_y=%u logical_x=%d logical_y=%d",
                    press.x,
                    press.y,
                    logical_x,
                    logical_y);
        return;
    }

    sticky_app_router_close(router);
    STICKY_LOGI(kTag,
                "launcher=touch action=select app=%s logical_x=%d logical_y=%d",
                sticky_app_id_name(selected_app),
                logical_x,
                logical_y);
    complete_selection(selected_app,
                       "touch",
                       false,
                       StickyImuOrientation::Unknown);
}

void log_route(const StickyAppRouteResult &route, const char *source)
{
    STICKY_LOGI(kTag,
                "launcher=route action=%s input=%s from=%s to=%s app=%s",
                sticky_app_route_action_name(route.action),
                source,
                sticky_imu_orientation_name(route.from_orientation),
                sticky_imu_orientation_name(route.to_orientation),
                sticky_app_id_name(route.selected_app));
}

void handle_route(const StickyAppRouteResult &route, const char *source)
{
    if (route.action == StickyAppRouteAction::BaselineCaptured) {
        STICKY_LOGI(kTag,
                    "launcher=baseline orientation=%s source=%s result=ready",
                    sticky_imu_orientation_name(route.to_orientation),
                    source);
        log_route(route, source);
        return;
    }
    if (route.action != StickyAppRouteAction::AppSelected) {
        return;
    }

    log_route(route, source);
    complete_selection(route.selected_app,
                       source,
                       route.selected_app ==
                           StickyAppId::BookOfAnswers,
                       route.to_orientation);
}

void app_task(void *)
{
    StickyAppRouterState router = {};
    LauncherImuPrestart imu_prestart = {};
    StickyImuOrientation last_settled = StickyImuOrientation::Unknown;
    STICKY_LOGI(kTag,
                "launcher=ready trigger=top_button selection=touch,rotation,shake apps=4 imu=on_demand shake_select_ms=%u current_app=%s result=ok",
                static_cast<unsigned>(kStickyLauncherShakeSelectMs),
                sticky_app_id_name(s_current_app));

    while (true) {
        StickyButtonEvent button_event = StickyButtonEvent::None;
        if (sticky_button_take_event(button_event)) {
            if (button_event == StickyButtonEvent::PressDown) {
                if (!router.launcher_open) {
                    prestart_launcher_imu(imu_prestart);
                }
            } else if (button_event == StickyButtonEvent::DoubleClick) {
                return_to_desktop_pet(router);
                if (imu_prestart.active &&
                    s_current_app == StickyAppId::DesktopPet) {
                    set_imu_running(false);
                }
                imu_prestart = {};
            } else if (button_event == StickyButtonEvent::SingleClick) {
                if (router.launcher_open) {
                    cancel_launcher(router);
                } else {
                    open_launcher(router,
                                  last_settled,
                                  imu_prestart);
                }
            }
        }

        if (router.launcher_open) {
            StickyTouchPress press = {};
            if (sticky_touch_take_press(press)) {
                handle_launcher_touch(press, router);
            }
        }

        if (router.launcher_open) {
            StickyImuState imu_state = {};
            const bool shake_active = sticky_imu_is_shaking();
            if (shake_active) {
                handle_route(sticky_app_router_shaking(
                                 router,
                                 sticky_imu_shake_duration_ms()),
                             "shake");
            }

            if (sticky_imu_get_state(imu_state) == ESP_OK) {
                if (router.baseline_orientation ==
                    StickyImuOrientation::Unknown) {
                    handle_route(sticky_app_router_observed(
                                     router,
                                     imu_state.observed_orientation),
                                 "observed");
                }

                if (router.launcher_open &&
                    imu_state.observed_orientation !=
                        StickyImuOrientation::Unknown &&
                    imu_state.observed_orientation != last_settled) {
                    const StickyAppRouteResult route =
                        sticky_app_router_rotation_candidate(
                            router,
                            imu_state.observed_orientation,
                            imu_state.orientation_stable_samples,
                            shake_active);
                    if (route.action != StickyAppRouteAction::None) {
                        last_settled = imu_state.observed_orientation;
                        handle_route(route, "rotation_fast");
                    }
                }
            }
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
    esp_err_t result = sticky_button_init();
    if (result != ESP_OK) {
        s_canvas = nullptr;
        return result;
    }

    result = desktop_pet_app_start(canvas);
    if (result != ESP_OK) {
        s_canvas = nullptr;
        return result;
    }
    s_pet_started = true;
    s_current_app = StickyAppId::DesktopPet;

    if (xTaskCreate(app_task,
                    "sticky_launcher",
                    kTaskStackSize,
                    nullptr,
                    kTaskPriority,
                    &s_app_task) != pdPASS) {
        s_canvas = nullptr;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
