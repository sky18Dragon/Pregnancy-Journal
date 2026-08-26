#pragma once

#include <cstddef>
#include <cstdint>

enum class BookOfAnswersPage {
    Home,
    Shaking,
    Thinking,
    Revealing,
    ShakeLonger,
    MessageResult,
    CrystalResult,
};

enum class BookOfAnswersMode {
    Message,
    Crystal,
};

enum class BookOfAnswersAction {
    None,
    SelectMessage,
    SelectCrystal,
    ShakeStarted,
    ShakeStopped,
    AskAgain,
    End,
};

struct BookOfAnswersState {
    BookOfAnswersPage page = BookOfAnswersPage::Home;
    BookOfAnswersMode mode = BookOfAnswersMode::Message;
};

struct BookOfAnswersShakeInputGate {
    bool waiting_for_quiet = false;
    bool quiet_window_active = false;
    uint32_t quiet_started_ms = 0U;
};

constexpr size_t kBookOfAnswersNoIndex = static_cast<size_t>(-1);
constexpr uint32_t kBookOfAnswersRequiredShakeMs = 3000U;
constexpr uint32_t kBookOfAnswersFreshShakeQuietMs = 500U;

// Applies one touch action while retaining the selected answer type.
// 应用一次触摸动作，并在返回主页时保留已经选择的答案类型。
bool book_of_answers_state_handle_action(BookOfAnswersState &state,
                                         BookOfAnswersAction action);

// Advances one timed animation stage toward the selected result page.
// 将定时动画向前推进一个阶段，最终进入当前答案类型的结果页。
bool book_of_answers_state_advance(BookOfAnswersState &state);

// Returns true after effective shake peaks span the required three seconds.
// 当有效摇晃峰值实际覆盖满三秒后返回true。
bool book_of_answers_shake_qualified(uint32_t shake_duration_ms);

// Requires the current launcher shake to end before accepting a new question.
// 要求应用选择器的当前摇晃结束后，才接受下一次求答案动作。
void book_of_answers_shake_input_require_fresh(
    BookOfAnswersShakeInputGate &gate);

// Allows shake input immediately for entries that did not consume a shake.
// 对未使用摇晃进入答案书的方式立即开放摇晃输入。
void book_of_answers_shake_input_allow_current(
    BookOfAnswersShakeInputGate &gate);

// Advances the quiet window and returns true exactly when input is rearmed.
// 推进静止计时，并且只在输入重新开放的瞬间返回true。
bool book_of_answers_shake_input_update(
    BookOfAnswersShakeInputGate &gate,
    bool shaking,
    uint32_t now_ms);

// Maps a random value to a valid index without immediately repeating the
// previous result when more than one option exists.
// 将随机值映射为有效下标；存在多个选项时不会立即重复上一次结果。
size_t book_of_answers_choose_index(uint32_t random_value,
                                    size_t previous_index,
                                    size_t option_count);

// Returns stable English labels used by diagnostics and host tests.
// 返回诊断日志和主机测试共用的稳定英文名称。
const char *book_of_answers_page_name(BookOfAnswersPage page);
const char *book_of_answers_mode_name(BookOfAnswersMode mode);
const char *book_of_answers_action_name(BookOfAnswersAction action);
