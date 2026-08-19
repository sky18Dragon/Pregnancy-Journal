#include "desktop_pet_app.h"

#include <cstdint>

#include "app_log.h"
#include "canvas.h"
#include "desktop_pet_pages.h"
#include "desktop_pet_state.h"
#include "desktop_pet_storage.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sticky_display.h"
#include "sticky_touch.h"

#ifndef STICKY_LOG_DESKTOP_PET_ENABLED
#define STICKY_LOG_DESKTOP_PET_ENABLED 0
#endif

namespace {

constexpr char kTag[] = "desktop_pet_app";
constexpr TickType_t kPollInterval = pdMS_TO_TICKS(30);
constexpr uint32_t kTaskStackSize = 6144;
constexpr UBaseType_t kTaskPriority = 3;
constexpr int64_t kActionPoseHoldUs = 1300000LL;
constexpr char kHomeMessage[] = "LET'S SPEND TODAY TOGETHER.";

Canvas *s_canvas = nullptr;
TaskHandle_t s_app_task = nullptr;
DesktopPetState s_state = {};
DesktopPetPose s_pose = DesktopPetPose::Idle;
const char *s_message = kHomeMessage;
bool s_test_open = false;
bool s_reset_confirmation = false;
int64_t s_pose_deadline_us = 0;
#if STICKY_DESKTOP_PET_TEST_MODE
int64_t s_day_deadline_us = 0;
#endif

esp_err_t refresh_display(bool partial_refresh, bool timing_log = true)
{
#if !STICKY_LOG_DISPLAY_TIMING_ENABLED
    (void)timing_log;
#endif
#if STICKY_LOG_DISPLAY_TIMING_ENABLED
    const int64_t started_us = timing_log ? esp_timer_get_time() : 0;
#endif
    const esp_err_t result = partial_refresh
                                 ? sticky_display_refresh_partial()
                                 : sticky_display_refresh_monochrome();
#if STICKY_LOG_DISPLAY_TIMING_ENABLED
    if (timing_log) {
        STICKY_LOGD(kTag,
                    "pet=refresh page=%s mode=%s elapsed_ms=%lld result=%s",
                    s_test_open ? "test" : "home",
                    partial_refresh ? "partial" : "full",
                    static_cast<long long>(
                        (esp_timer_get_time() - started_us) / 1000LL),
                    esp_err_to_name(result));
    }
#endif
    if (result != ESP_OK) {
        STICKY_LOGE(kTag,
                    "pet=refresh page=%s mode=%s result=%s",
                    s_test_open ? "test" : "home",
                    partial_refresh ? "partial" : "full",
                    esp_err_to_name(result));
    }
    return result;
}

void render_current_page(bool partial_refresh, bool timing_log = true)
{
    if (s_test_open) {
        desktop_pet_page_render_test(
            *s_canvas, s_state, s_reset_confirmation);
    } else {
        desktop_pet_page_render_home(
            *s_canvas, s_state, s_pose, s_message);
    }
    refresh_display(partial_refresh, timing_log);
}

void save_state(const char *reason)
{
    const esp_err_t result = desktop_pet_storage_save(s_state);
    if (result == ESP_OK) {
#if STICKY_LOG_DESKTOP_PET_ENABLED
        STICKY_LOGD(kTag,
                    "pet=save reason=%s day=%u growth=%u love=%u result=ok",
                    reason,
                    static_cast<unsigned>(s_state.day),
                    static_cast<unsigned>(s_state.growth),
                    static_cast<unsigned>(s_state.love));
#endif
        return;
    }
    STICKY_LOGE(kTag,
                "pet=save reason=%s result=%s",
                reason,
                esp_err_to_name(result));
}

void handle_test_action(DesktopPetAction action)
{
    if (action == DesktopPetAction::CloseTest) {
        s_test_open = false;
        s_reset_confirmation = false;
        s_message = kHomeMessage;
        s_pose = DesktopPetPose::Idle;
        s_pose_deadline_us = 0;
        sticky_touch_clear_press();
        render_current_page(true);
        return;
    }

    if (action == DesktopPetAction::Reset && !s_reset_confirmation) {
        s_reset_confirmation = true;
        render_current_page(true);
        return;
    }

    if (action == DesktopPetAction::Reset) {
        const esp_err_t erase_result = desktop_pet_storage_reset();
        if (erase_result != ESP_OK) {
            STICKY_LOGE(kTag,
                        "pet=test action=reset erase_result=%s",
                        esp_err_to_name(erase_result));
            return;
        }
    }

    const DesktopPetActionResult result =
        desktop_pet_state_apply(s_state, action);
    if (!result.changed) {
        return;
    }
    s_reset_confirmation = false;
    s_message = result.message;
    save_state(desktop_pet_action_name(action));
    STICKY_LOGI(kTag,
                "pet=test action=%s day=%u growth=%u love=%u result=ok",
                desktop_pet_action_name(action),
                static_cast<unsigned>(s_state.day),
                static_cast<unsigned>(s_state.growth),
                static_cast<unsigned>(s_state.love));
    render_current_page(true);
}

void handle_action(DesktopPetAction action)
{
    if (action == DesktopPetAction::None) {
        return;
    }
    if (s_test_open) {
        handle_test_action(action);
        return;
    }
    if (action == DesktopPetAction::OpenTest) {
        s_test_open = true;
        s_reset_confirmation = false;
        sticky_touch_clear_press();
        render_current_page(true);
        return;
    }

    const DesktopPetActionResult result =
        desktop_pet_state_apply(s_state, action);
    if (!result.changed) {
        return;
    }
    s_pose = result.pose;
    s_message = result.message;
    save_state(desktop_pet_action_name(action));
    STICKY_LOGI(kTag,
                "pet=care action=%s pose=%s rewarded=%d growth_delta=%u love_delta=%u growth=%u love=%u result=ok",
                desktop_pet_action_name(action),
                desktop_pet_pose_name(result.pose),
                result.rewarded ? 1 : 0,
                static_cast<unsigned>(result.growth_delta),
                static_cast<unsigned>(result.love_delta),
                static_cast<unsigned>(s_state.growth),
                static_cast<unsigned>(s_state.love));
    render_current_page(true);
    // Hold time begins after the e-paper refresh finishes so the complete
    // pose remains visible for the requested duration.
    // 电子纸刷新完成后再开始计时，确保完整动作真正显示足够时长。
    s_pose_deadline_us = esp_timer_get_time() + kActionPoseHoldUs;
}

DesktopPetAction action_for_press(const StickyTouchPress &press)
{
    int logical_x = 0;
    int logical_y = 0;
    s_canvas->physical_to_logical(
        press.x, press.y, logical_x, logical_y);
    const DesktopPetAction action =
        desktop_pet_page_action_at(s_test_open, logical_x, logical_y);
#if STICKY_LOG_DESKTOP_PET_ENABLED
    const uint32_t now_ms =
        static_cast<uint32_t>(esp_timer_get_time() / 1000LL);
    STICKY_LOGD(kTag,
                "pet=touch page=%s action=%s physical_x=%u physical_y=%u logical_x=%d logical_y=%d queue_latency_ms=%u",
                s_test_open ? "test" : "home",
                desktop_pet_action_name(action),
                static_cast<unsigned>(press.x),
                static_cast<unsigned>(press.y),
                logical_x,
                logical_y,
                static_cast<unsigned>(now_ms - press.captured_at_ms));
#endif
    return action;
}

void app_task(void *)
{
    bool found = false;
    const esp_err_t load_result = desktop_pet_storage_load(s_state, found);
    if (load_result != ESP_OK) {
        STICKY_LOGE(kTag,
                    "pet=load result=%s fallback=default",
                    esp_err_to_name(load_result));
        s_state = {};
    }

    sticky_touch_clear_press();
    render_current_page(false);
#if STICKY_DESKTOP_PET_TEST_MODE
    s_day_deadline_us = esp_timer_get_time() +
                        static_cast<int64_t>(kDesktopPetTestDayLengthMs) *
                            1000LL;
#endif
    STICKY_LOGI(kTag,
                "pet=ready page=home profile=%s save=%s day=%u growth=%u love=%u result=ok",
#if STICKY_DESKTOP_PET_TEST_MODE
                "test",
#else
                "production",
#endif
                found ? "loaded" : "new",
                static_cast<unsigned>(s_state.day),
                static_cast<unsigned>(s_state.growth),
                static_cast<unsigned>(s_state.love));

    while (true) {
        StickyTouchPress press = {};
        if (sticky_touch_take_press(press)) {
            handle_action(action_for_press(press));
        }

        const int64_t now_us = esp_timer_get_time();
        if (!s_test_open && s_pose_deadline_us > 0 &&
            now_us >= s_pose_deadline_us) {
            s_pose = DesktopPetPose::Idle;
            s_message = kHomeMessage;
            s_pose_deadline_us = 0;
            render_current_page(true, false);
        }

#if STICKY_DESKTOP_PET_TEST_MODE
        if (s_day_deadline_us > 0 && now_us >= s_day_deadline_us) {
            desktop_pet_state_advance_day(s_state);
            s_day_deadline_us +=
                static_cast<int64_t>(kDesktopPetTestDayLengthMs) * 1000LL;
            s_message = "A NEW DAY TOGETHER!";
            save_state("automatic_next_day");
            STICKY_LOGI(kTag,
                        "pet=day source=timer day=%u result=ok",
                        static_cast<unsigned>(s_state.day));
            render_current_page(true);
            s_pose_deadline_us = esp_timer_get_time() + kActionPoseHoldUs;
        }
#endif
        vTaskDelay(kPollInterval);
    }
}

}  // namespace

esp_err_t desktop_pet_app_start(Canvas &canvas)
{
    app_log_register_tag(kTag);
    if (s_app_task != nullptr) {
        return ESP_OK;
    }
    s_canvas = &canvas;
    if (xTaskCreate(app_task,
                    "desktop_pet_app",
                    kTaskStackSize,
                    nullptr,
                    kTaskPriority,
                    &s_app_task) != pdPASS) {
        s_canvas = nullptr;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
