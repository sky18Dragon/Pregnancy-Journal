#include "book_of_answers_app.h"

#include <atomic>
#include <cstdint>

#include "app_log.h"
#include "book_of_answers_answers.h"
#include "book_of_answers_pages.h"
#include "book_of_answers_state.h"
#include "canvas.h"
#include "esp_attr.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sticky_app_lifecycle.h"
#include "sticky_display.h"
#include "sticky_imu.h"
#include "sticky_touch.h"

namespace {

constexpr char kTag[] = "book_of_answers_app";
constexpr TickType_t kPollInterval = pdMS_TO_TICKS(40);
constexpr uint32_t kTaskStackSize = 6144;
constexpr UBaseType_t kTaskPriority = 3;
constexpr size_t kShakeFrameCount = 4U;
constexpr size_t kStageFrameCount = 2U;
constexpr size_t kShakeLongerFrameCount = 4U;
constexpr int64_t kShakeFrameHoldUs = 180000LL;
constexpr int64_t kThinkingFrameHoldUs = 450000LL;
constexpr int64_t kRevealingFrameHoldUs = 450000LL;
constexpr int64_t kResultFrameHoldUs = 800000LL;
constexpr int64_t kShakeLongerFrameHoldUs = 600000LL;
constexpr uint32_t kSleepSnapshotMagic = 0x424F4F4BU;
constexpr uint32_t kIdleSleepTimeoutMs = 5U * 60U * 1000U;

struct BookOfAnswersSleepSnapshot {
    uint32_t magic;
    BookOfAnswersState state;
    size_t message_answer_index;
    size_t crystal_answer_index;
};

RTC_NOINIT_ATTR BookOfAnswersSleepSnapshot s_sleep_snapshot;

Canvas *s_canvas = nullptr;
TaskHandle_t s_app_task = nullptr;
StickyAppLifecycle s_lifecycle = {};
BookOfAnswersState s_state = {};
size_t s_shake_frame_index = 0U;
size_t s_animation_frame_index = 0U;
size_t s_message_answer_index = kBookOfAnswersNoIndex;
size_t s_crystal_answer_index = kBookOfAnswersNoIndex;
int64_t s_animation_deadline_us = 0;
BookOfAnswersShakeInputGate s_shake_input_gate = {};
std::atomic<int> s_entry_shake_policy{-1};

void apply_entry_shake_policy()
{
    const int policy =
        s_entry_shake_policy.exchange(-1, std::memory_order_acq_rel);
    if (policy < 0) {
        return;
    }
    if (policy == 1) {
        book_of_answers_shake_input_require_fresh(s_shake_input_gate);
        STICKY_LOGI(kTag,
                    "book=input state=waiting_for_fresh_shake quiet_ms=%u reason=launcher_shake_consumed",
                    static_cast<unsigned>(kBookOfAnswersFreshShakeQuietMs));
        return;
    }
    book_of_answers_shake_input_allow_current(s_shake_input_gate);
}

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
    case BookOfAnswersPage::Home:
        s_animation_deadline_us = 0;
        break;
    case BookOfAnswersPage::Shaking:
        s_animation_deadline_us = now + kShakeFrameHoldUs;
        break;
    case BookOfAnswersPage::Thinking:
        s_animation_deadline_us = now + kThinkingFrameHoldUs;
        break;
    case BookOfAnswersPage::Revealing:
        s_animation_deadline_us = now + kRevealingFrameHoldUs;
        break;
    case BookOfAnswersPage::ShakeLonger:
        s_animation_deadline_us = now + kShakeLongerFrameHoldUs;
        break;
    case BookOfAnswersPage::MessageResult:
    case BookOfAnswersPage::CrystalResult:
        s_animation_deadline_us = now + kResultFrameHoldUs;
        break;
    }
}

BookOfAnswersAnimationFrame current_animation_frame()
{
    return (s_animation_frame_index % 2U) == 0U
               ? BookOfAnswersAnimationFrame::Primary
               : BookOfAnswersAnimationFrame::Secondary;
}

void render_current_page(bool partial_refresh, bool timing_log = true)
{
    if (!partial_refresh && s_state.page == BookOfAnswersPage::Home) {
        // A clean full waveform keeps the static reading page crisp after an
        // app transition; animated stages continue using partial refreshes.
        // APP切换后用完整波形保持静态阅读页清晰，动画阶段继续使用局刷。
        sticky_display_cancel_app_transition_refresh();
    }
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
        book_of_answers_page_render_thinking(
            *s_canvas, current_animation_frame());
        break;
    case BookOfAnswersPage::Revealing:
        book_of_answers_page_render_revealing(
            *s_canvas, current_animation_frame());
        break;
    case BookOfAnswersPage::ShakeLonger:
        book_of_answers_page_render_shake_longer(
            *s_canvas, current_animation_frame());
        break;
    case BookOfAnswersPage::MessageResult: {
        const BookMessageAnswer &answer =
            book_message_answer(s_message_answer_index);
        book_of_answers_page_render_message_result(
            *s_canvas, answer.text, current_animation_frame());
        break;
    }
    case BookOfAnswersPage::CrystalResult:
        book_of_answers_page_render_crystal_result(
            *s_canvas,
            book_crystal_answer(s_crystal_answer_index),
            current_animation_frame());
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

void handle_action(BookOfAnswersAction action, const char *reason)
{
    const BookOfAnswersPage previous_page = s_state.page;
    const BookOfAnswersMode previous_mode = s_state.mode;
    if (!book_of_answers_state_handle_action(s_state, action)) {
        return;
    }

    if (s_state.page == BookOfAnswersPage::Shaking) {
        s_shake_frame_index = 0U;
    }

    const bool page_changed = previous_page != s_state.page;
    if (page_changed) {
        s_animation_frame_index = 0U;
        sticky_touch_clear_press();
    }
    log_transition(previous_page,
                   action,
                   previous_mode == s_state.mode
                       ? reason
                       : "answer_type_changed");
    render_current_page(true);
}

void complete_shake_stage();

void advance_animation()
{
    const bool answer_animation_page =
        s_state.page == BookOfAnswersPage::Shaking ||
        s_state.page == BookOfAnswersPage::Thinking ||
        s_state.page == BookOfAnswersPage::Revealing;
    const bool finite_animation_page =
        answer_animation_page ||
        s_state.page == BookOfAnswersPage::ShakeLonger;

    // The user only needs to keep shaking during the fixed three-second shake
    // stage. Thinking and revealing continue normally after qualification.
    // 用户只需在固定三秒摇晃阶段保持动作；达标后正常播放思考和揭晓动画。
    if (s_state.page == BookOfAnswersPage::Shaking &&
        !sticky_imu_is_shaking()) {
        const uint32_t shake_duration_ms =
            sticky_imu_shake_duration_ms();
        if (book_of_answers_shake_qualified(shake_duration_ms)) {
            complete_shake_stage();
            return;
        }
        STICKY_LOGI(kTag,
                    "book=shake qualification=insufficient shake_ms=%u required_ms=%u reason=stopped_early",
                    static_cast<unsigned>(shake_duration_ms),
                    static_cast<unsigned>(
                        kBookOfAnswersRequiredShakeMs));
        handle_action(
            BookOfAnswersAction::ShakeStopped, "imu_shake_stopped");
        return;
    }

    const size_t frame_count =
        s_state.page == BookOfAnswersPage::Shaking
            ? kShakeFrameCount
            : (s_state.page == BookOfAnswersPage::ShakeLonger
                   ? kShakeLongerFrameCount
                   : kStageFrameCount);
    size_t &frame_index =
        s_state.page == BookOfAnswersPage::Shaking
            ? s_shake_frame_index
            : s_animation_frame_index;

    if (finite_animation_page && frame_index + 1U < frame_count) {
        ++frame_index;
#if STICKY_LOG_BOOK_ANIMATION_ENABLED
        STICKY_LOGD(kTag,
                    "book=animation page=%s frame=%u",
                    book_of_answers_page_name(s_state.page),
                    static_cast<unsigned>(frame_index));
#endif
        render_current_page(true, false);
        return;
    }

    if (!finite_animation_page) {
        s_animation_frame_index =
            (s_animation_frame_index + 1U) % kStageFrameCount;
#if STICKY_LOG_BOOK_ANIMATION_ENABLED
        STICKY_LOGD(kTag,
                    "book=animation page=%s frame=%u",
                    book_of_answers_page_name(s_state.page),
                    static_cast<unsigned>(s_animation_frame_index));
#endif
        render_current_page(true, false);
        return;
    }

    const BookOfAnswersPage previous_page = s_state.page;
    if (previous_page == BookOfAnswersPage::Shaking) {
        s_shake_frame_index = 0U;
        render_current_page(true, false);
        return;
    }
    if (previous_page == BookOfAnswersPage::Revealing) {
        choose_answer_for_round();
    }
    if (!book_of_answers_state_advance(s_state)) {
        s_animation_deadline_us = 0;
        return;
    }

    sticky_touch_clear_press();
    s_animation_frame_index = 0U;
    s_shake_frame_index = 0U;
    log_transition(previous_page,
                   BookOfAnswersAction::None,
                   "animation_complete");
    render_current_page(true);
}

void complete_shake_stage()
{
    if (s_state.page != BookOfAnswersPage::Shaking) {
        return;
    }
    const uint32_t shake_duration_ms = sticky_imu_shake_duration_ms();
    if (!book_of_answers_shake_qualified(shake_duration_ms)) {
        STICKY_LOGI(kTag,
                    "book=shake qualification=insufficient shake_ms=%u required_ms=%u reason=stopped_early",
                    static_cast<unsigned>(shake_duration_ms),
                    static_cast<unsigned>(
                        kBookOfAnswersRequiredShakeMs));
        handle_action(
            BookOfAnswersAction::ShakeStopped, "imu_shake_stopped");
        return;
    }

    const BookOfAnswersPage previous_page = s_state.page;
    STICKY_LOGI(kTag,
                "book=shake qualification=passed shake_ms=%u required_ms=%u result=ok",
                static_cast<unsigned>(shake_duration_ms),
                static_cast<unsigned>(kBookOfAnswersRequiredShakeMs));
    if (!book_of_answers_state_advance(s_state)) {
        return;
    }

    s_animation_frame_index = 0U;
    s_shake_frame_index = 0U;
    sticky_touch_clear_press();
    log_transition(previous_page,
                   BookOfAnswersAction::None,
                   "shake_duration_complete");
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
    apply_entry_shake_policy();
    sticky_touch_clear_press();
    render_current_page(false);
    STICKY_LOGI(kTag,
                "book=ready page=home mode=message input=fresh_continuous_imu_shake required_shake_ms=%u entry_quiet_ms=%u message_answers=%u crystal_answers=%u shake_frames=%u animated_pages=6 result=ok",
                static_cast<unsigned>(kBookOfAnswersRequiredShakeMs),
                static_cast<unsigned>(kBookOfAnswersFreshShakeQuietMs),
                static_cast<unsigned>(book_message_answer_count()),
                static_cast<unsigned>(book_crystal_answer_count()),
                static_cast<unsigned>(kShakeFrameCount));

    while (true) {
        const bool resumed =
            sticky_app_lifecycle_checkpoint(s_lifecycle);
        // Apply the launcher handoff after a paused task is released and
        // before it can consume any IMU events from the new session.
        // 暂停任务恢复后、读取新会话IMU事件前应用选择器交接策略。
        apply_entry_shake_policy();
        if (resumed) {
            sticky_touch_clear_press();
            render_current_page(false);
        }

        StickyTouchPress press = {};
        if (sticky_touch_take_press(press)) {
            handle_action(action_for_press(press), "touch");
        }

        const uint32_t now_ms = static_cast<uint32_t>(
            esp_timer_get_time() / 1000LL);
        if (s_shake_input_gate.waiting_for_quiet) {
            // Launcher events are consumed while the old physical gesture
            // settles, so they cannot leak into the answer request flow.
            // 等待旧动作停稳时消费选择器事件，避免其流入求答案流程。
            sticky_imu_take_shake_started_event();
            sticky_imu_take_shake_stopped_event();
            if (book_of_answers_shake_input_update(
                    s_shake_input_gate,
                    sticky_imu_is_shaking(),
                    now_ms)) {
                STICKY_LOGI(kTag,
                            "book=input state=ready reason=fresh_quiet_window result=ok");
            }
        } else {
            if (sticky_imu_take_shake_started_event()) {
                if (s_state.page == BookOfAnswersPage::Home ||
                    s_state.page == BookOfAnswersPage::ShakeLonger) {
                    handle_action(
                        BookOfAnswersAction::ShakeStarted,
                        "imu_shake_started");
                } else {
                    STICKY_LOGI(
                        kTag,
                        "book=shake page=%s action=ignored reason=page_busy",
                        book_of_answers_page_name(s_state.page));
                }
            }

            if (sticky_imu_take_shake_stopped_event()) {
                if (s_state.page == BookOfAnswersPage::Shaking) {
                    const uint32_t shake_duration_ms =
                        sticky_imu_shake_duration_ms();
                    if (book_of_answers_shake_qualified(
                            shake_duration_ms)) {
                        complete_shake_stage();
                    } else {
                        STICKY_LOGI(
                            kTag,
                            "book=shake qualification=insufficient shake_ms=%u required_ms=%u reason=stopped_early",
                            static_cast<unsigned>(shake_duration_ms),
                            static_cast<unsigned>(
                                kBookOfAnswersRequiredShakeMs));
                        handle_action(BookOfAnswersAction::ShakeStopped,
                                      "imu_shake_stopped");
                    }
                }
            }
        }

        // IMU peak span is measured in the sensor task, so display refresh
        // duration cannot extend or shorten the required shake input.
        // 有效峰值跨度由传感器任务独立计时，屏幕刷新不会延长或缩短摇晃输入。
        const int64_t now_us = esp_timer_get_time();
        if (!s_shake_input_gate.waiting_for_quiet &&
            s_state.page == BookOfAnswersPage::Shaking &&
            book_of_answers_shake_qualified(
                sticky_imu_shake_duration_ms())) {
            complete_shake_stage();
        } else if (s_animation_deadline_us > 0 &&
                   now_us >= s_animation_deadline_us) {
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

    if (s_sleep_snapshot.magic == kSleepSnapshotMagic) {
        s_state = s_sleep_snapshot.state;
        s_message_answer_index = s_sleep_snapshot.message_answer_index;
        s_crystal_answer_index = s_sleep_snapshot.crystal_answer_index;
        s_sleep_snapshot.magic = 0U;
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

void book_of_answers_app_prepare_entry(bool launcher_shake_consumed)
{
    s_entry_shake_policy.store(
        launcher_shake_consumed ? 1 : 0,
        std::memory_order_release);
}

esp_err_t book_of_answers_app_pause()
{
    return sticky_app_lifecycle_pause(s_lifecycle, s_app_task);
}

esp_err_t book_of_answers_app_resume()
{
    if (s_app_task == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    sticky_app_lifecycle_resume(s_lifecycle);
    return ESP_OK;
}

bool book_of_answers_app_power_sleep_allowed()
{
    return s_state.page != BookOfAnswersPage::Shaking &&
           s_state.page != BookOfAnswersPage::Thinking &&
           s_state.page != BookOfAnswersPage::Revealing;
}

esp_err_t book_of_answers_app_prepare_power_sleep()
{
    const esp_err_t result = book_of_answers_app_pause();
    if (result != ESP_OK) {
        return result;
    }
    s_sleep_snapshot.state = s_state;
    s_sleep_snapshot.message_answer_index = s_message_answer_index;
    s_sleep_snapshot.crystal_answer_index = s_crystal_answer_index;
    s_sleep_snapshot.magic = kSleepSnapshotMagic;
    return ESP_OK;
}

uint32_t book_of_answers_app_power_sleep_timeout_ms()
{
    return book_of_answers_app_power_sleep_allowed()
               ? kIdleSleepTimeoutMs
               : 0U;
}
