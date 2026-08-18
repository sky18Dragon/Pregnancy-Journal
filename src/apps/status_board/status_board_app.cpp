#include "status_board_app.h"

#include <cstring>

#include "app_log.h"
#include "canvas.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "status_board_pages.h"
#include "status_board_state.h"
#include "sticky_display.h"
#include "sticky_touch.h"

namespace {

constexpr char kTag[] = "status_board_app";
constexpr TickType_t kPollInterval = pdMS_TO_TICKS(40);
constexpr uint32_t kTaskStackSize = 4096;
constexpr UBaseType_t kTaskPriority = 3;
constexpr size_t kCustomTextMaximum = 20U;

Canvas *s_canvas = nullptr;
TaskHandle_t s_app_task = nullptr;
StatusBoardState s_state = {};
StatusBoardKeyboardMode s_keyboard_mode = StatusBoardKeyboardMode::Letters;
char s_custom_text[kCustomTextMaximum + 1U] = {};
size_t s_custom_text_length = 0U;
bool s_input_error = false;

void render_page(bool partial_refresh)
{
    switch (s_state.page) {
    case StatusBoardPage::Menu:
        status_board_page_render_menu(*s_canvas, s_state.selected_status);
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

    const esp_err_t result = partial_refresh
                                 ? sticky_display_refresh_partial()
                                 : sticky_display_refresh_monochrome();
    if (result != ESP_OK) {
        STICKY_LOGE(kTag,
                    "status_board=display refresh=%s result=%s",
                    partial_refresh ? "partial" : "full",
                    esp_err_to_name(result));
    }
    sticky_touch_clear_press();
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

void handle_action(StatusBoardAction action)
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
        render_page(true);
        return;
    }

    if (s_state.page == StatusBoardPage::CustomInput &&
        action == StatusBoardAction::Apply &&
        s_custom_text_length == 0U) {
        s_input_error = true;
        STICKY_LOGW(kTag,
                    "status_board=custom_input action=apply result=empty");
        render_page(true);
        return;
    }

    const StatusBoardPage previous_page = s_state.page;
    const StatusBoardStatus previous_status = s_state.selected_status;
    if (!status_board_state_handle_action(
            s_state, action, s_custom_text_length > 0U)) {
        return;
    }

    if (s_state.page == StatusBoardPage::CustomInput) {
        s_keyboard_mode = StatusBoardKeyboardMode::Letters;
        s_input_error = false;
    }
    STICKY_LOGI(kTag,
                "status_board=transition page_from=%s page_to=%s status_from=%s status_to=%s action=%s result=ok",
                status_board_page_name(previous_page),
                status_board_page_name(s_state.page),
                status_board_status_name(previous_status),
                status_board_status_name(s_state.selected_status),
                status_board_action_name(action));
    render_page(true);
}

void app_task(void *)
{
    render_page(false);
    STICKY_LOGI(kTag,
                "status_board=ready orientation=landscape page=%s status=%s choices=6 result=ok",
                status_board_page_name(s_state.page),
                status_board_status_name(s_state.selected_status));

    while (true) {
        StickyTouchPress press = {};
        if (sticky_touch_take_press(press)) {
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
                STICKY_LOGI(kTag,
                            "status_board=touch page=%s action=%s physical_x=%u physical_y=%u logical_x=%d logical_y=%d",
                            status_board_page_name(s_state.page),
                            status_board_action_name(action),
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
