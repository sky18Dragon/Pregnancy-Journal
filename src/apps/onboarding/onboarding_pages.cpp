#include "onboarding_pages.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "canvas.h"
#include "font.h"
#include "onboarding_assets.h"
#include "pixel_asset.h"
#include "ui_language.h"

namespace {

constexpr int kFooterTop = 714;
constexpr int kPreviousRight = 190;
constexpr int kNextLeft = 320;
constexpr int kPrimaryNavigationBottom = 762;
constexpr int kSkipLeft = 120;
constexpr int kSkipRight = 360;
constexpr int kFooterPanelTop = 714;
constexpr int kFooterPanelBottom = 798;
constexpr int kFrameSampleY = 700;

int find_left_frame_edge(const Canvas &canvas)
{
    for (int x = 0; x < 32; ++x) {
        if (canvas.pixel_at(x, kFrameSampleY) == GrayLevel::Black) {
            return x;
        }
    }
    return 10;
}

int find_right_frame_edge(const Canvas &canvas)
{
    for (int x = 479; x >= 448; --x) {
        if (canvas.pixel_at(x, kFrameSampleY) == GrayLevel::Black) {
            return x;
        }
    }
    return 469;
}

// Draws one three-pixel outer frame above the shared footer.
// 在共享底栏上方绘制统一的三像素页面外框。
void draw_page_frame(Canvas &canvas)
{
    const int left = find_left_frame_edge(canvas);
    const int right = find_right_frame_edge(canvas);
    canvas.fill_rect(left, 18, 3, kFooterPanelTop - 18,
                     GrayLevel::Black);
    canvas.fill_rect(right - 2, 18, 3, kFooterPanelTop - 18,
                     GrayLevel::Black);
    canvas.fill_rect(left + 9, 9, right - left - 17, 3,
                     GrayLevel::Black);
    canvas.draw_line(left, 18, left + 9, 9, GrayLevel::Black);
    canvas.draw_line(left + 1, 18, left + 10, 9, GrayLevel::Black);
    canvas.draw_line(right, 18, right - 9, 9, GrayLevel::Black);
    canvas.draw_line(right - 1, 18, right - 10, 9,
                     GrayLevel::Black);
}

int text_width(const char *text, uint8_t scale)
{
    return ui_text_width(text, scale);
}

void draw_centered(Canvas &canvas,
                   int center_x,
                   int top,
                   const char *text,
                   uint8_t scale)
{
    canvas.draw_text(center_x - text_width(text, scale) / 2,
                     top,
                     text,
                     scale);
}

void draw_page_badge(Canvas &canvas, uint8_t page_index)
{
    char label[2] = {
        static_cast<char>('1' + page_index),
        '\0',
    };
    canvas.fill_circle(52, 58, 25, GrayLevel::Black);
    canvas.draw_text(44, 48, label, 3, GrayLevel::White);
}

// Draws the rabbit's introductory speech above the redrawn illustration.
// 在重绘插画上方绘制兔子的开场对白。
void draw_page_one_intro(Canvas &canvas)
{
    draw_centered(canvas, 280, 38, "HELLO, I'M STICKY!", 3);

    constexpr int kBubbleLeft = 96;
    constexpr int kBubbleRight = 414;
    constexpr int kBubbleTop = 94;
    constexpr int kBubbleBottom = 221;
    constexpr int kCorner = 10;

    canvas.fill_rect(kBubbleLeft + kCorner,
                     kBubbleTop,
                     kBubbleRight - kBubbleLeft - kCorner * 2,
                     3,
                     GrayLevel::Black);
    canvas.fill_rect(kBubbleLeft + kCorner,
                     kBubbleBottom - 2,
                     150,
                     3,
                     GrayLevel::Black);
    canvas.fill_rect(kBubbleLeft,
                     kBubbleTop + kCorner,
                     3,
                     kBubbleBottom - kBubbleTop - kCorner * 2,
                     GrayLevel::Black);
    canvas.fill_rect(kBubbleRight - 2,
                     kBubbleTop + kCorner,
                     3,
                     kBubbleBottom - kBubbleTop - kCorner * 2,
                     GrayLevel::Black);
    canvas.draw_line(kBubbleLeft,
                     kBubbleTop + kCorner,
                     kBubbleLeft + kCorner,
                     kBubbleTop,
                     GrayLevel::Black);
    canvas.draw_line(kBubbleRight,
                     kBubbleTop + kCorner,
                     kBubbleRight - kCorner,
                     kBubbleTop,
                     GrayLevel::Black);
    canvas.draw_line(kBubbleLeft,
                     kBubbleBottom - kCorner,
                     kBubbleLeft + kCorner,
                     kBubbleBottom,
                     GrayLevel::Black);
    canvas.draw_line(kBubbleRight,
                     kBubbleBottom - kCorner,
                     kBubbleRight - kCorner,
                     kBubbleBottom,
                     GrayLevel::Black);
    canvas.fill_rect(kBubbleRight - 160,
                     kBubbleBottom - 2,
                     160,
                     3,
                     GrayLevel::Black);
    canvas.draw_line(246, kBubbleBottom, 264, 245, GrayLevel::Black);
    canvas.draw_line(264, 245, 276, kBubbleBottom, GrayLevel::Black);
    canvas.draw_line(247, kBubbleBottom, 264, 242, GrayLevel::Black);
    canvas.draw_line(264, 242, 275, kBubbleBottom, GrayLevel::Black);

    draw_centered(canvas, 255, 118, "LET'S GROW TOGETHER!", 2);
    draw_centered(canvas, 255, 154, "I'LL SHOW YOU", 2);
    draw_centered(canvas, 255, 180, "HOW TO USE STICKY.", 2);
}

// Replaces page-two image text with crisp firmware-rendered labels.
// 使用清晰的固件字体重新绘制第二页的指标说明。
void draw_page_two_copy(Canvas &canvas)
{
    canvas.fill_rect(82, 25, 356, 45, GrayLevel::White);
    draw_centered(canvas, 276, 36, "CARE FOR YOUR PET", 3);

    canvas.fill_rect(84, 80, 320, 65, GrayLevel::White);
    canvas.fill_rect(100, 83, 288, 2, GrayLevel::Black);
    canvas.fill_rect(100, 122, 128, 2, GrayLevel::Black);
    canvas.fill_rect(252, 122, 136, 2, GrayLevel::Black);
    canvas.fill_rect(92, 91, 2, 25, GrayLevel::Black);
    canvas.fill_rect(396, 91, 2, 25, GrayLevel::Black);
    canvas.draw_line(92, 91, 100, 83, GrayLevel::Black);
    canvas.draw_line(93, 91, 101, 83, GrayLevel::Black);
    canvas.draw_line(396, 91, 388, 83, GrayLevel::Black);
    canvas.draw_line(397, 91, 389, 83, GrayLevel::Black);
    canvas.draw_line(92, 116, 100, 124, GrayLevel::Black);
    canvas.draw_line(93, 116, 101, 124, GrayLevel::Black);
    canvas.draw_line(396, 116, 388, 124, GrayLevel::Black);
    canvas.draw_line(397, 116, 389, 124, GrayLevel::Black);
    canvas.draw_line(228, 123, 240, 140, GrayLevel::Black);
    canvas.draw_line(229, 123, 240, 138, GrayLevel::Black);
    canvas.draw_line(240, 140, 252, 123, GrayLevel::Black);
    canvas.draw_line(240, 138, 251, 123, GrayLevel::Black);
    draw_centered(canvas, 240, 88, "KNOW WHAT EACH VALUE", 2);
    draw_centered(canvas, 240, 105, "MEANS", 2);

    canvas.fill_rect(58, 156, 206, 52, GrayLevel::White);
    canvas.draw_text(66, 163, "GROWTH", 3);
    canvas.draw_text(66, 191, "UNLOCKS NEW LIFE STAGES", 1);

    canvas.fill_rect(54, 358, 76, 33, GrayLevel::White);
    canvas.fill_rect(15, 391, 160, 23, GrayLevel::White);
    canvas.fill_rect(126, 369, 60, 32, GrayLevel::White);
    canvas.draw_text(59, 365, "LOVE", 3);
    canvas.draw_text(59, 402, "DEEPENS YOUR BOND", 1);
    pixel_asset_draw(canvas, 138, 373, onboarding_love_arrow_asset());

    canvas.fill_rect(52, 566, 184, 48, GrayLevel::White);
    canvas.draw_text(58, 566, "FULLNESS", 3);
    canvas.draw_text(58, 595, "SHOWS WHEN TO FEED", 1);

    canvas.fill_rect(278, 640, 188, 64, GrayLevel::White);
    canvas.draw_text(324, 655, "ENERGY", 3);
    canvas.draw_text(281, 685, "REQUIRED FOR INTERACTIONS", 1);
}

// Draws page-three growth guidance around the three illustrated outcomes.
// 在三组成长结果插画周围绘制第三页的说明文字。
void draw_page_three_copy(Canvas &canvas)
{
    draw_centered(canvas, 278, 36, "YOUR CARE SHAPES ITS GROWTH", 2);
    draw_centered(canvas,
                  240,
                  74,
                  "DAILY CHOICES BUILD ITS PERSONALITY",
                  1);

    draw_centered(canvas, 126, 282, "FEED OFTEN", 2);
    draw_centered(canvas, 348, 282, "FOODIE", 2);
    draw_centered(canvas, 126, 492, "PET OFTEN", 2);
    draw_centered(canvas, 348, 492, "AFFECTIONATE", 2);
    draw_centered(canvas, 126, 682, "PLAY OFTEN", 2);
    draw_centered(canvas, 348, 682, "ACTIVE", 2);
}

// Draws representative app names under the redrawn launcher illustrations.
// 在重绘的启动器插画下方绘制代表性应用名称。
void draw_page_four_copy(Canvas &canvas)
{
    draw_centered(canvas, 278, 38, "CHOOSE AN APP", 3);
    draw_centered(canvas, 240, 86, "EXPLORE STICKY APPS", 1);

    draw_centered(canvas, 128, 365, "Pet", 2);
    draw_centered(canvas, 128, 387, "YOUR LITTLE ONE IS HERE", 1);
    draw_centered(canvas, 352, 365, "Pomodoro Time", 2);
    draw_centered(canvas, 352, 387, "IT'S TIME TO FOCUS", 1);

    draw_centered(canvas, 128, 610, "Status Board", 2);
    draw_centered(canvas, 128, 633, "SHOW YOUR STATUS", 1);
    draw_centered(canvas, 352, 610, "Answers of book", 2);
    draw_centered(canvas,
                  352,
                  633,
                  "ASK WITHIN, THE BOOK ANSWERS",
                  1);
}

// Labels the redrawn launcher gestures and physical-button actions.
// 为重绘的启动器手势与实体按键动作添加说明。
void draw_page_five_copy(Canvas &canvas)
{
    draw_centered(canvas, 278, 36, "FIND YOUR APPS", 3);
    draw_centered(canvas,
                  240,
                  82,
                  "OPEN, CLOSE, RETURN HOME, OR SLEEP",
                  1);

    draw_centered(canvas, 258, 145, "SWIPE DOWN", 2);
    draw_centered(canvas, 258, 164, "TO CLOSE", 2);

    draw_centered(canvas, 396, 276, "AI KEY", 2);
    draw_centered(canvas, 396, 298, "TAP: OPEN", 1);
    draw_centered(canvas, 396, 313, "DOUBLE TAP: PET", 1);

    draw_centered(canvas, 390, 448, "HOLD BOTH", 2);
    draw_centered(canvas, 390, 470, "SIDE KEYS", 1);
    draw_centered(canvas, 390, 485, "TO SLEEP", 1);

    draw_centered(canvas, 143, 478, "TAP AN APP", 1);
    draw_centered(canvas, 143, 491, "TO LAUNCH", 1);

    draw_centered(canvas, 258, 578, "SWIPE UP", 2);
    draw_centered(canvas, 258, 597, "TO OPEN", 2);
}

// Labels the rotation and shake gestures shown on the final tutorial page.
// 为教程最后一页的旋转与摇晃手势添加说明。
void draw_page_six_copy(Canvas &canvas)
{
    draw_centered(canvas, 278, 36, "TURN TO CHOOSE", 3);
    draw_centered(canvas,
                  240,
                  82,
                  "OPEN THE LAUNCHER, THEN MOVE STICKY",
                  1);
    draw_centered(canvas, 240, 256, "LANDSCAPE -> PORTRAIT", 2);
    draw_centered(canvas, 240, 278, "OPENS POMODORO TIME", 1);
    draw_centered(canvas, 240, 446, "PORTRAIT -> LANDSCAPE", 2);
    draw_centered(canvas, 240, 468, "OPENS STATUS BOARD", 1);
    draw_centered(canvas, 240, 650, "SHAKE STICKY", 2);
    draw_centered(canvas, 240, 672, "OPENS ANSWERS OF BOOK", 1);
}

// Rebuilds one consistent footer panel and reconnects it to the page frame.
// 重建统一的底栏面板，并将它与页面外框连续连接。
void draw_navigation_footer(Canvas &canvas, uint8_t page_index)
{
    const int panel_left = find_left_frame_edge(canvas);
    const int panel_right = find_right_frame_edge(canvas);
    const int clear_left = std::max(0, panel_left - 1);
    const int clear_right = std::min(480, panel_right + 2);

    canvas.fill_rect(clear_left,
                     kFooterPanelTop,
                     clear_right - clear_left,
                     kFooterPanelBottom - kFooterPanelTop,
                     GrayLevel::White);
    canvas.fill_rect(panel_left,
                     kFooterPanelTop,
                     panel_right - panel_left + 1,
                     3,
                     GrayLevel::Black);
    canvas.fill_rect(panel_left,
                     kFooterPanelTop,
                     3,
                     74,
                     GrayLevel::Black);
    canvas.fill_rect(panel_right - 2,
                     kFooterPanelTop,
                     3,
                     74,
                     GrayLevel::Black);
    canvas.draw_line(panel_left, 787,
                     panel_left + 9, 796, GrayLevel::Black);
    canvas.draw_line(panel_left + 1, 787,
                     panel_left + 10, 796, GrayLevel::Black);
    canvas.draw_line(panel_right, 787,
                     panel_right - 9, 796, GrayLevel::Black);
    canvas.draw_line(panel_right - 1, 787,
                     panel_right - 10, 796, GrayLevel::Black);
    canvas.fill_rect(panel_left + 9,
                     795,
                     panel_right - panel_left - 17,
                     3,
                     GrayLevel::Black);

    const bool first_page = page_index == 0U;
    if (!first_page) {
        canvas.draw_line(24, 738, 36, 726, GrayLevel::Black);
        canvas.draw_line(24, 739, 36, 751, GrayLevel::Black);
        canvas.draw_line(25, 738, 37, 726, GrayLevel::Black);
        canvas.draw_line(25, 739, 37, 751, GrayLevel::Black);
        draw_centered(canvas, 80, 726, "BACK", 3);
    }

    char page_label[12] = {};
    std::snprintf(page_label,
                  sizeof(page_label),
                  "%u / %u",
                  static_cast<unsigned>(page_index + 1U),
                  static_cast<unsigned>(kOnboardingPageCount));
    draw_centered(canvas, 240, 726, page_label, 3);

    const bool final_page = page_index + 1U >= kOnboardingPageCount;
    draw_centered(canvas,
                  390,
                  726,
                  final_page ? "START" : "NEXT",
                  3);
    if (!final_page) {
        canvas.draw_line(445, 726, 457, 738, GrayLevel::Black);
        canvas.draw_line(457, 739, 445, 751, GrayLevel::Black);
        canvas.draw_line(446, 726, 458, 738, GrayLevel::Black);
        canvas.draw_line(458, 739, 446, 751, GrayLevel::Black);
    }

    constexpr const char *kSkipLabel = "SKIP TUTORIAL";
    const int skip_width = text_width(kSkipLabel, 2);
    const int skip_x = 240 - skip_width / 2;
    canvas.draw_text(skip_x, 766, kSkipLabel, 2);
    canvas.fill_rect(skip_x, 784, skip_width, 2, GrayLevel::Black);
}

}  // namespace

void onboarding_page_render(Canvas &canvas, uint8_t page_index)
{
    canvas.set_rotation(CanvasRotation::Deg90CounterClockwise);
    canvas.clear(GrayLevel::White);

    // Each page is one complete approved image converted to a 1-bit bitmap.
    // 每一页都是将已确认的完整设计稿直接转换得到的1位位图。
    const uint8_t safe_page = std::min<uint8_t>(
        page_index, static_cast<uint8_t>(kOnboardingAssetCount - 1U));
    pixel_asset_draw(canvas, 0, 0, onboarding_asset(safe_page));
    draw_page_badge(canvas, safe_page);
    if (safe_page == 0U) {
        draw_page_one_intro(canvas);
    } else if (safe_page == 1U) {
        draw_page_two_copy(canvas);
    } else if (safe_page == 2U) {
        draw_page_three_copy(canvas);
    } else if (safe_page == 3U) {
        draw_page_four_copy(canvas);
    } else if (safe_page == 4U) {
        draw_page_five_copy(canvas);
    } else if (safe_page == 5U) {
        draw_page_six_copy(canvas);
    }
    draw_page_frame(canvas);
    draw_navigation_footer(canvas, safe_page);
}

OnboardingAction onboarding_page_action_at(uint8_t page_index, int x, int y)
{
    if (y < kFooterTop || x < 0 || x >= 480 || y >= 800) {
        return OnboardingAction::None;
    }
    if (y >= kPrimaryNavigationBottom) {
        return x >= kSkipLeft && x < kSkipRight
                   ? OnboardingAction::Skip
                   : OnboardingAction::None;
    }
    if (page_index > 0U && x < kPreviousRight) {
        return OnboardingAction::Previous;
    }
    if (x >= kNextLeft) {
        return page_index + 1U >= kOnboardingPageCount
                   ? OnboardingAction::Finish
                   : OnboardingAction::Next;
    }
    return OnboardingAction::None;
}
