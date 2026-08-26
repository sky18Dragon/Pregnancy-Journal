#pragma once

#include "book_of_answers_state.h"

class Canvas;

enum class BookOfAnswersShakeFrame {
    Left,
    Right,
};

enum class BookOfAnswersAnimationFrame {
    Primary,
    Secondary,
};

// Draws the stable home page and highlights the selected answer mode.
// 绘制稳定主页，并突出当前选择的答案模式。
void book_of_answers_page_render_home(Canvas &canvas,
                                      BookOfAnswersMode mode);

// Draws one large-motion frame while the user is actively shaking.
// 绘制用户持续摇晃时的一帧大幅动作画面。
void book_of_answers_page_render_shaking(Canvas &canvas,
                                         BookOfAnswersShakeFrame frame);

// Draws the alternating thinking animation after a qualified shake.
// 绘制摇晃达标后的交替思考动画。
void book_of_answers_page_render_thinking(
    Canvas &canvas,
    BookOfAnswersAnimationFrame frame);

// Draws the answer-reveal transition shared by both answer modes.
// 绘制两种答案模式共用的答案揭晓过场。
void book_of_answers_page_render_revealing(
    Canvas &canvas,
    BookOfAnswersAnimationFrame frame);

// Draws the retry page shown when the shake duration is under three seconds.
// 绘制摇晃未满三秒时显示的重试页面。
void book_of_answers_page_render_shake_longer(
    Canvas &canvas,
    BookOfAnswersAnimationFrame frame);

// Draws a full message answer and its subtle result animation.
// 绘制完整的一句话答案及轻量结果动画。
void book_of_answers_page_render_message_result(Canvas &canvas,
                                                const char *answer,
                                                BookOfAnswersAnimationFrame frame);

// Draws YES, NO, or UNCLEAR inside the animated crystal ball.
// 在动态水晶球内绘制YES、NO或UNCLEAR。
void book_of_answers_page_render_crystal_result(Canvas &canvas,
                                                const char *answer,
                                                BookOfAnswersAnimationFrame frame);

// Maps one logical portrait touch coordinate to the visible page action.
// 将竖屏逻辑触摸坐标映射为当前页面操作。
BookOfAnswersAction book_of_answers_page_action_at(BookOfAnswersPage page,
                                                   int x,
                                                   int y);
