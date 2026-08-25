#include "onboarding_pages.h"

#include <algorithm>

#include "canvas.h"
#include "onboarding_assets.h"
#include "pixel_asset.h"

namespace {

constexpr int kFooterTop = 714;
constexpr int kSkipRight = 160;
constexpr int kNextLeft = 320;

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
}

OnboardingAction onboarding_page_action_at(uint8_t page_index, int x, int y)
{
    if (y < kFooterTop || x < 0 || x >= 480 || y >= 800) {
        return OnboardingAction::None;
    }
    if (x < kSkipRight) {
        return OnboardingAction::Skip;
    }
    if (x >= kNextLeft) {
        return page_index + 1U >= kOnboardingPageCount
                   ? OnboardingAction::Finish
                   : OnboardingAction::Next;
    }
    return OnboardingAction::None;
}
