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
constexpr int kSkipRight = 160;
constexpr int kNextLeft = 320;
constexpr int kFooterContentTop = 724;
constexpr int kFooterContentBottom = 791;

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

// Replaces the baked footer with navigation that reflects the current page.
// 使用与当前页状态一致的导航栏覆盖设计稿中的静态底栏。
void draw_navigation_footer(Canvas &canvas, uint8_t page_index)
{
    canvas.fill_rect(12,
                     kFooterContentTop,
                     456,
                     kFooterContentBottom - kFooterContentTop,
                     GrayLevel::White);
    canvas.fill_rect(12, 722, 456, 3, GrayLevel::Black);

    const bool first_page = page_index == 0U;
    if (!first_page) {
        canvas.draw_line(28, 754, 38, 744, GrayLevel::Black);
        canvas.draw_line(28, 754, 38, 764, GrayLevel::Black);
    }
    draw_centered(canvas,
                  first_page ? 67 : 82,
                  744,
                  first_page ? "EXIT" : "BACK",
                  2);

    char page_label[12] = {};
    std::snprintf(page_label,
                  sizeof(page_label),
                  "%u / %u",
                  static_cast<unsigned>(page_index + 1U),
                  static_cast<unsigned>(kOnboardingPageCount));
    draw_centered(canvas, 240, 744, page_label, 2);

    const bool final_page = page_index + 1U >= kOnboardingPageCount;
    draw_centered(canvas,
                  final_page ? 385 : 390,
                  744,
                  final_page ? "START" : "NEXT",
                  2);
    if (!final_page) {
        canvas.draw_line(442, 744, 452, 754, GrayLevel::Black);
        canvas.draw_line(452, 754, 442, 764, GrayLevel::Black);
    }
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
    if (x < kSkipRight) {
        return page_index == 0U ? OnboardingAction::Skip
                                : OnboardingAction::Previous;
    }
    if (x >= kNextLeft) {
        return page_index + 1U >= kOnboardingPageCount
                   ? OnboardingAction::Finish
                   : OnboardingAction::Next;
    }
    return OnboardingAction::None;
}
