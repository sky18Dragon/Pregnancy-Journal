#include "book_of_answers_app.h"

#include <cstdint>

#include "app_log.h"
#include "book_of_answers_answers.h"
#include "book_of_answers_pages.h"
#include "book_of_answers_state.h"
#include "canvas.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sticky_display.h"
#include "sticky_touch.h"

namespace {

constexpr char kTag[] = "book_of_answers_app";
constexpr TickType_t kPollInterval = pdMS_TO_TICKS(40);
constexpr uint32_t kTaskStackSize = 6144;
constexpr UBaseType_t kTaskPriority = 3;
constexpr size_t kShakeFrameCount = 4U;
constexpr int64_t kShakeFrameHoldUs = 180000LL;
constexpr int64_t kThinkingHoldUs = 650000LL;
constexpr int64_t kRevealingHoldUs = 650000LL;

Canvas *s_canvas = nullptr;
TaskHandle_t s_app_task = nullptr;
BookOfAnswersState s_state = {};
size_t s_shake_frame_index = 0U;
size_t s_message_answer_index = kBookOfAnswersNoIndex;
size_t s_crystal_answer_index = kBookOfAnswersNoIndex;
int64_t s_animation_deadline_us = 0;

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
                    "book=refresh state=begin page=%s mode=%s",
                    book_of_answers_page_name(s_state.page),
                    partial_refresh ? "partial" : "full");
    }
#endif
    const esp_err_t result = partial_refresh
                                 ? sticky_display_refresh_partial()
                                 : sticky_display_refresh_monochrome();
#if STICKY_LOG_DISPLAY_TIMING_ENABLED
    if (timing_log) {
        STICKY_LOGD(kTag,
                    "book=refresh state=done page=%s mode=%s elapsed_ms=%lld result=%s",
                    book_of_answers_page_name(s_state.page),
                    partial_refresh ? "partial" : "full",
                    static_cast<long long>(
                        (esp_timer_get_time() - refresh_started_us) / 1000LL),
                    esp_err_to_name(result));
    }
#endif
    if (result != ESP_OK) {
        STICKY_LOGE(kTag,
                    "book=display refresh=%s result=%s",
                    partial_refresh ? "partial" : "full",
                    esp_err_to_name(result));
    }
    return result;
}

void schedule_animation_stage()
{
    const int64_t now = esp_timer_get_time();
    switch (s_state.page) {
    case BookOfAnswersPage::Shaking:
        s_animation_deadline_us = now + kShakeFrameHoldUs;
        break;
    case BookOfAnswersPage::Thinking:
        s_animation_deadline_us = now + kThinkingHoldUs;
        break;
    case BookOfAnswersPage::Revealing:
        s_animation_deadline_us = now + kRevealingHoldUs;
        break;
    case BookOfAnswersPage::Home:
    case BookOfAnswersPage::MessageResult:
    case BookOfAnswersPage::CrystalResult:
        s_animation_deadline_us = 0;
        break;
    }
}

void render_current_page(bool partial_refresh, bool timing_log = true)
{
    switch (s_state.page) {
    case BookOfAnswersPage::Home:
        book_of_answers_page_render_home(*s_canvas, s_state.mode);
        break;
    case BookOfAnswersPage::Shaking:
        book_of_answers_page_render_shaking(
            *s_canvas,
            (s_shake_frame_index % 2U) == 0U
                ? BookOfAnswersShakeFrame::Left
                : BookOfAnswersShakeFrame::Right);
        break;
    case BookOfAnswersPage::Thinking:
        book_of_answers_page_render_thinking(*s_canvas);
        break;
    case BookOfAnswersPage::Revealing:
        book_of_answers_page_render_revealing(*s_canvas);
        break;
    case BookOfAnswersPage::MessageResult: {
        const BookMessageAnswer &answer =
            book_message_answer(s_message_answer_index);
        book_of_answers_page_render_message_result(
            *s_canvas, answer.text);
        break;
    }
    case BookOfAnswersPage::CrystalResult:
        book_of_answers_page_render_crystal_result(
            *s_canvas, book_crystal_answer(s_crystal_answer_index));
        break;
    }

    refresh_display(partial_refresh, timing_log);
    schedule_animation_stage();
}

void choose_answer_for_round()
{
    if (s_state.mode == BookOfAnswersMode::Message) {
        s_message_answer_index = book_of_answers_choose_index(
            esp_random(),
            s_message_answer_index,
            book_message_answer_count());
        const BookMessageAnswer &answer =
            book_message_answer(s_message_answer_index);
        STICKY_LOGI(kTag,
                    "book=answer selected mode=message index=%u source_id=%u answer=%s",
                    static_cast<unsigned>(s_message_answer_index),
                    static_cast<unsigned>(s_message_answer_index + 1U),
                    answer.text);
        return;
    }

    s_crystal_answer_index = book_of_answers_choose_index(
        esp_random(),
        s_crystal_answer_index,
        book_crystal_answer_count());
    STICKY_LOGI(kTag,
                "book=answer selected mode=crystal index=%u value=%s",
                static_cast<unsigned>(s_crystal_answer_index),
                book_crystal_answer(s_crystal_answer_index));
}

void log_transition(BookOfAnswersPage previous_page,
                    BookOfAnswersAction action,
                    const char *reason)
{
    STICKY_LOGI(kTag,
                "book=transition page_from=%s page_to=%s mode=%s action=%s reason=%s result=ok",
                book_of_answers_page_name(previous_page),
                book_of_answers_page_name(s_state.page),
                book_of_answers_mode_name(s_state.mode),
                book_of_answers_action_name(action),
                reason);
}

void handle_action(BookOfAnswersAction action)
{
    const BookOfAnswersPage previous_page = s_state.page;
    const BookOfAnswersMode previous_mode = s_state.mode;
    if (!book_of_answers_state_handle_action(s_state, action)) {
        return;
    }

    if (s_state.page == BookOfAnswersPage::Shaking) {
        choose_answer_for_round();
        s_shake_frame_index = 0U;
    }

    const bool page_changed = previous_page != s_state.page;
    if (page_changed) {
        sticky_touch_clear_press();
    }
    log_transition(previous_page,
                   action,
                   previous_mode == s_state.mode
                       ? "touch"
                       : "answer_type_changed");
    render_current_page(true);
}

void advance_animation()
{
    if (s_state.page == BookOfAnswersPage::Shaking &&
        s_shake_frame_index + 1U < kShakeFrameCount) {
        ++s_shake_frame_index;
#if STICKY_LOG_BOOK_ANIMATION_ENABLED
        STICKY_LOGD(kTag,
                    "book=animation page=shaking frame=%u side=%s",
                    static_cast<unsigned>(s_shake_frame_index),
                    (s_shake_frame_index % 2U) == 0U ? "left" : "right");
#endif
        render_current_page(true, false);
        return;
    }

    const BookOfAnswersPage previous_page = s_state.page;
    if (!book_of_answers_state_advance(s_state)) {
        s_animation_deadline_us = 0;
        return;
    }

    sticky_touch_clear_press();
    log_transition(previous_page,
                   BookOfAnswersAction::None,
                   "animation_complete");
    render_current_page(true);
}

BookOfAnswersAction action_for_press(const StickyTouchPress &press)
{
    int logical_x = 0;
    int logical_y = 0;
    s_canvas->physical_to_logical(
        press.x, press.y, logical_x, logical_y);
    const BookOfAnswersAction action =
        book_of_answers_page_action_at(s_state.page, logical_x, logical_y);
    if (action != BookOfAnswersAction::None) {
        const uint32_t now_ms =
            static_cast<uint32_t>(esp_timer_get_time() / 1000LL);
        STICKY_LOGI(kTag,
                    "book=touch page=%s action=%s physical_x=%u physical_y=%u logical_x=%d logical_y=%d queue_latency_ms=%u",
                    book_of_answers_page_name(s_state.page),
                    book_of_answers_action_name(action),
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
    sticky_touch_clear_press();
    render_current_page(false);
    STICKY_LOGI(kTag,
                "book=ready page=home mode=message message_answers=%u crystal_answers=%u shake_frames=%u result=ok",
                static_cast<unsigned>(book_message_answer_count()),
                static_cast<unsigned>(book_crystal_answer_count()),
                static_cast<unsigned>(kShakeFrameCount));

    while (true) {
        StickyTouchPress press = {};
        if (sticky_touch_take_press(press)) {
            handle_action(action_for_press(press));
        }

        // Touch is consumed before any timed animation refresh.
        // 每轮先处理触摸，再判断是否推进定时动画。
        if (s_animation_deadline_us > 0 &&
            esp_timer_get_time() >= s_animation_deadline_us) {
            advance_animation();
        }

        vTaskDelay(kPollInterval);
    }
}

}  // namespace

esp_err_t book_of_answers_app_start(Canvas &canvas)
{
    app_log_register_tag(kTag);
    if (s_app_task != nullptr) {
        return ESP_OK;
    }

    s_canvas = &canvas;
    if (xTaskCreate(app_task,
                    "book_answers_app",
                    kTaskStackSize,
                    nullptr,
                    kTaskPriority,
                    &s_app_task) != pdPASS) {
        s_canvas = nullptr;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
