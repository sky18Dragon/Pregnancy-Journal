#include "onboarding_pages.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "canvas.h"
#include "font.h"
#include "onboarding_assets.h"
#include "pixel_asset.h"

namespace {

constexpr int kFooterTop = 714;
constexpr int kPreviousRight = 190;
constexpr int kNextLeft = 320;
constexpr int kPrimaryNavigationBottom = 765;
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

int text_width(const char *text, uint8_t scale)
{
    if (text == nullptr || text[0] == '\0') {
        return 0;
    }
    return static_cast<int>(std::strlen(text)) *
               (kFontWidth + kFontSpacing) * scale -
           kFontSpacing * scale;
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
        canvas.draw_line(27, 744, 37, 734, GrayLevel::Black);
        canvas.draw_line(27, 744, 37, 754, GrayLevel::Black);
        draw_centered(canvas, 72, 734, "BACK", 2);
    }

    char page_label[12] = {};
    std::snprintf(page_label,
                  sizeof(page_label),
                  "%u / %u",
                  static_cast<unsigned>(page_index + 1U),
                  static_cast<unsigned>(kOnboardingPageCount));
    draw_centered(canvas, 240, 734, page_label, 2);

    const bool final_page = page_index + 1U >= kOnboardingPageCount;
    draw_centered(canvas,
                  final_page ? 385 : 390,
                  734,
                  final_page ? "START" : "NEXT",
                  2);
    if (!final_page) {
        canvas.draw_line(442, 734, 452, 744, GrayLevel::Black);
        canvas.draw_line(452, 744, 442, 754, GrayLevel::Black);
    }

    constexpr const char *kSkipLabel = "SKIP TUTORIAL";
    const int skip_width = text_width(kSkipLabel, 1);
    const int skip_x = 240 - skip_width / 2;
    canvas.draw_text(skip_x, 773, kSkipLabel, 1);
    canvas.fill_rect(skip_x, 784, skip_width, 1, GrayLevel::Black);
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
