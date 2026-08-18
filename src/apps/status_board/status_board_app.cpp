#include "status_board_app.h"

#include <cstring>

#include "app_log.h"
#include "canvas.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "status_board_pages.h"
#include "status_board_state.h"
#include "status_pet_animation.h"
#include "sticky_display.h"
#include "sticky_touch.h"

namespace {

constexpr char kTag[] = "status_board_app";
constexpr TickType_t kPollInterval = pdMS_TO_TICKS(40);
constexpr uint32_t kTaskStackSize = 4096;
constexpr UBaseType_t kTaskPriority = 3;
constexpr size_t kCustomTextMaximum = 20U;
constexpr uint16_t kDisplayPrimaryHoldMs = 450U;
constexpr uint16_t kDisplaySecondaryHoldMs = 250U;

Canvas *s_canvas = nullptr;
TaskHandle_t s_app_task = nullptr;
StatusBoardState s_state = {};
StatusBoardKeyboardMode s_keyboard_mode = StatusBoardKeyboardMode::Letters;
char s_custom_text[kCustomTextMaximum + 1U] = {};
size_t s_custom_text_length = 0U;
bool s_input_error = false;
size_t s_pet_frame_index = 0U;
int64_t s_next_pet_frame_us = 0;
bool s_display_secondary_frame = false;
int64_t s_next_display_frame_us = 0;

esp_err_t refresh_display(bool partial_refresh, bool timing_log = true)
{
#if !STICKY_LOG_DISPLAY_TIMING_ENABLED
    (void)timing_log;
#endif
#if STICKY_LOG_DISPLAY_TIMING_ENABLED
    int64_t refresh_started_us = 0;
    if (timing_log) {
        refresh_started_us = esp_timer_get_time();
        STICKY_LOGD(kTag,
                    "status_board=refresh state=begin page=%s mode=%s",
                    status_board_page_name(s_state.page),
                    partial_refresh ? "partial" : "full");
    }
#endif
    const esp_err_t result = partial_refresh
                                 ? sticky_display_refresh_partial()
                                 : sticky_display_refresh_monochrome();
#if STICKY_LOG_DISPLAY_TIMING_ENABLED
    if (timing_log) {
        STICKY_LOGD(kTag,
                    "status_board=refresh state=done page=%s mode=%s elapsed_ms=%lld result=%s",
                    status_board_page_name(s_state.page),
                    partial_refresh ? "partial" : "full",
                    static_cast<long long>(
                        (esp_timer_get_time() - refresh_started_us) / 1000LL),
                    esp_err_to_name(result));
    }
#endif
    if (result != ESP_OK) {
        STICKY_LOGE(kTag,
                    "status_board=display refresh=%s result=%s",
                    partial_refresh ? "partial" : "full",
                    esp_err_to_name(result));
    }
    return result;
}

void schedule_current_pet_frame()
{
    const StatusPetFrame &frame = status_pet_frame(s_pet_frame_index);
    s_next_pet_frame_us =
        esp_timer_get_time() + static_cast<int64_t>(frame.hold_ms) * 1000LL;
}

void reset_pet_animation()
{
    s_pet_frame_index = 0U;
    s_next_pet_frame_us = 0;
}

void schedule_current_display_frame()
{
    const uint16_t hold_ms = s_display_secondary_frame
                                 ? kDisplaySecondaryHoldMs
                                 : kDisplayPrimaryHoldMs;
    s_next_display_frame_us =
        esp_timer_get_time() + static_cast<int64_t>(hold_ms) * 1000LL;
}

void reset_display_animation()
{
    s_display_secondary_frame = false;
    s_next_display_frame_us = 0;
}

void render_page(bool partial_refresh)
{
    switch (s_state.page) {
    case StatusBoardPage::Menu:
        status_board_page_render_menu(*s_canvas, s_state.selected_status);
        status_board_page_render_menu_pet(
            *s_canvas,
            status_pet_frame(s_pet_frame_index).pose,
            status_pet_frame(s_pet_frame_index).center_x);
        break;
    case StatusBoardPage::Display:
        status_board_page_render_display(
            *s_canvas, s_state.selected_status, s_custom_text);
        break;
    case StatusBoardPage::CustomInput:
        status_board_page_render_custom_input(
            *s_canvas, s_custom_text, s_keyboard_mode, s_input_error);
        break;
    }

    refresh_display(partial_refresh);
    if (s_state.page == StatusBoardPage::Menu) {
        schedule_current_pet_frame();
    } else if (s_state.page == StatusBoardPage::Display) {
        schedule_current_display_frame();
    }
}

void render_next_pet_frame()
{
    s_pet_frame_index =
        (s_pet_frame_index + 1U) % status_pet_frame_count();
    const StatusPetFrame &frame = status_pet_frame(s_pet_frame_index);
    status_board_page_render_menu_pet(
        *s_canvas, frame.pose, frame.center_x);
#if STICKY_LOG_PET_ANIMATION_ENABLED
    const int64_t refresh_started_us = esp_timer_get_time();
#endif
    refresh_display(true, false);
#if STICKY_LOG_PET_ANIMATION_ENABLED
    STICKY_LOGD(kTag,
                "status_board=pet_animation frame=%u pose=%s center_x=%d refresh_ms=%lld",
                static_cast<unsigned>(s_pet_frame_index),
                status_pet_pose_name(frame.pose),
                frame.center_x,
                static_cast<long long>(
                    (esp_timer_get_time() - refresh_started_us) / 1000LL));
#endif
    schedule_current_pet_frame();
}

void render_next_display_frame()
{
    s_display_secondary_frame = !s_display_secondary_frame;
    status_board_page_render_display_pet(
        *s_canvas, s_state.selected_status, s_display_secondary_frame);
#if STICKY_LOG_STATUS_ANIMATION_ENABLED
    const int64_t refresh_started_us = esp_timer_get_time();
#endif
    refresh_display(true, false);
#if STICKY_LOG_STATUS_ANIMATION_ENABLED
    STICKY_LOGD(kTag,
                "status_board=status_animation status=%s frame=%s refresh_ms=%lld",
                status_board_status_name(s_state.selected_status),
                s_display_secondary_frame ? "secondary" : "primary",
                static_cast<long long>(
                    (esp_timer_get_time() - refresh_started_us) / 1000LL));
#endif
    schedule_current_display_frame();
}

bool append_character(char character)
{
    if (s_custom_text_length >= kCustomTextMaximum) {
        return false;
    }
    s_custom_text[s_custom_text_length++] = character;
    s_custom_text[s_custom_text_length] = '\0';
    return true;
}

bool handle_custom_editor_action(StatusBoardAction action)
{
    char character = '\0';
    if (status_board_action_character(action, character)) {
        s_input_error = false;
        if (!append_character(character)) {
            STICKY_LOGW(kTag,
                        "status_board=custom_input action=append result=full length=%u",
                        static_cast<unsigned>(s_custom_text_length));
        }
        return true;
    }

    switch (action) {
    case StatusBoardAction::Space:
        s_input_error = false;
        if (s_custom_text_length > 0U &&
            s_custom_text[s_custom_text_length - 1U] != ' ') {
            append_character(' ');
        }
        return true;
    case StatusBoardAction::Delete:
        s_input_error = false;
        if (s_custom_text_length > 0U) {
            s_custom_text[--s_custom_text_length] = '\0';
        }
        return true;
    case StatusBoardAction::Clear:
        s_input_error = false;
        s_custom_text_length = 0U;
        s_custom_text[0] = '\0';
        return true;
    case StatusBoardAction::ToggleKeyboard:
        s_input_error = false;
        s_keyboard_mode =
            s_keyboard_mode == StatusBoardKeyboardMode::Letters
                ? StatusBoardKeyboardMode::Numbers
                : StatusBoardKeyboardMode::Letters;
        return true;
    default:
        return false;
    }
}

bool handle_action(StatusBoardAction action)
{
    if (s_state.page == StatusBoardPage::CustomInput &&
        handle_custom_editor_action(action)) {
        STICKY_LOGD(kTag,
                    "status_board=custom_input action=%s length=%u keyboard=%s",
                    status_board_action_name(action),
                    static_cast<unsigned>(s_custom_text_length),
                    s_keyboard_mode == StatusBoardKeyboardMode::Letters
                        ? "letters"
                        : "numbers");
        return true;
    }

    if (s_state.page == StatusBoardPage::CustomInput &&
        action == StatusBoardAction::Apply &&
        s_custom_text_length == 0U) {
        s_input_error = true;
        STICKY_LOGW(kTag,
                    "status_board=custom_input action=apply result=empty");
        return true;
    }

    const StatusBoardPage previous_page = s_state.page;
    const StatusBoardStatus previous_status = s_state.selected_status;
    if (!status_board_state_handle_action(
            s_state, action, s_custom_text_length > 0U)) {
        return false;
    }

    if (s_state.page == StatusBoardPage::CustomInput) {
        s_keyboard_mode = StatusBoardKeyboardMode::Letters;
        s_input_error = false;
    }
    if (previous_page != StatusBoardPage::Menu &&
        s_state.page == StatusBoardPage::Menu) {
        reset_pet_animation();
    }
    if (previous_page != StatusBoardPage::Display &&
        s_state.page == StatusBoardPage::Display) {
        reset_display_animation();
    }
    STICKY_LOGI(kTag,
                "status_board=transition page_from=%s page_to=%s status_from=%s status_to=%s action=%s result=ok",
                status_board_page_name(previous_page),
                status_board_page_name(s_state.page),
                status_board_status_name(previous_status),
                status_board_status_name(s_state.selected_status),
                status_board_action_name(action));
    return true;
}

StatusBoardAction action_for_press(const StickyTouchPress &press)
{
    int logical_x = 0;
    int logical_y = 0;
    s_canvas->physical_to_logical(
        press.x, press.y, logical_x, logical_y);
    const StatusBoardAction action =
        status_board_page_action_at(s_state.page,
                                    s_keyboard_mode,
                                    logical_x,
                                    logical_y);
    if (action != StatusBoardAction::None) {
        const uint32_t now_ms =
            static_cast<uint32_t>(esp_timer_get_time() / 1000LL);
        STICKY_LOGI(kTag,
                    "status_board=touch page=%s action=%s physical_x=%u physical_y=%u logical_x=%d logical_y=%d queue_latency_ms=%u",
                    status_board_page_name(s_state.page),
                    status_board_action_name(action),
                    static_cast<unsigned>(press.x),
                    static_cast<unsigned>(press.y),
                    logical_x,
                    logical_y,
                    static_cast<unsigned>(now_ms - press.captured_at_ms));
    }
    return action;
}

void app_task(void *)
{
    // Clears boot-time events before starting the first frame.
    // 开始绘制首帧前清理启动阶段的事件。
    sticky_touch_clear_press();
    render_page(false);
    STICKY_LOGI(kTag,
                "status_board=ready orientation=landscape page=%s status=%s choices=6 result=ok",
                status_board_page_name(s_state.page),
                status_board_status_name(s_state.selected_status));
    STICKY_LOGI(kTag,
                "status_board=pet_animation state=ready frames=%u loop=left_jump_right_walk_left result=ok",
                static_cast<unsigned>(status_pet_frame_count()));
    STICKY_LOGI(kTag,
                "status_board=status_animation state=ready statuses=6 frames_per_status=2 result=ok");

    while (true) {
        StickyTouchPress press = {};
        if (sticky_touch_take_press(press)) {
            bool redraw_needed = false;
            unsigned batched_actions = 0;

            while (true) {
                const StatusBoardAction action = action_for_press(press);
                if (action != StatusBoardAction::None) {
                    redraw_needed = handle_action(action) || redraw_needed;
                    ++batched_actions;

                    if (!status_board_action_can_batch(action)) {
                        // Clears remaining events from the current layout
                        // before rendering the next interaction layout.
                        // 绘制下一个交互布局前，清理当前布局剩余的事件。
                        sticky_touch_clear_press();
                        break;
                    }
                }

                if (s_state.page != StatusBoardPage::CustomInput ||
                    !sticky_touch_take_press(press)) {
                    break;
                }
            }

            if (redraw_needed) {
                if (batched_actions > 1U) {
                    STICKY_LOGI(kTag,
                                "status_board=input_batch actions=%u refreshes=1",
                                batched_actions);
                }
                render_page(true);
            }
        }

        // Touch is always handled before an animation refresh is considered.
        // 每轮循环总是先处理触摸，再判断是否刷新动画。
        if (s_state.page == StatusBoardPage::Menu &&
            s_next_pet_frame_us > 0 &&
            esp_timer_get_time() >= s_next_pet_frame_us) {
            render_next_pet_frame();
        } else if (s_state.page == StatusBoardPage::Display &&
                   s_next_display_frame_us > 0 &&
                   esp_timer_get_time() >= s_next_display_frame_us) {
            render_next_display_frame();
        }

        vTaskDelay(kPollInterval);
    }
}

}  // namespace

esp_err_t status_board_app_start(Canvas &canvas)
{
    app_log_register_tag(kTag);
    if (s_app_task != nullptr) {
        return ESP_OK;
    }

    s_canvas = &canvas;
    if (xTaskCreate(app_task,
                    "status_board_app",
                    kTaskStackSize,
                    nullptr,
                    kTaskPriority,
                    &s_app_task) != pdPASS) {
        s_canvas = nullptr;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
