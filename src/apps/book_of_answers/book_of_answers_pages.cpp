#include "book_of_answers_pages.h"

#include <algorithm>
#include <cstring>

#include "book_of_answers_assets.h"
#include "canvas.h"
#include "pixel_asset.h"

namespace {

constexpr int kScreenWidth = 480;
constexpr int kScreenHeight = 800;

struct Rect {
    int x;
    int y;
    int width;
    int height;

    bool contains(int point_x, int point_y) const
    {
        return point_x >= x && point_y >= y &&
               point_x < x + width && point_y < y + height;
    }
};

constexpr Rect kMessageModeRect = {35, 620, 195, 56};
constexpr Rect kCrystalModeRect = {250, 620, 195, 56};
constexpr Rect kTransitionStatusRect = {40, 700, 400, 64};
constexpr Rect kAskAgainRect = {40, 680, 400, 64};
constexpr Rect kEndRect = {120, 748, 240, 52};
constexpr size_t kAnswerMaximumLines = 4U;
constexpr size_t kAnswerMaximumCharactersPerLine = 23U;
constexpr size_t kAnswerLineCapacity =
    kAnswerMaximumCharactersPerLine + 1U;

struct WrappedAnswer {
    char lines[kAnswerMaximumLines][kAnswerLineCapacity] = {};
    size_t line_count = 0U;
};

int text_width(const char *text, int scale)
{
    if (text == nullptr || text[0] == '\0') {
        return 0;
    }
    return (static_cast<int>(std::strlen(text)) * 6 - 1) * scale;
}

int fitted_text_scale(const char *text, int maximum_scale, int width)
{
    const int unscaled_width = text_width(text, 1);
    if (unscaled_width <= 0) {
        return 1;
    }
    return std::max(1, std::min(maximum_scale, width / unscaled_width));
}

WrappedAnswer wrap_answer_text(const char *answer)
{
    WrappedAnswer wrapped = {};
    if (answer == nullptr || answer[0] == '\0') {
        std::memcpy(wrapped.lines[0], "?", 2U);
        wrapped.line_count = 1U;
        return wrapped;
    }

    // Greedily wraps complete words into the same four-line boundary that the
    // CSV generator validates before firmware compilation.
    // 按完整单词依次换行，行数和行宽与固件生成脚本的校验边界保持一致。
    const char *cursor = answer;
    while (*cursor != '\0') {
        while (*cursor == ' ') {
            ++cursor;
        }
        if (*cursor == '\0') {
            break;
        }

        const char *word = cursor;
        while (*cursor != '\0' && *cursor != ' ') {
            ++cursor;
        }
        const size_t word_length = static_cast<size_t>(cursor - word);

        if (wrapped.line_count == 0U) {
            wrapped.line_count = 1U;
        }
        char *line = wrapped.lines[wrapped.line_count - 1U];
        const size_t line_length = std::strlen(line);
        const size_t separator = line_length == 0U ? 0U : 1U;
        if (line_length + separator + word_length >
            kAnswerMaximumCharactersPerLine) {
            if (wrapped.line_count >= kAnswerMaximumLines) {
                break;
            }
            ++wrapped.line_count;
            line = wrapped.lines[wrapped.line_count - 1U];
        } else if (separator != 0U) {
            line[line_length] = ' ';
        }

        const size_t target_offset = std::strlen(line);
        const size_t copy_length = std::min(
            word_length,
            kAnswerMaximumCharactersPerLine - target_offset);
        std::memcpy(line + target_offset, word, copy_length);
        line[target_offset + copy_length] = '\0';
    }
    return wrapped;
}

int answer_text_scale(const WrappedAnswer &answer)
{
    int scale = 3;
    switch (answer.line_count) {
    case 1U:
        scale = 6;
        break;
    case 2U:
        scale = 5;
        break;
    case 3U:
        scale = 4;
        break;
    case 4U:
    default:
        scale = 3;
        break;
    }

    for (size_t line = 0U; line < answer.line_count; ++line) {
        scale = std::min(
            scale, fitted_text_scale(answer.lines[line], scale, 420));
    }
    return scale;
}

void draw_centered_text(Canvas &canvas,
                        int y,
                        const char *text,
                        int scale,
                        GrayLevel color = GrayLevel::Black)
{
    canvas.draw_text((kScreenWidth - text_width(text, scale)) / 2,
                     y,
                     text,
                     static_cast<uint8_t>(scale),
                     color);
}

void draw_centered_text_in_rect(Canvas &canvas,
                                const Rect &rect,
                                const char *text,
                                int scale,
                                GrayLevel color)
{
    const int x = rect.x + (rect.width - text_width(text, scale)) / 2;
    const int y = rect.y + (rect.height - 7 * scale) / 2;
    canvas.draw_text(x,
                     y,
                     text,
                     static_cast<uint8_t>(scale),
                     color);
}

void draw_double_rect(Canvas &canvas, const Rect &rect)
{
    canvas.draw_rect(rect.x,
                     rect.y,
                     rect.width,
                     rect.height,
                     GrayLevel::Black);
    canvas.draw_rect(rect.x + 1,
                     rect.y + 1,
                     rect.width - 2,
                     rect.height - 2,
                     GrayLevel::Black);
}

void draw_button(Canvas &canvas,
                 const Rect &rect,
                 const char *label,
                 bool selected,
                 int scale)
{
    if (selected) {
        canvas.fill_rect(rect.x,
                         rect.y,
                         rect.width,
                         rect.height,
                         GrayLevel::Black);
        draw_centered_text_in_rect(
            canvas, rect, label, scale, GrayLevel::White);
        return;
    }
    draw_double_rect(canvas, rect);
    draw_centered_text_in_rect(
        canvas, rect, label, scale, GrayLevel::Black);
}

void draw_sparkle(Canvas &canvas, int center_x, int center_y, int radius)
{
    canvas.draw_line(center_x - radius,
                     center_y,
                     center_x + radius,
                     center_y,
                     GrayLevel::Black);
    canvas.draw_line(center_x,
                     center_y - radius,
                     center_x,
                     center_y + radius,
                     GrayLevel::Black);
    canvas.fill_circle(center_x, center_y, 2, GrayLevel::Black);
}

void begin_page(Canvas &canvas)
{
    // Every Book of Answers page uses the calibrated upright portrait axes.
    // 答案书所有页面统一使用已标定的正向竖屏坐标。
    canvas.set_rotation(CanvasRotation::Deg90CounterClockwise);
    canvas.clear(GrayLevel::White);
}

void draw_header(Canvas &canvas, int scale = 3)
{
    draw_centered_text(canvas, 25, "BOOK OF ANSWERS", scale);
    canvas.fill_rect(32, 69, 190, 2, GrayLevel::Black);
    canvas.fill_rect(258, 69, 190, 2, GrayLevel::Black);
    draw_sparkle(canvas, 240, 70, 10);
}

void draw_transition_footer(Canvas &canvas,
                            const char *instruction,
                            const char *status)
{
    draw_centered_text(canvas, 650, instruction, 2);
    draw_button(canvas, kTransitionStatusRect, status, true, 4);
}

void draw_result_controls(Canvas &canvas)
{
    draw_button(canvas, kAskAgainRect, "ASK AGAIN", true, 4);
    draw_centered_text(canvas, 764, "END", 3);
}

}  // namespace

void book_of_answers_page_render_home(Canvas &canvas,
                                      BookOfAnswersMode mode,
                                      BookOfAnswersAnimationFrame frame)
{
    begin_page(canvas);
    draw_header(canvas);
    draw_centered_text(canvas, 116, "THINK OF A QUESTION", 3);
    draw_centered_text(canvas, 154, "IN YOUR HEART", 3);

    pixel_asset_draw_centered(
        canvas,
        240,
        390,
        book_of_answers_asset(
            frame == BookOfAnswersAnimationFrame::Primary
                ? BookOfAnswersAssetId::Home
                : BookOfAnswersAssetId::HomeAlt));

    draw_centered_text(canvas, 590, "ANSWER TYPE", 2);
    draw_button(canvas,
                kMessageModeRect,
                "MESSAGE",
                mode == BookOfAnswersMode::Message,
                3);
    draw_button(canvas,
                kCrystalModeRect,
                "YES / NO",
                mode == BookOfAnswersMode::Crystal,
                3);
    draw_centered_text(canvas, 716, "SHAKE THE DEVICE", 3);
    const int sparkle_x =
        frame == BookOfAnswersAnimationFrame::Primary ? 94 : 386;
    draw_sparkle(canvas, sparkle_x, 726, 8);
    canvas.draw_line(145, 760, 335, 760, GrayLevel::Black);
}

void book_of_answers_page_render_shaking(Canvas &canvas,
                                         BookOfAnswersShakeFrame frame)
{
    begin_page(canvas);
    draw_header(canvas);
    draw_centered_text(canvas, 116, "THINK OF A QUESTION", 3);
    draw_centered_text(canvas, 154, "IN YOUR HEART", 3);

    const BookOfAnswersAssetId asset =
        frame == BookOfAnswersShakeFrame::Left
            ? BookOfAnswersAssetId::ShakeLeft
            : BookOfAnswersAssetId::ShakeRight;
    pixel_asset_draw_centered(canvas, 240, 390, book_of_answers_asset(asset));
    draw_transition_footer(canvas, "THE CRYSTAL IS MOVING", "SHAKING...");
}

void book_of_answers_page_render_thinking(
    Canvas &canvas,
    BookOfAnswersAnimationFrame frame)
{
    begin_page(canvas);
    draw_header(canvas);
    draw_centered_text(canvas, 116, "KEEP SHAKING", 3);
    draw_centered_text(canvas, 154, "KEEP YOUR QUESTION IN MIND", 2);
    pixel_asset_draw_centered(
        canvas,
        240,
        390,
        book_of_answers_asset(
            frame == BookOfAnswersAnimationFrame::Primary
                ? BookOfAnswersAssetId::Thinking
                : BookOfAnswersAssetId::ThinkingAlt));
    draw_transition_footer(canvas, "LET THE CRYSTAL THINK", "THINKING...");
}

void book_of_answers_page_render_revealing(
    Canvas &canvas,
    BookOfAnswersAnimationFrame frame)
{
    begin_page(canvas);
    draw_header(canvas);
    draw_centered_text(canvas, 116, "KEEP SHAKING", 3);
    draw_centered_text(canvas, 154, "THE ANSWER IS NEAR", 3);
    pixel_asset_draw_centered(
        canvas,
        240,
        390,
        book_of_answers_asset(
            frame == BookOfAnswersAnimationFrame::Primary
                ? BookOfAnswersAssetId::Revealing
                : BookOfAnswersAssetId::RevealingAlt));
    draw_transition_footer(canvas, "ONE LAST MOMENT", "REVEALING...");
}

void book_of_answers_page_render_shake_longer(
    Canvas &canvas,
    BookOfAnswersAnimationFrame frame)
{
    begin_page(canvas);
    draw_header(canvas);
    draw_centered_text(canvas, 110, "SHAKE A LITTLE LONGER", 3);
    draw_centered_text(canvas, 151, "KEEP YOUR QUESTION", 2);
    draw_centered_text(canvas, 178, "IN YOUR HEART", 2);
    pixel_asset_draw_centered(
        canvas,
        240,
        410,
        book_of_answers_asset(
            frame == BookOfAnswersAnimationFrame::Primary
                ? BookOfAnswersAssetId::ShakeLongerPrimary
                : BookOfAnswersAssetId::ShakeLongerSecondary));
    draw_transition_footer(
        canvas, "THE CRYSTAL NEEDS MORE TIME", "TRY AGAIN...");
}

void book_of_answers_page_render_message_result(Canvas &canvas,
                                                const char *answer,
                                                BookOfAnswersAnimationFrame frame)
{
    begin_page(canvas);
    draw_header(canvas);

    constexpr Rect kAnswerPanel = {0, 98, 480, 168};
    canvas.fill_rect(kAnswerPanel.x,
                     kAnswerPanel.y,
                     kAnswerPanel.width,
                     kAnswerPanel.height,
                     GrayLevel::Black);
    const WrappedAnswer wrapped = wrap_answer_text(answer);
    const int scale = answer_text_scale(wrapped);
    const int line_height = 7 * scale;
    const int line_gap = 3 * scale;
    const int text_height =
        static_cast<int>(wrapped.line_count) * line_height +
        static_cast<int>(wrapped.line_count - 1U) * line_gap;
    int line_y =
        kAnswerPanel.y + (kAnswerPanel.height - text_height) / 2;
    for (size_t line = 0U; line < wrapped.line_count; ++line) {
        draw_centered_text(canvas,
                           line_y,
                           wrapped.lines[line],
                           scale,
                           GrayLevel::White);
        line_y += line_height + line_gap;
    }

    pixel_asset_draw_centered(
        canvas,
        240,
        470,
        book_of_answers_asset(
            frame == BookOfAnswersAnimationFrame::Primary
                ? BookOfAnswersAssetId::MessageResult
                : BookOfAnswersAssetId::MessageResultAlt));
    canvas.fill_rect(32, 648, 190, 2, GrayLevel::Black);
    canvas.fill_rect(258, 648, 190, 2, GrayLevel::Black);
    draw_sparkle(canvas, 240, 649, 10);
    draw_result_controls(canvas);
}

void book_of_answers_page_render_crystal_result(Canvas &canvas,
                                                const char *answer,
                                                BookOfAnswersAnimationFrame frame)
{
    begin_page(canvas);
    draw_centered_text(canvas, 24, "BOOK OF ANSWERS", 4);
    draw_centered_text(canvas, 70, "THE CRYSTAL HAS DECIDED", 2);

    pixel_asset_draw_centered(
        canvas,
        240,
        380,
        book_of_answers_asset(
            frame == BookOfAnswersAnimationFrame::Primary
                ? BookOfAnswersAssetId::CrystalResultPrimary
                : BookOfAnswersAssetId::CrystalResultSecondary));

    constexpr int kBallCenterX = 240;
    constexpr int kBallCenterY = 330;
    const int answer_scale = std::max(
        8, fitted_text_scale(answer, 14, 360));
    const int answer_x =
        kBallCenterX - text_width(answer, answer_scale) / 2;
    const int answer_y =
        kBallCenterY - (7 * answer_scale) / 2;
    canvas.draw_text(answer_x,
                     answer_y,
                     answer,
                     static_cast<uint8_t>(answer_scale),
                     GrayLevel::Black);
    draw_result_controls(canvas);
}

BookOfAnswersAction book_of_answers_page_action_at(BookOfAnswersPage page,
                                                   int x,
                                                   int y)
{
    if (x < 0 || y < 0 || x >= kScreenWidth || y >= kScreenHeight) {
        return BookOfAnswersAction::None;
    }

    if (page == BookOfAnswersPage::Home) {
        if (kMessageModeRect.contains(x, y)) {
            return BookOfAnswersAction::SelectMessage;
        }
        if (kCrystalModeRect.contains(x, y)) {
            return BookOfAnswersAction::SelectCrystal;
        }
        return BookOfAnswersAction::None;
    }

    if (page == BookOfAnswersPage::MessageResult ||
        page == BookOfAnswersPage::CrystalResult) {
        if (kAskAgainRect.contains(x, y)) {
            return BookOfAnswersAction::AskAgain;
        }
        if (kEndRect.contains(x, y)) {
            return BookOfAnswersAction::End;
        }
    }
    return BookOfAnswersAction::None;
}
