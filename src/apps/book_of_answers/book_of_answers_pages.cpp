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
constexpr Rect kStartRect = {40, 700, 400, 64};
constexpr Rect kAskAgainRect = {40, 680, 400, 64};
constexpr Rect kEndRect = {120, 748, 240, 52};

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
    draw_button(canvas, kStartRect, status, true, 4);
}

void draw_result_controls(Canvas &canvas)
{
    draw_button(canvas, kAskAgainRect, "ASK AGAIN", true, 4);
    draw_centered_text(canvas, 764, "END", 3);
}

}  // namespace

void book_of_answers_page_render_home(Canvas &canvas,
                                      BookOfAnswersMode mode)
{
    begin_page(canvas);
    draw_header(canvas);
    draw_centered_text(canvas, 116, "THINK OF A QUESTION", 3);
    draw_centered_text(canvas, 154, "IN YOUR HEART", 3);

    pixel_asset_draw_centered(
        canvas,
        240,
        390,
        book_of_answers_asset(BookOfAnswersAssetId::Home));

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
    draw_button(canvas, kStartRect, "SHAKE TO ASK", true, 4);
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

void book_of_answers_page_render_thinking(Canvas &canvas)
{
    begin_page(canvas);
    draw_header(canvas);
    draw_centered_text(canvas, 116, "HOLD STILL", 3);
    draw_centered_text(canvas, 154, "LET THE CRYSTAL SETTLE", 2);
    pixel_asset_draw_centered(
        canvas,
        240,
        390,
        book_of_answers_asset(BookOfAnswersAssetId::Thinking));
    draw_transition_footer(canvas, "KEEP THE DEVICE STEADY", "THINKING...");
}

void book_of_answers_page_render_revealing(Canvas &canvas)
{
    begin_page(canvas);
    draw_header(canvas);
    draw_centered_text(canvas, 116, "THE CRYSTAL", 3);
    draw_centered_text(canvas, 154, "HAS DECIDED", 3);
    pixel_asset_draw_centered(
        canvas,
        240,
        390,
        book_of_answers_asset(BookOfAnswersAssetId::Revealing));
    draw_transition_footer(canvas, "YOUR ANSWER IS READY", "REVEALING...");
}

void book_of_answers_page_render_message_result(Canvas &canvas,
                                                const char *first_line,
                                                const char *second_line)
{
    begin_page(canvas);
    draw_header(canvas);

    constexpr Rect kAnswerPanel = {0, 98, 480, 168};
    canvas.fill_rect(kAnswerPanel.x,
                     kAnswerPanel.y,
                     kAnswerPanel.width,
                     kAnswerPanel.height,
                     GrayLevel::Black);
    const bool two_lines = second_line != nullptr && second_line[0] != '\0';
    if (two_lines) {
        const int first_scale = fitted_text_scale(first_line, 5, 420);
        const int second_scale = fitted_text_scale(second_line, 5, 420);
        draw_centered_text(canvas,
                           124,
                           first_line,
                           first_scale,
                           GrayLevel::White);
        draw_centered_text(canvas,
                           190,
                           second_line,
                           second_scale,
                           GrayLevel::White);
    } else {
        const int scale = fitted_text_scale(first_line, 6, 420);
        draw_centered_text(canvas,
                           160,
                           first_line,
                           scale,
                           GrayLevel::White);
    }

    pixel_asset_draw_centered(
        canvas,
        240,
        470,
        book_of_answers_asset(BookOfAnswersAssetId::MessageResult));
    canvas.fill_rect(32, 648, 190, 2, GrayLevel::Black);
    canvas.fill_rect(258, 648, 190, 2, GrayLevel::Black);
    draw_sparkle(canvas, 240, 649, 10);
    draw_result_controls(canvas);
}

void book_of_answers_page_render_crystal_result(Canvas &canvas,
                                                const char *answer)
{
    begin_page(canvas);
    draw_centered_text(canvas, 24, "BOOK OF ANSWERS", 4);
    draw_centered_text(canvas, 70, "THE CRYSTAL HAS DECIDED", 2);

    constexpr int kBallCenterX = 225;
    constexpr int kBallCenterY = 340;
    constexpr int kBallRadius = 205;
    canvas.draw_circle(kBallCenterX,
                       kBallCenterY,
                       kBallRadius,
                       GrayLevel::Black);
    canvas.draw_circle(kBallCenterX,
                       kBallCenterY,
                       kBallRadius - 1,
                       GrayLevel::Black);
    draw_sparkle(canvas, 125, 210, 12);
    draw_sparkle(canvas, 310, 470, 10);

    const int answer_scale = std::max(
        8, fitted_text_scale(answer, 14, 360));
    draw_centered_text(canvas, 320, answer, answer_scale);

    canvas.draw_line(135, 548, 315, 548, GrayLevel::Black);
    canvas.draw_line(150, 566, 300, 566, GrayLevel::Black);
    canvas.draw_line(135, 548, 150, 566, GrayLevel::Black);
    canvas.draw_line(315, 548, 300, 566, GrayLevel::Black);
    canvas.fill_rect(165, 567, 120, 8, GrayLevel::Black);

    pixel_asset_draw_centered(
        canvas,
        415,
        480,
        book_of_answers_asset(BookOfAnswersAssetId::CrystalPeek));
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
        if (kStartRect.contains(x, y)) {
            return BookOfAnswersAction::Start;
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
