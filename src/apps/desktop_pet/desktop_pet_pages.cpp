#include "desktop_pet_pages.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "canvas.h"
#include "desktop_pet_assets.h"
#include "pixel_asset.h"

namespace {

constexpr int kScreenWidth = 480;

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

constexpr Rect kTestBadgeRect = {390, 12, 90, 72};
constexpr Rect kEggBodyRect = {55, 155, 370, 450};
constexpr Rect kFeedRect = {0, 600, 160, 170};
constexpr Rect kTalkRect = {160, 600, 160, 170};
constexpr Rect kPlayRect = {320, 600, 160, 170};
constexpr Rect kPetBodyRect = {110, 270, 260, 320};
constexpr Rect kNameRect = {0, 0, 350, 70};

constexpr Rect kCloseTestRect = {360, 20, 100, 55};
constexpr Rect kNextDayRect = {35, 215, 410, 52};
constexpr Rect kAddGrowthRect = {35, 277, 410, 52};
constexpr Rect kAddLoveRect = {35, 339, 410, 52};
constexpr Rect kReduceEnergyRect = {35, 401, 410, 52};
constexpr Rect kSleepNowRect = {35, 463, 410, 52};
constexpr Rect kGoOutRect = {35, 525, 410, 52};
constexpr Rect kResetRect = {35, 597, 410, 62};
constexpr Rect kOutingCallHomeRect = {0, 600, 480, 170};
constexpr Rect kFoodieChoiceRect = {45, 220, 390, 125};
constexpr Rect kAffectionateChoiceRect = {45, 365, 390, 125};
constexpr Rect kActiveChoiceRect = {45, 510, 390, 125};
constexpr Rect kSleepWakeRect = {0, 0, 480, 800};

constexpr Rect kNameBackRect = {0, 0, 92, 102};
constexpr Rect kNameInputRect = {25, 112, 430, 112};
constexpr Rect kNameLetterRow1[] = {
    {10, 270, 42, 64}, {56, 270, 42, 64}, {102, 270, 42, 64},
    {148, 270, 42, 64}, {194, 270, 42, 64}, {240, 270, 42, 64},
    {286, 270, 42, 64}, {332, 270, 42, 64}, {378, 270, 42, 64},
    {424, 270, 42, 64},
};
constexpr Rect kNameLetterRow2[] = {
    {33, 344, 42, 64}, {79, 344, 42, 64}, {125, 344, 42, 64},
    {171, 344, 42, 64}, {217, 344, 42, 64}, {263, 344, 42, 64},
    {309, 344, 42, 64}, {355, 344, 42, 64}, {401, 344, 42, 64},
};
constexpr Rect kNameLetterRow3[] = {
    {79, 418, 42, 64}, {125, 418, 42, 64}, {171, 418, 42, 64},
    {217, 418, 42, 64}, {263, 418, 42, 64}, {309, 418, 42, 64},
    {355, 418, 42, 64},
};
constexpr Rect kNameDigitRow1[] = {
    {25, 290, 78, 76}, {113, 290, 78, 76}, {201, 290, 78, 76},
    {289, 290, 78, 76}, {377, 290, 78, 76},
};
constexpr Rect kNameDigitRow2[] = {
    {25, 380, 78, 76}, {113, 380, 78, 76}, {201, 380, 78, 76},
    {289, 380, 78, 76}, {377, 380, 78, 76},
};
constexpr Rect kNameToggleRect = {15, 510, 105, 66};
constexpr Rect kNameSpaceRect = {128, 510, 210, 66};
constexpr Rect kNameDeleteRect = {346, 510, 119, 66};
constexpr Rect kNameClearRect = {15, 594, 130, 78};
constexpr Rect kNameApplyRect = {153, 594, 312, 78};

constexpr DesktopPetNameAction kNameLetterRow1Actions[] = {
    DesktopPetNameAction::KeyQ, DesktopPetNameAction::KeyW,
    DesktopPetNameAction::KeyE, DesktopPetNameAction::KeyR,
    DesktopPetNameAction::KeyT, DesktopPetNameAction::KeyY,
    DesktopPetNameAction::KeyU, DesktopPetNameAction::KeyI,
    DesktopPetNameAction::KeyO, DesktopPetNameAction::KeyP,
};
constexpr DesktopPetNameAction kNameLetterRow2Actions[] = {
    DesktopPetNameAction::KeyA, DesktopPetNameAction::KeyS,
    DesktopPetNameAction::KeyD, DesktopPetNameAction::KeyF,
    DesktopPetNameAction::KeyG, DesktopPetNameAction::KeyH,
    DesktopPetNameAction::KeyJ, DesktopPetNameAction::KeyK,
    DesktopPetNameAction::KeyL,
};
constexpr DesktopPetNameAction kNameLetterRow3Actions[] = {
    DesktopPetNameAction::KeyZ, DesktopPetNameAction::KeyX,
    DesktopPetNameAction::KeyC, DesktopPetNameAction::KeyV,
    DesktopPetNameAction::KeyB, DesktopPetNameAction::KeyN,
    DesktopPetNameAction::KeyM,
};
constexpr DesktopPetNameAction kNameDigitActions[] = {
    DesktopPetNameAction::Digit1, DesktopPetNameAction::Digit2,
    DesktopPetNameAction::Digit3, DesktopPetNameAction::Digit4,
    DesktopPetNameAction::Digit5, DesktopPetNameAction::Digit6,
    DesktopPetNameAction::Digit7, DesktopPetNameAction::Digit8,
    DesktopPetNameAction::Digit9, DesktopPetNameAction::Digit0,
};

int text_width(const char *text, int scale)
{
    if (text == nullptr || text[0] == '\0') {
        return 0;
    }
    return (static_cast<int>(std::strlen(text)) * 6 - 1) * scale;
}

void draw_centered(Canvas &canvas,
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

void draw_centered_in_rect(Canvas &canvas,
                           const Rect &rect,
                           const char *text,
                           int scale,
                           GrayLevel color = GrayLevel::Black)
{
    const int x = rect.x + (rect.width - text_width(text, scale)) / 2;
    const int y = rect.y + (rect.height - 7 * scale) / 2;
    canvas.draw_text(x, y, text, static_cast<uint8_t>(scale), color);
}

void draw_button(Canvas &canvas,
                 const Rect &rect,
                 const char *label,
                 bool filled)
{
    if (filled) {
        canvas.fill_rect(rect.x, rect.y, rect.width, rect.height,
                         GrayLevel::Black);
        draw_centered_in_rect(canvas, rect, label, 3, GrayLevel::White);
        return;
    }
    canvas.draw_rect(rect.x, rect.y, rect.width, rect.height,
                     GrayLevel::Black);
    canvas.draw_rect(rect.x + 2, rect.y + 2, rect.width - 4,
                     rect.height - 4, GrayLevel::Black);
    draw_centered_in_rect(canvas, rect, label, 3);
}

void draw_name_key(Canvas &canvas,
                   const Rect &rect,
                   const char *label,
                   int scale = 3)
{
    canvas.draw_rect(rect.x, rect.y, rect.width, rect.height,
                     GrayLevel::Black);
    canvas.draw_rect(rect.x + 2, rect.y + 2, rect.width - 4,
                     rect.height - 4, GrayLevel::Black);
    draw_centered_in_rect(canvas, rect, label, scale);
}

void draw_name_back_arrow(Canvas &canvas)
{
    canvas.draw_line(23, 48, 47, 28, GrayLevel::Black);
    canvas.draw_line(23, 48, 47, 68, GrayLevel::Black);
    canvas.draw_line(24, 49, 66, 49, GrayLevel::Black);
    canvas.draw_line(24, 52, 66, 52, GrayLevel::Black);
}

DesktopPetNameAction name_action_in_rects(
    const Rect *rects,
    const DesktopPetNameAction *actions,
    size_t count,
    int x,
    int y)
{
    for (size_t index = 0U; index < count; ++index) {
        if (rects[index].contains(x, y)) {
            return actions[index];
        }
    }
    return DesktopPetNameAction::None;
}

void draw_speech_bubble(Canvas &canvas, const char *message)
{
    constexpr int x = 95;
    constexpr int y = 135;
    constexpr int width = 290;
    constexpr int height = 92;
    constexpr int corner = 10;

    // A small chamfer creates the crisp rounded pixel shape in the reference.
    // 使用小倒角还原参考图中清晰的像素圆角气泡。
    canvas.draw_line(x + corner, y, x + width - corner, y);
    canvas.draw_line(x, y + corner, x, y + height - corner);
    canvas.draw_line(x + width, y + corner, x + width, y + height - corner);
    canvas.draw_line(x + corner, y + height, x + 132, y + height);
    canvas.draw_line(x + 158, y + height, x + width - corner, y + height);
    canvas.draw_line(x, y + corner, x + corner, y);
    canvas.draw_line(x + width - corner, y, x + width, y + corner);
    canvas.draw_line(x, y + height - corner, x + corner, y + height);
    canvas.draw_line(x + width, y + height - corner,
                     x + width - corner, y + height);
    canvas.draw_line(x + 132, y + height, x + 145, y + height + 24);
    canvas.draw_line(x + 145, y + height + 24, x + 158, y + height);

    char first_line[25] = {};
    char second_line[25] = {};
    const size_t length = std::strlen(message);
    if (length <= 20U) {
        std::snprintf(first_line, sizeof(first_line), "%s", message);
    } else {
        size_t split = std::min<size_t>(20U, length);
        while (split > 0U && message[split] != ' ') {
            --split;
        }
        if (split == 0U) {
            split = std::min<size_t>(20U, length);
        }
        std::snprintf(first_line, sizeof(first_line), "%.*s",
                      static_cast<int>(split), message);
        const char *remaining = message + split;
        while (*remaining == ' ') {
            ++remaining;
        }
        std::snprintf(second_line, sizeof(second_line), "%s", remaining);
    }

    const int first_y = second_line[0] == '\0' ? y + 35 : y + 22;
    canvas.draw_text(
        x + (width - text_width(first_line, 2)) / 2,
        first_y,
        first_line,
        2);
    if (second_line[0] != '\0') {
        canvas.draw_text(
            x + (width - text_width(second_line, 2)) / 2,
            y + 53,
            second_line,
            2);
    }
}

DesktopPetAssetId idle_frame_asset(DesktopPetIdleFrame frame)
{
    switch (frame) {
    case DesktopPetIdleFrame::Blink:
        return DesktopPetAssetId::IdleBlink;
    case DesktopPetIdleFrame::EarTwitch:
        return DesktopPetAssetId::IdleEarTwitch;
    case DesktopPetIdleFrame::LookAround:
        return DesktopPetAssetId::IdleLookAround;
    case DesktopPetIdleFrame::Stretch:
        return DesktopPetAssetId::IdleStretch;
    case DesktopPetIdleFrame::Hungry:
        return DesktopPetAssetId::IdleHungry;
    case DesktopPetIdleFrame::Tired:
        return DesktopPetAssetId::IdleTired;
    case DesktopPetIdleFrame::Normal:
    default:
        return DesktopPetAssetId::Idle;
    }
}

DesktopPetAssetId idle_frame_mask_asset(DesktopPetIdleFrame frame)
{
    switch (frame) {
    case DesktopPetIdleFrame::Blink:
        return DesktopPetAssetId::IdleBlinkMask;
    case DesktopPetIdleFrame::EarTwitch:
        return DesktopPetAssetId::IdleEarTwitchMask;
    case DesktopPetIdleFrame::LookAround:
        return DesktopPetAssetId::IdleLookAroundMask;
    case DesktopPetIdleFrame::Stretch:
        return DesktopPetAssetId::IdleStretchMask;
    case DesktopPetIdleFrame::Hungry:
        return DesktopPetAssetId::IdleHungryMask;
    case DesktopPetIdleFrame::Tired:
        return DesktopPetAssetId::IdleTiredMask;
    case DesktopPetIdleFrame::Normal:
    default:
        return DesktopPetAssetId::IdleMask;
    }
}

DesktopPetAssetId child_idle_frame_asset(DesktopPetIdleFrame frame)
{
    switch (frame) {
    case DesktopPetIdleFrame::Blink:
        return DesktopPetAssetId::ChildIdleBlink;
    case DesktopPetIdleFrame::EarTwitch:
        return DesktopPetAssetId::ChildIdleEarTwitch;
    case DesktopPetIdleFrame::LookAround:
        return DesktopPetAssetId::ChildIdleLookAround;
    case DesktopPetIdleFrame::Stretch:
        return DesktopPetAssetId::ChildIdleStretch;
    case DesktopPetIdleFrame::Hungry:
        return DesktopPetAssetId::ChildIdleHungry;
    case DesktopPetIdleFrame::Tired:
        return DesktopPetAssetId::ChildIdleTired;
    case DesktopPetIdleFrame::Normal:
    default:
        return DesktopPetAssetId::ChildIdle;
    }
}

DesktopPetAssetId child_idle_frame_mask_asset(DesktopPetIdleFrame frame)
{
    switch (frame) {
    case DesktopPetIdleFrame::Blink:
        return DesktopPetAssetId::ChildIdleBlinkMask;
    case DesktopPetIdleFrame::EarTwitch:
        return DesktopPetAssetId::ChildIdleEarTwitchMask;
    case DesktopPetIdleFrame::LookAround:
        return DesktopPetAssetId::ChildIdleLookAroundMask;
    case DesktopPetIdleFrame::Stretch:
        return DesktopPetAssetId::ChildIdleStretchMask;
    case DesktopPetIdleFrame::Hungry:
        return DesktopPetAssetId::ChildIdleHungryMask;
    case DesktopPetIdleFrame::Tired:
        return DesktopPetAssetId::ChildIdleTiredMask;
    case DesktopPetIdleFrame::Normal:
    default:
        return DesktopPetAssetId::ChildIdleMask;
    }
}

DesktopPetAssetId youth_idle_asset(PetPersonalityBranch branch,
                                   DesktopPetIdleFrame frame)
{
    const bool signature = frame != DesktopPetIdleFrame::Normal &&
                           frame != DesktopPetIdleFrame::Tired;
    switch (branch) {
    case PetPersonalityBranch::Foodie:
        if (frame == DesktopPetIdleFrame::Hungry) {
            return DesktopPetAssetId::YouthFoodieFeed;
        }
        return signature ? DesktopPetAssetId::YouthFoodieSignature
                         : DesktopPetAssetId::YouthFoodieIdle;
    case PetPersonalityBranch::Affectionate:
        if (frame == DesktopPetIdleFrame::Hungry) {
            return DesktopPetAssetId::YouthAffectionateFeed;
        }
        return signature ? DesktopPetAssetId::YouthAffectionateSignature
                         : DesktopPetAssetId::YouthAffectionateIdle;
    case PetPersonalityBranch::Active:
        if (frame == DesktopPetIdleFrame::Hungry) {
            return DesktopPetAssetId::YouthActiveFeed;
        }
        return signature ? DesktopPetAssetId::YouthActiveSignature
                         : DesktopPetAssetId::YouthActiveIdle;
    case PetPersonalityBranch::Undecided:
    default:
        return DesktopPetAssetId::ChildIdle;
    }
}

DesktopPetAssetId youth_idle_mask_asset(PetPersonalityBranch branch,
                                        DesktopPetIdleFrame frame)
{
    const bool signature = frame != DesktopPetIdleFrame::Normal &&
                           frame != DesktopPetIdleFrame::Tired;
    switch (branch) {
    case PetPersonalityBranch::Foodie:
        if (frame == DesktopPetIdleFrame::Hungry) {
            return DesktopPetAssetId::YouthFoodieFeedMask;
        }
        return signature ? DesktopPetAssetId::YouthFoodieSignatureMask
                         : DesktopPetAssetId::YouthFoodieIdleMask;
    case PetPersonalityBranch::Affectionate:
        if (frame == DesktopPetIdleFrame::Hungry) {
            return DesktopPetAssetId::YouthAffectionateFeedMask;
        }
        return signature
                   ? DesktopPetAssetId::YouthAffectionateSignatureMask
                   : DesktopPetAssetId::YouthAffectionateIdleMask;
    case PetPersonalityBranch::Active:
        if (frame == DesktopPetIdleFrame::Hungry) {
            return DesktopPetAssetId::YouthActiveFeedMask;
        }
        return signature ? DesktopPetAssetId::YouthActiveSignatureMask
                         : DesktopPetAssetId::YouthActiveIdleMask;
    case PetPersonalityBranch::Undecided:
    default:
        return DesktopPetAssetId::ChildIdleMask;
    }
}

// Maps one Adult branch and idle state to its generated character frame.
// 将成年路线和待机状态映射到对应的角色素材帧。
DesktopPetAssetId adult_idle_asset(PetPersonalityBranch branch,
                                   DesktopPetIdleFrame frame)
{
    const bool signature = frame != DesktopPetIdleFrame::Normal &&
                           frame != DesktopPetIdleFrame::Tired;
    switch (branch) {
    case PetPersonalityBranch::Foodie:
        if (frame == DesktopPetIdleFrame::Hungry) {
            return DesktopPetAssetId::AdultFoodieFeed;
        }
        return signature ? DesktopPetAssetId::AdultFoodieSignature
                         : DesktopPetAssetId::AdultFoodieIdle;
    case PetPersonalityBranch::Affectionate:
        if (frame == DesktopPetIdleFrame::Hungry) {
            return DesktopPetAssetId::AdultAffectionateFeed;
        }
        return signature ? DesktopPetAssetId::AdultAffectionateSignature
                         : DesktopPetAssetId::AdultAffectionateIdle;
    case PetPersonalityBranch::Active:
        if (frame == DesktopPetIdleFrame::Hungry) {
            return DesktopPetAssetId::AdultActiveFeed;
        }
        return signature ? DesktopPetAssetId::AdultActiveSignature
                         : DesktopPetAssetId::AdultActiveIdle;
    case PetPersonalityBranch::Undecided:
    default:
        return DesktopPetAssetId::YouthAffectionateIdle;
    }
}

DesktopPetAssetId adult_idle_mask_asset(PetPersonalityBranch branch,
                                        DesktopPetIdleFrame frame)
{
    const bool signature = frame != DesktopPetIdleFrame::Normal &&
                           frame != DesktopPetIdleFrame::Tired;
    switch (branch) {
    case PetPersonalityBranch::Foodie:
        if (frame == DesktopPetIdleFrame::Hungry) {
            return DesktopPetAssetId::AdultFoodieFeedMask;
        }
        return signature ? DesktopPetAssetId::AdultFoodieSignatureMask
                         : DesktopPetAssetId::AdultFoodieIdleMask;
    case PetPersonalityBranch::Affectionate:
        if (frame == DesktopPetIdleFrame::Hungry) {
            return DesktopPetAssetId::AdultAffectionateFeedMask;
        }
        return signature ? DesktopPetAssetId::AdultAffectionateSignatureMask
                         : DesktopPetAssetId::AdultAffectionateIdleMask;
    case PetPersonalityBranch::Active:
        if (frame == DesktopPetIdleFrame::Hungry) {
            return DesktopPetAssetId::AdultActiveFeedMask;
        }
        return signature ? DesktopPetAssetId::AdultActiveSignatureMask
                         : DesktopPetAssetId::AdultActiveIdleMask;
    case PetPersonalityBranch::Undecided:
    default:
        return DesktopPetAssetId::YouthAffectionateIdleMask;
    }
}

DesktopPetAssetId pose_asset(const PetCoreState &pet,
                             DesktopPetPose pose,
                             DesktopPetIdleFrame idle_frame)
{
    if (pet.stage == PetLifeStage::Adult) {
        switch (pet.branch) {
        case PetPersonalityBranch::Foodie:
            if (pose == DesktopPetPose::Feed) {
                return DesktopPetAssetId::AdultFoodieFeed;
            }
            if (pose == DesktopPetPose::Pet) {
                return DesktopPetAssetId::AdultFoodiePet;
            }
            if (pose == DesktopPetPose::Play) {
                return DesktopPetAssetId::AdultFoodiePlay;
            }
            break;
        case PetPersonalityBranch::Affectionate:
            if (pose == DesktopPetPose::Feed) {
                return DesktopPetAssetId::AdultAffectionateFeed;
            }
            if (pose == DesktopPetPose::Pet) {
                return DesktopPetAssetId::AdultAffectionatePet;
            }
            if (pose == DesktopPetPose::Play) {
                return DesktopPetAssetId::AdultAffectionatePlay;
            }
            break;
        case PetPersonalityBranch::Active:
            if (pose == DesktopPetPose::Feed) {
                return DesktopPetAssetId::AdultActiveFeed;
            }
            if (pose == DesktopPetPose::Pet) {
                return DesktopPetAssetId::AdultActivePet;
            }
            if (pose == DesktopPetPose::Play) {
                return DesktopPetAssetId::AdultActivePlay;
            }
            break;
        case PetPersonalityBranch::Undecided:
        default:
            break;
        }
        return adult_idle_asset(pet.branch, idle_frame);
    }
    if (pet.stage == PetLifeStage::Youth) {
        switch (pet.branch) {
        case PetPersonalityBranch::Foodie:
            if (pose == DesktopPetPose::Feed) {
                return DesktopPetAssetId::YouthFoodieFeed;
            }
            if (pose == DesktopPetPose::Pet) {
                return DesktopPetAssetId::YouthFoodiePet;
            }
            if (pose == DesktopPetPose::Play) {
                return DesktopPetAssetId::YouthFoodiePlay;
            }
            break;
        case PetPersonalityBranch::Affectionate:
            if (pose == DesktopPetPose::Feed) {
                return DesktopPetAssetId::YouthAffectionateFeed;
            }
            if (pose == DesktopPetPose::Pet) {
                return DesktopPetAssetId::YouthAffectionatePet;
            }
            if (pose == DesktopPetPose::Play) {
                return DesktopPetAssetId::YouthAffectionatePlay;
            }
            break;
        case PetPersonalityBranch::Active:
            if (pose == DesktopPetPose::Feed) {
                return DesktopPetAssetId::YouthActiveFeed;
            }
            if (pose == DesktopPetPose::Pet) {
                return DesktopPetAssetId::YouthActivePet;
            }
            if (pose == DesktopPetPose::Play) {
                return DesktopPetAssetId::YouthActivePlay;
            }
            break;
        case PetPersonalityBranch::Undecided:
        default:
            break;
        }
        return youth_idle_asset(pet.branch, idle_frame);
    }
    if (pet.stage == PetLifeStage::Child) {
        switch (pose) {
        case DesktopPetPose::Feed:
            return DesktopPetAssetId::ChildFeed;
        case DesktopPetPose::Pet:
            return DesktopPetAssetId::ChildPet;
        case DesktopPetPose::Play:
            return DesktopPetAssetId::ChildPlay;
        case DesktopPetPose::Idle:
        default:
            return child_idle_frame_asset(idle_frame);
        }
    }
    switch (pose) {
    case DesktopPetPose::Feed:
        return DesktopPetAssetId::Feed;
    case DesktopPetPose::Pet:
        return DesktopPetAssetId::Pet;
    case DesktopPetPose::Play:
        return DesktopPetAssetId::Play;
    case DesktopPetPose::Idle:
    default:
        return idle_frame_asset(idle_frame);
    }
}

DesktopPetAssetId pose_mask_asset(const PetCoreState &pet,
                                  DesktopPetPose pose,
                                  DesktopPetIdleFrame idle_frame)
{
    if (pet.stage == PetLifeStage::Adult) {
        switch (pet.branch) {
        case PetPersonalityBranch::Foodie:
            if (pose == DesktopPetPose::Feed) {
                return DesktopPetAssetId::AdultFoodieFeedMask;
            }
            if (pose == DesktopPetPose::Pet) {
                return DesktopPetAssetId::AdultFoodiePetMask;
            }
            if (pose == DesktopPetPose::Play) {
                return DesktopPetAssetId::AdultFoodiePlayMask;
            }
            break;
        case PetPersonalityBranch::Affectionate:
            if (pose == DesktopPetPose::Feed) {
                return DesktopPetAssetId::AdultAffectionateFeedMask;
            }
            if (pose == DesktopPetPose::Pet) {
                return DesktopPetAssetId::AdultAffectionatePetMask;
            }
            if (pose == DesktopPetPose::Play) {
                return DesktopPetAssetId::AdultAffectionatePlayMask;
            }
            break;
        case PetPersonalityBranch::Active:
            if (pose == DesktopPetPose::Feed) {
                return DesktopPetAssetId::AdultActiveFeedMask;
            }
            if (pose == DesktopPetPose::Pet) {
                return DesktopPetAssetId::AdultActivePetMask;
            }
            if (pose == DesktopPetPose::Play) {
                return DesktopPetAssetId::AdultActivePlayMask;
            }
            break;
        case PetPersonalityBranch::Undecided:
        default:
            break;
        }
        return adult_idle_mask_asset(pet.branch, idle_frame);
    }
    if (pet.stage == PetLifeStage::Youth) {
        switch (pet.branch) {
        case PetPersonalityBranch::Foodie:
            if (pose == DesktopPetPose::Feed) {
                return DesktopPetAssetId::YouthFoodieFeedMask;
            }
            if (pose == DesktopPetPose::Pet) {
                return DesktopPetAssetId::YouthFoodiePetMask;
            }
            if (pose == DesktopPetPose::Play) {
                return DesktopPetAssetId::YouthFoodiePlayMask;
            }
            break;
        case PetPersonalityBranch::Affectionate:
            if (pose == DesktopPetPose::Feed) {
                return DesktopPetAssetId::YouthAffectionateFeedMask;
            }
            if (pose == DesktopPetPose::Pet) {
                return DesktopPetAssetId::YouthAffectionatePetMask;
            }
            if (pose == DesktopPetPose::Play) {
                return DesktopPetAssetId::YouthAffectionatePlayMask;
            }
            break;
        case PetPersonalityBranch::Active:
            if (pose == DesktopPetPose::Feed) {
                return DesktopPetAssetId::YouthActiveFeedMask;
            }
            if (pose == DesktopPetPose::Pet) {
                return DesktopPetAssetId::YouthActivePetMask;
            }
            if (pose == DesktopPetPose::Play) {
                return DesktopPetAssetId::YouthActivePlayMask;
            }
            break;
        case PetPersonalityBranch::Undecided:
        default:
            break;
        }
        return youth_idle_mask_asset(pet.branch, idle_frame);
    }
    if (pet.stage == PetLifeStage::Child) {
        switch (pose) {
        case DesktopPetPose::Feed:
            return DesktopPetAssetId::ChildFeedMask;
        case DesktopPetPose::Pet:
            return DesktopPetAssetId::ChildPetMask;
        case DesktopPetPose::Play:
            return DesktopPetAssetId::ChildPlayMask;
        case DesktopPetPose::Idle:
        default:
            return child_idle_frame_mask_asset(idle_frame);
        }
    }
    switch (pose) {
    case DesktopPetPose::Feed:
        return DesktopPetAssetId::FeedMask;
    case DesktopPetPose::Pet:
        return DesktopPetAssetId::PetMask;
    case DesktopPetPose::Play:
        return DesktopPetAssetId::PlayMask;
    case DesktopPetPose::Idle:
    default:
        return idle_frame_mask_asset(idle_frame);
    }
}

void draw_progress(Canvas &canvas, uint16_t growth, uint16_t limit)
{
    constexpr int segment_count = 6;
    constexpr int segment_width = 39;
    constexpr int gap = 4;
    constexpr int x = 24;
    constexpr int y = 105;
    const int filled = std::min(
        segment_count,
        static_cast<int>((growth * segment_count + limit - 1U) / limit));
    for (int index = 0; index < segment_count; ++index) {
        const int segment_x = x + index * (segment_width + gap);
        if (index < filled) {
            canvas.fill_rect(segment_x, y, segment_width, 6,
                             GrayLevel::Black);
        } else {
            canvas.draw_rect(segment_x, y, segment_width, 6,
                             GrayLevel::Black);
        }
    }
}

void draw_action(Canvas &canvas,
                 int center_x,
                 DesktopPetAssetId icon,
                 const char *label)
{
    pixel_asset_draw_centered(canvas, center_x, 650,
                              desktop_pet_asset(icon));
    canvas.draw_text(center_x - text_width(label, 3) / 2,
                     715, label, 3);
}

void draw_sparkle(Canvas &canvas, int x, int y, int radius)
{
    canvas.draw_line(x - radius, y, x + radius, y, GrayLevel::Black);
    canvas.draw_line(x, y - radius, x, y + radius, GrayLevel::Black);
    canvas.fill_rect(x - 1, y - 1, 3, 3, GrayLevel::Black);
}

DesktopPetAssetId egg_asset(const DesktopPetState &state,
                            DesktopPetHatchFrame frame)
{
    if (frame == DesktopPetHatchFrame::Opened) {
        return DesktopPetAssetId::EggOpen;
    }
    if (frame == DesktopPetHatchFrame::WobbleLeft &&
        state.hatch_taps == 1U) {
        return DesktopPetAssetId::EggWobbleLeft;
    }
    if (frame == DesktopPetHatchFrame::WobbleRight &&
        state.hatch_taps == 1U) {
        return DesktopPetAssetId::EggWobbleRight;
    }
    if ((frame == DesktopPetHatchFrame::WobbleLeft ||
         frame == DesktopPetHatchFrame::WobbleRight) &&
        state.hatch_taps == 2U) {
        return DesktopPetAssetId::EggCrackOne;
    }
    if (state.hatch_taps >= kDesktopPetRequiredHatchTaps) {
        return DesktopPetAssetId::EggCrackTwo;
    }
    if (state.hatch_taps >= 2U) {
        return DesktopPetAssetId::EggCrackTwo;
    }
    if (state.hatch_taps >= 1U) {
        return DesktopPetAssetId::EggCrackOne;
    }
    return DesktopPetAssetId::EggIntact;
}

DesktopPetAssetId egg_mask_asset(DesktopPetAssetId asset)
{
    switch (asset) {
    case DesktopPetAssetId::EggWobbleLeft:
        return DesktopPetAssetId::EggWobbleLeftMask;
    case DesktopPetAssetId::EggWobbleRight:
        return DesktopPetAssetId::EggWobbleRightMask;
    case DesktopPetAssetId::EggCrackOne:
        return DesktopPetAssetId::EggCrackOneMask;
    case DesktopPetAssetId::EggCrackTwo:
        return DesktopPetAssetId::EggCrackTwoMask;
    case DesktopPetAssetId::EggOpen:
        return DesktopPetAssetId::EggOpenMask;
    case DesktopPetAssetId::EggIntact:
    default:
        return DesktopPetAssetId::EggIntactMask;
    }
}

int egg_frame_offset(const DesktopPetState &state,
                     DesktopPetHatchFrame frame)
{
    if (state.hatch_taps <= 1U) {
        return 0;
    }
    if (frame == DesktopPetHatchFrame::WobbleLeft) {
        return state.hatch_taps >= kDesktopPetRequiredHatchTaps ? -18 : -12;
    }
    if (frame == DesktopPetHatchFrame::WobbleRight) {
        return state.hatch_taps >= kDesktopPetRequiredHatchTaps ? 18 : 12;
    }
    return 0;
}

DesktopPetAssetId egg_nest_asset(const DesktopPetState &state,
                                 DesktopPetHatchFrame frame)
{
    if (frame == DesktopPetHatchFrame::Opened) {
        return DesktopPetAssetId::EggNestHatched;
    }
    uint8_t visible_taps = state.hatch_taps;
    if ((frame == DesktopPetHatchFrame::WobbleLeft ||
         frame == DesktopPetHatchFrame::WobbleRight) &&
        visible_taps > 0U) {
        --visible_taps;
    }
    if (visible_taps >= 2U) {
        return DesktopPetAssetId::EggNestCracked;
    }
    if (visible_taps >= 1U) {
        return DesktopPetAssetId::EggNestChip;
    }
    return DesktopPetAssetId::EggNestIntact;
}

void draw_outing_bag(Canvas &canvas, int x, int y)
{
    canvas.draw_rect(x, y + 13, 58, 66, GrayLevel::Black);
    canvas.draw_rect(x + 3, y + 16, 52, 60, GrayLevel::Black);
    canvas.draw_line(x + 4, y + 36, x + 54, y + 36,
                     GrayLevel::Black);
    canvas.draw_line(x + 7, y + 36, x + 17, y + 47,
                     GrayLevel::Black);
    canvas.draw_line(x + 51, y + 36, x + 41, y + 47,
                     GrayLevel::Black);
    canvas.draw_rect(x + 24, y + 38, 10, 9, GrayLevel::Black);
    canvas.draw_line(x + 13, y + 13, x + 18, y + 3,
                     GrayLevel::Black);
    canvas.draw_line(x + 18, y + 3, x + 40, y + 3,
                     GrayLevel::Black);
    canvas.draw_line(x + 40, y + 3, x + 45, y + 13,
                     GrayLevel::Black);
    for (int stitch_y = y + 52; stitch_y < y + 72; stitch_y += 7) {
        canvas.draw_line(x + 11, stitch_y, x + 14, stitch_y,
                         GrayLevel::Black);
        canvas.draw_line(x + 44, stitch_y, x + 47, stitch_y,
                         GrayLevel::Black);
    }
}

void draw_outing_footprints(Canvas &canvas, bool returning)
{
    // Draws a fading paw trail between the rabbit and the screen edge.
    // 绘制从兔子位置延伸到屏幕边缘、逐渐变小的脚印轨迹。
    for (int index = 0; index < 5; ++index) {
        const int step = returning ? 4 - index : index;
        const int x = 226 + step * 49;
        const int y = 548 - (index % 2) * 12;
        const int pad_radius = step < 3 ? 4 : 3;
        canvas.fill_circle(x, y, pad_radius, GrayLevel::Black);
        canvas.fill_circle(x - 5, y - 7, 2, GrayLevel::Black);
        canvas.fill_circle(x + 4, y - 8, 2, GrayLevel::Black);
    }
}

void draw_outing_souvenir(Canvas &canvas,
                          PetPersonalityBranch branch,
                          int x,
                          int y)
{
    if (branch == PetPersonalityBranch::Foodie) {
        canvas.draw_line(x + 9, y + 4, x + 9, y + 30,
                         GrayLevel::Black);
        canvas.draw_line(x + 9, y + 8, x + 20, y + 1,
                         GrayLevel::Black);
        canvas.fill_circle(x + 2, y + 27, 7, GrayLevel::Black);
        canvas.fill_circle(x + 15, y + 27, 7, GrayLevel::Black);
        canvas.fill_circle(x + 8, y + 39, 7, GrayLevel::Black);
        draw_sparkle(canvas, x + 9, y + 52, 5);
        return;
    }
    if (branch == PetPersonalityBranch::Affectionate) {
        canvas.draw_line(x + 10, y + 18, x + 10, y + 49,
                         GrayLevel::Black);
        canvas.draw_line(x + 10, y + 35, x + 21, y + 28,
                         GrayLevel::Black);
        canvas.draw_circle(x + 10, y + 12, 5, GrayLevel::Black);
        canvas.draw_circle(x + 10, y + 1, 6, GrayLevel::Black);
        canvas.draw_circle(x + 20, y + 8, 6, GrayLevel::Black);
        canvas.draw_circle(x + 17, y + 19, 6, GrayLevel::Black);
        canvas.draw_circle(x + 3, y + 19, 6, GrayLevel::Black);
        canvas.draw_circle(x, y + 8, 6, GrayLevel::Black);
        draw_sparkle(canvas, x + 10, y + 57, 5);
        return;
    }

    canvas.draw_line(x, y + 34, x + 8, y, GrayLevel::Black);
    canvas.draw_line(x + 8, y, x + 17, y + 34, GrayLevel::Black);
    canvas.draw_line(x + 3, y + 16, x + 13, y + 16,
                     GrayLevel::Black);
    canvas.draw_line(x + 8, y + 3, x + 8, y + 30,
                     GrayLevel::Black);
    canvas.draw_line(x + 8, y + 11, x - 2, y + 5,
                     GrayLevel::Black);
    canvas.draw_line(x + 8, y + 20, x + 20, y + 12,
                     GrayLevel::Black);
    draw_sparkle(canvas, x + 9, y + 45, 5);
}

// Draws a centered pixel asset with its horizontal direction reversed.
// 将像素素材以中心点定位，并水平翻转其朝向。
void draw_centered_mirrored_asset(Canvas &canvas,
                                  int center_x,
                                  int center_y,
                                  const PixelAsset &asset,
                                  GrayLevel color)
{
    if (asset.data == nullptr || asset.width == 0U || asset.height == 0U) {
        return;
    }

    const size_t row_stride =
        (static_cast<size_t>(asset.width) + 7U) / 8U;
    const int left = center_x - static_cast<int>(asset.width) / 2;
    const int top = center_y - static_cast<int>(asset.height) / 2;
    for (uint16_t source_y = 0U; source_y < asset.height; ++source_y) {
        for (uint16_t source_x = 0U; source_x < asset.width; ++source_x) {
            const size_t index = static_cast<size_t>(source_y) * row_stride +
                                 source_x / 8U;
            const uint8_t mask = static_cast<uint8_t>(
                1U << (7U - source_x % 8U));
            if ((asset.data[index] & mask) == 0U) {
                continue;
            }
            const int mirrored_x =
                static_cast<int>(asset.width - 1U - source_x);
            canvas.fill_rect(left + mirrored_x, top + source_y,
                             1, 1, color);
        }
    }
}

void draw_outing_character(Canvas &canvas,
                           const DesktopPetState &state,
                           int center_x,
                           DesktopPetPose pose,
                           DesktopPetIdleFrame idle_frame,
                           bool bag,
                           bool souvenir,
                           bool face_right = false)
{
    constexpr int center_y = 425;
    if (bag) {
        const int bag_x = face_right ? center_x - 105 : center_x + 47;
        draw_outing_bag(canvas, bag_x, center_y - 80);
    }
    const PixelAsset &mask = desktop_pet_asset(
        pose_mask_asset(state.pet, pose, idle_frame));
    const PixelAsset &body = desktop_pet_asset(
        pose_asset(state.pet, pose, idle_frame));
    if (face_right) {
        draw_centered_mirrored_asset(canvas, center_x, center_y,
                                     mask, GrayLevel::White);
        draw_centered_mirrored_asset(canvas, center_x, center_y,
                                     body, GrayLevel::Black);
    } else {
        pixel_asset_draw_centered(canvas, center_x, center_y,
                                  mask, 1, GrayLevel::White);
        pixel_asset_draw_centered(canvas, center_x, center_y, body);
    }
    if (souvenir) {
        draw_outing_souvenir(canvas, state.pet.branch,
                             center_x + 72, center_y + 20);
    }
}

// Draws the shared identity and care-status area used by the home and outing
// scenes.
// 绘制主页与外出场景共用的身份和照料状态区域。
void draw_pet_status_header(Canvas &canvas, const DesktopPetState &state)
{
    if (desktop_pet_state_has_name(state)) {
        canvas.draw_text(22, 16, state.name, 3);
        canvas.draw_text(24, 50, desktop_pet_state_stage_label(state), 2);
        const int pencil_x = 31 + text_width(state.name, 3);
        canvas.draw_line(pencil_x, 27, pencil_x + 12, 15,
                         GrayLevel::Black);
        canvas.draw_line(pencil_x + 3, 30, pencil_x + 15, 18,
                         GrayLevel::Black);
        canvas.draw_line(pencil_x, 27, pencil_x + 3, 30,
                         GrayLevel::Black);
    } else {
        canvas.draw_text(22, 24, desktop_pet_state_stage_label(state), 3);
        const int pencil_x = 31 +
            text_width(desktop_pet_state_stage_label(state), 3);
        canvas.draw_line(pencil_x, 35, pencil_x + 12, 23,
                         GrayLevel::Black);
        canvas.draw_line(pencil_x + 3, 38, pencil_x + 15, 26,
                         GrayLevel::Black);
        canvas.draw_line(pencil_x, 35, pencil_x + 3, 38,
                         GrayLevel::Black);
    }
#if STICKY_DESKTOP_PET_TEST_MODE
    canvas.draw_rect(411, 18, 53, 35, GrayLevel::Black);
    canvas.draw_rect(413, 20, 49, 31, GrayLevel::Black);
    canvas.draw_text(419, 27, "TEST", 2);
#endif

    char growth_label[24] = {};
    const uint16_t growth_limit = desktop_pet_state_growth_limit(state);
    std::snprintf(growth_label, sizeof(growth_label), "GROWTH %u / %u",
                  static_cast<unsigned>(state.pet.growth),
                  static_cast<unsigned>(growth_limit));
    canvas.draw_text(24, 72, growth_label, 2);
    draw_progress(canvas, state.pet.growth, growth_limit);

    pixel_asset_draw(canvas, 310, 67,
                     desktop_pet_asset(DesktopPetAssetId::LoveIcon));
    char love_label[16] = {};
    std::snprintf(love_label, sizeof(love_label), "LOVE %u",
                  static_cast<unsigned>(state.pet.bond));
    canvas.draw_text(380, 76, love_label, 2);

    char food_label[16] = {};
    std::snprintf(food_label, sizeof(food_label), "FOOD %u",
                  static_cast<unsigned>(state.pet.needs.food));
    canvas.draw_text(24, 118, food_label, 2);
    char energy_label[16] = {};
    std::snprintf(energy_label, sizeof(energy_label), "ENERGY %u",
                  static_cast<unsigned>(state.pet.needs.energy));
    canvas.draw_text(456 - text_width(energy_label, 2),
                     118, energy_label, 2);
}

void draw_pet_day_label(Canvas &canvas, const DesktopPetState &state)
{
    char day_label[16] = {};
    std::snprintf(day_label, sizeof(day_label), "DAY %u",
                  static_cast<unsigned>(state.pet.day));
    draw_centered(canvas, 775, day_label, 2);
}

void draw_home_action_bar(Canvas &canvas)
{
    canvas.fill_rect(14, 599, 452, 3, GrayLevel::Black);
    for (int y = 616; y < 748; y += 8) {
        canvas.draw_line(160, y, 160, y + 3, GrayLevel::Black);
        canvas.draw_line(320, y, 320, y + 3, GrayLevel::Black);
    }
    draw_action(canvas, 80, DesktopPetAssetId::FeedIcon, "FEED");
    draw_action(canvas, 240, DesktopPetAssetId::TalkIcon, "TALK");
    draw_action(canvas, 400, DesktopPetAssetId::PlayIcon, "PLAY");
}

void draw_outing_action_bar(Canvas &canvas)
{
    canvas.fill_rect(14, 599, 452, 3, GrayLevel::Black);
    draw_action(canvas, 240, DesktopPetAssetId::TalkIcon,
                "CALL HER HOME");
}

}  // namespace

void desktop_pet_page_render_egg(Canvas &canvas,
                                 const DesktopPetState &state,
                                 DesktopPetHatchFrame frame)
{
    canvas.set_rotation(CanvasRotation::Deg90CounterClockwise);
    canvas.clear(GrayLevel::White);

    const bool opened = frame == DesktopPetHatchFrame::Opened;
    draw_centered(canvas, 58,
                  opened ? "WELCOME, LITTLE ONE!"
                         : "A NEW FRIEND IS HERE",
                  3);
    draw_centered(canvas, 105,
                  opened ? "OUR DAYS TOGETHER BEGIN NOW"
                         : "SOMETHING WARM IS WAITING INSIDE",
                  2);
#if STICKY_DESKTOP_PET_TEST_MODE
    canvas.draw_rect(411, 18, 53, 35, GrayLevel::Black);
    canvas.draw_rect(413, 20, 49, 31, GrayLevel::Black);
    canvas.draw_text(419, 27, "TEST", 2);
#endif

    for (uint8_t index = 0U; index < kDesktopPetRequiredHatchTaps; ++index) {
        const int x = 218 + static_cast<int>(index) * 22;
        if (index < state.hatch_taps) {
            canvas.fill_circle(x, 153, 6, GrayLevel::Black);
        } else {
            canvas.draw_circle(x, 153, 6, GrayLevel::Black);
        }
    }

    const DesktopPetAssetId asset = egg_asset(state, frame);
    const int center_x = 240 + egg_frame_offset(state, frame);
    pixel_asset_draw_centered(canvas, 240, 430,
                              desktop_pet_asset(
                                  egg_nest_asset(state, frame)));
    pixel_asset_draw_centered(canvas, center_x, 350,
                              desktop_pet_asset(egg_mask_asset(asset)),
                              1, GrayLevel::White);
    pixel_asset_draw_centered(canvas, center_x, 350,
                              desktop_pet_asset(asset));

    draw_sparkle(canvas, 75, 295, 10);
    draw_sparkle(canvas, 403, 335, 8);
    draw_sparkle(canvas, 94, 452, 6);

    const char *status = "TAP THE EGG GENTLY";
    const char *hint = "THREE TAPS TO WELCOME IT";
    if (opened) {
        status = "HELLO, LITTLE ONE!";
        hint = "YOUR FIRST DAY STARTS NOW";
    } else if (state.hatch_taps >= 2U) {
        status = "TWO LITTLE EARS!";
        hint = "ONE MORE GENTLE TAP";
    } else if (state.hatch_taps >= 1U) {
        status = "A TINY CRACK APPEARED!";
        hint = "IT CAN HEAR YOU NOW";
    }

    canvas.draw_rect(40, 575, 400, 122, GrayLevel::Black);
    canvas.draw_rect(43, 578, 394, 116, GrayLevel::Black);
    draw_centered(canvas, 608, status, 3);
    draw_centered(canvas, 657, hint, 2);
    draw_centered(canvas, 738,
                  opened ? "HATCHLING  GROWTH 10  LOVE 18"
                         : "BE GENTLE. A FRIEND IS GROWING.",
                  2);
}

void desktop_pet_page_render_home(Canvas &canvas,
                                  const DesktopPetState &state,
                                  DesktopPetPose pose,
                                  DesktopPetIdleFrame idle_frame,
                                  const char *message)
{
    canvas.set_rotation(CanvasRotation::Deg90CounterClockwise);
    canvas.clear(GrayLevel::White);

    draw_pet_status_header(canvas, state);

    pixel_asset_draw(canvas, 20, 226,
                     desktop_pet_asset(DesktopPetAssetId::Room));
    DesktopPetPose visible_pose = pose;
    DesktopPetIdleFrame visible_idle_frame = idle_frame;
    if (desktop_pet_state_requires_sleep(state)) {
        // Energy depletion owns the visible pose, including the final frame of
        // an action that consumed the last energy point.
        // 能量耗尽时统一接管显示姿势，包括刚好消耗最后一点能量的动作末帧。
        visible_pose = DesktopPetPose::Idle;
        visible_idle_frame = DesktopPetIdleFrame::Tired;
    } else if (visible_pose == DesktopPetPose::Idle &&
               desktop_pet_state_is_low_energy(state)) {
        visible_idle_frame = DesktopPetIdleFrame::Tired;
    }
    int character_center_y = 411;
    if (visible_idle_frame == DesktopPetIdleFrame::Tired &&
        state.pet.stage == PetLifeStage::Hatchling) {
        character_center_y += 12;
    } else if (visible_idle_frame == DesktopPetIdleFrame::Tired &&
               state.pet.stage == PetLifeStage::Child) {
        character_center_y += 18;
    }
    pixel_asset_draw_centered(canvas, 240, character_center_y,
                              desktop_pet_asset(
                                  pose_mask_asset(state.pet,
                                                  visible_pose,
                                                  visible_idle_frame)),
                              1, GrayLevel::White);
    pixel_asset_draw_centered(canvas, 240, character_center_y,
                              desktop_pet_asset(
                                  pose_asset(state.pet,
                                             visible_pose,
                                             visible_idle_frame)));
    draw_speech_bubble(canvas, message);

    draw_home_action_bar(canvas);
    draw_pet_day_label(canvas, state);
}

void desktop_pet_page_render_sleep(Canvas &canvas,
                                   const DesktopPetState &state,
                                   bool secondary_frame)
{
    canvas.set_rotation(CanvasRotation::Deg90CounterClockwise);
    canvas.clear(GrayLevel::White);

    const char *name = desktop_pet_state_has_name(state)
                           ? state.name
                           : desktop_pet_state_stage_label(state);
    draw_centered(canvas, 30, name, 3);
    draw_centered(canvas, 72, "SLEEPING PEACEFULLY", 3);

    char energy_label[24] = {};
    std::snprintf(energy_label, sizeof(energy_label), "ENERGY %u / 100",
                  static_cast<unsigned>(state.pet.needs.energy));
    draw_centered(canvas, 119, energy_label, 2);
    canvas.draw_rect(40, 155, 400, 18, GrayLevel::Black);
    const int energy_width = static_cast<int>(
        static_cast<uint32_t>(state.pet.needs.energy) * 394U / 100U);
    if (energy_width > 0) {
        canvas.fill_rect(43, 158, energy_width, 12, GrayLevel::Black);
    }

    pixel_asset_draw(canvas, 20, 205,
                     desktop_pet_asset(
                         secondary_frame
                             ? DesktopPetAssetId::SleepSceneB
                             : DesktopPetAssetId::SleepSceneA));

    canvas.fill_rect(35, 594, 410, 76, GrayLevel::Black);
    draw_centered_in_rect(canvas, {35, 594, 410, 76},
                          "WAKE UP", 3, GrayLevel::White);
    draw_centered(canvas, 704, "ENERGY RESTORES WHILE SLEEPING", 2);
    draw_centered(canvas, 745, "TAP ANYWHERE TO WAKE", 2);
}

void desktop_pet_page_render_outing(
    Canvas &canvas,
    const DesktopPetState &state,
    const DesktopPetOutingSession &outing,
    bool secondary_frame)
{
    canvas.set_rotation(CanvasRotation::Deg90CounterClockwise);
    canvas.clear(GrayLevel::White);

    draw_pet_status_header(canvas, state);

    pixel_asset_draw(canvas, 20, 226,
                     desktop_pet_asset(DesktopPetAssetId::Room));

    switch (outing.phase) {
    case DesktopPetOutingPhase::Packing:
        draw_outing_character(canvas, state, 230,
                              DesktopPetPose::Idle,
                              DesktopPetIdleFrame::Normal,
                              true, false);
        break;
    case DesktopPetOutingPhase::Leaving:
        draw_outing_footprints(canvas, false);
        draw_outing_character(canvas, state, 392,
                              DesktopPetPose::Idle,
                              DesktopPetIdleFrame::Normal,
                              true, false, true);
        canvas.draw_line(282, 384, 264, 377, GrayLevel::Black);
        canvas.draw_line(286, 370, 273, 357, GrayLevel::Black);
        canvas.draw_line(292, 357, 286, 340, GrayLevel::Black);
        break;
    case DesktopPetOutingPhase::Away:
        draw_outing_footprints(canvas, false);
        if (secondary_frame) {
            canvas.draw_line(90, 290, 101, 282, GrayLevel::Black);
            canvas.draw_line(101, 282, 112, 290, GrayLevel::Black);
            canvas.draw_line(116, 302, 128, 294, GrayLevel::Black);
            canvas.draw_line(128, 294, 140, 302, GrayLevel::Black);
        } else {
            canvas.draw_line(102, 302, 113, 294, GrayLevel::Black);
            canvas.draw_line(113, 294, 124, 302, GrayLevel::Black);
        }
        break;
    case DesktopPetOutingPhase::Returning:
        draw_outing_footprints(canvas, true);
        draw_outing_character(canvas, state, 385,
                              DesktopPetPose::Idle,
                              DesktopPetIdleFrame::Normal,
                              true, true);
        break;
    case DesktopPetOutingPhase::Reunion:
        draw_outing_character(canvas, state, 235,
                              DesktopPetPose::Idle,
                              DesktopPetIdleFrame::Normal,
                              false, true);
        draw_sparkle(canvas, 75, 345, 8);
        draw_sparkle(canvas, 402, 324, 7);
        break;
    case DesktopPetOutingPhase::Home:
    default:
        break;
    }

    if (outing.phase == DesktopPetOutingPhase::Away) {
        draw_outing_action_bar(canvas);
    } else {
        draw_home_action_bar(canvas);
    }
    draw_pet_day_label(canvas, state);
}

void desktop_pet_page_render_test(Canvas &canvas,
                                  const DesktopPetState &state,
                                  bool reset_confirmation)
{
    canvas.set_rotation(CanvasRotation::Deg90CounterClockwise);
    canvas.clear(GrayLevel::White);
    canvas.draw_text(25, 25, "PET TEST PANEL", 4);
    draw_button(canvas, kCloseTestRect, "X", false);

    char scores[64] = {};
    std::snprintf(scores, sizeof(scores),
                  "%s  DAY %u  GROWTH %u  LOVE %u",
                  desktop_pet_state_stage_label(state),
                  static_cast<unsigned>(state.pet.day),
                  static_cast<unsigned>(state.pet.growth),
                  static_cast<unsigned>(state.pet.bond));
    draw_centered(canvas, 115, scores, 2);
    if (state.pet.stage == PetLifeStage::Egg) {
        std::snprintf(scores, sizeof(scores), "HATCH %u / %u",
                      static_cast<unsigned>(state.hatch_taps),
                      static_cast<unsigned>(kDesktopPetRequiredHatchTaps));
    } else {
        std::snprintf(scores, sizeof(scores), "PATH F%u  P%u  A%u",
                      static_cast<unsigned>(state.pet.foodie_score),
                      static_cast<unsigned>(state.pet.affectionate_score),
                      static_cast<unsigned>(state.pet.active_score));
    }
    draw_centered(canvas, 155, scores, 2);
    std::snprintf(scores, sizeof(scores), "FOOD %u  ENERGY %u",
                  static_cast<unsigned>(state.pet.needs.food),
                  static_cast<unsigned>(state.pet.needs.energy));
    draw_centered(canvas, 195, scores, 2);

    if (state.pet.stage == PetLifeStage::Egg) {
        draw_centered(canvas, 285, "CLOSE THIS PANEL", 3);
        draw_centered(canvas, 350, "THEN TAP THE EGG GENTLY", 3);
        draw_centered(canvas, 425, "EACH TAP IS SAVED", 2);
    } else {
        draw_button(canvas, kNextDayRect, "NEXT DAY", false);
        draw_button(canvas, kAddGrowthRect, "+30 GROWTH", false);
        draw_button(canvas, kAddLoveRect, "+20 LOVE", false);
        draw_button(canvas, kReduceEnergyRect, "-30 ENERGY", false);
        draw_button(canvas, kSleepNowRect, "SLEEP NOW", false);
        draw_button(canvas, kGoOutRect, "GO OUT", false);
    }
    draw_button(canvas, kResetRect,
                reset_confirmation ? "CONFIRM RESET" : "RESET PET",
                reset_confirmation);
    draw_centered(canvas, 681,
                  reset_confirmation
                      ? "TAP RESET AGAIN TO CONFIRM"
                      : "TEST CHANGES SAVE TO THE PET RECORD",
                  2);
}

void desktop_pet_page_render_evolution(
    Canvas &canvas,
    const DesktopPetState &state,
    DesktopPetEvolutionFrame frame)
{
    canvas.set_rotation(CanvasRotation::Deg90CounterClockwise);
    canvas.clear(GrayLevel::White);

    const bool youth_evolution = state.pet.stage == PetLifeStage::Youth;
    const bool adult_evolution = state.pet.stage == PetLifeStage::Adult;
    const DesktopPetAssetId target_asset =
        adult_evolution
            ? adult_idle_asset(state.pet.branch,
                               DesktopPetIdleFrame::Normal)
        : youth_evolution
            ? youth_idle_asset(state.pet.branch,
                               DesktopPetIdleFrame::Normal)
            : DesktopPetAssetId::ChildIdle;
    const DesktopPetAssetId target_mask =
        adult_evolution
            ? adult_idle_mask_asset(state.pet.branch,
                                    DesktopPetIdleFrame::Normal)
        : youth_evolution
            ? youth_idle_mask_asset(state.pet.branch,
                                    DesktopPetIdleFrame::Normal)
            : DesktopPetAssetId::ChildIdleMask;
    const DesktopPetAssetId asset =
        frame == DesktopPetEvolutionFrame::Starting
            ? (adult_evolution
                   ? youth_idle_asset(state.pet.branch,
                                      DesktopPetIdleFrame::Normal)
               : youth_evolution ? DesktopPetAssetId::ChildIdle
                                 : DesktopPetAssetId::Idle)
            : target_asset;
    const DesktopPetAssetId mask =
        frame == DesktopPetEvolutionFrame::Starting
            ? (adult_evolution
                   ? youth_idle_mask_asset(state.pet.branch,
                                           DesktopPetIdleFrame::Normal)
               : youth_evolution ? DesktopPetAssetId::ChildIdleMask
                                 : DesktopPetAssetId::IdleMask)
            : target_mask;

    const char *title = adult_evolution
                            ? "OUR JOURNEY SHAPED ME..."
                        : youth_evolution
                            ? "MY PERSONALITY IS BLOOMING..."
                            : "SOMETHING IS HAPPENING...";
    if (frame == DesktopPetEvolutionFrame::Silhouette) {
        title = adult_evolution
                    ? "GROWING INTO MYSELF..."
                    : (youth_evolution ? "FINDING MY PATH..." : "GROWING...");
    } else if (frame == DesktopPetEvolutionFrame::Revealed) {
        title = adult_evolution
                    ? "I'M ALL GROWN UP!"
                    : (youth_evolution ? "I FOUND MY PATH!" : "LOOK! I GREW!");
    }
    draw_centered(canvas, 82, title,
                  frame == DesktopPetEvolutionFrame::Starting ? 2 : 3);

    draw_sparkle(canvas, 72, 215, 14);
    draw_sparkle(canvas, 401, 248, 10);
    draw_sparkle(canvas, 91, 552, 9);
    draw_sparkle(canvas, 389, 581, 15);
    draw_sparkle(canvas, 405, 151, 6);

    if (frame == DesktopPetEvolutionFrame::Silhouette) {
        pixel_asset_draw_centered(canvas, 240, 398,
                                  desktop_pet_asset(mask),
                                  2, GrayLevel::Black);
        canvas.draw_line(25, 398, 92, 398, GrayLevel::Black);
        canvas.draw_line(388, 398, 455, 398, GrayLevel::Black);
        canvas.draw_line(73, 288, 121, 318, GrayLevel::Black);
        canvas.draw_line(359, 318, 407, 288, GrayLevel::Black);
        canvas.draw_line(70, 518, 123, 481, GrayLevel::Black);
        canvas.draw_line(357, 481, 410, 518, GrayLevel::Black);
    } else {
        pixel_asset_draw_centered(canvas, 240, 398,
                                  desktop_pet_asset(mask),
                                  2, GrayLevel::White);
        pixel_asset_draw_centered(canvas, 240, 398,
                                  desktop_pet_asset(asset), 2);
    }

    if (frame == DesktopPetEvolutionFrame::Starting) {
        draw_centered(canvas, 650,
                      adult_evolution
                          ? "EVERY MOMENT HELPED ME GROW."
                      : youth_evolution
                          ? "ALL OUR DAYS SHAPED WHO I AM."
                          : "A WARM LIGHT SURROUNDS ME.",
                      2);
    } else if (frame == DesktopPetEvolutionFrame::Silhouette) {
        draw_centered(canvas, 650,
                      adult_evolution
                          ? "ONE LAST BRIGHT MOMENT..."
                      : youth_evolution
                          ? "ONE MORE MOMENT..."
                          : "MY EARS ARE GETTING LONGER!",
                      2);
    } else {
        if (adult_evolution) {
            const char *path_line = "OUR ADULT JOURNEY BEGINS!";
            if (state.pet.branch == PetPersonalityBranch::Foodie) {
                path_line = "THE FOODIE IN ME GREW STRONG!";
            } else if (state.pet.branch ==
                       PetPersonalityBranch::Affectionate) {
                path_line = "MY HEART GREW WITH OUR BOND!";
            } else if (state.pet.branch == PetPersonalityBranch::Active) {
                path_line = "I'M READY FOR BIG ADVENTURES!";
            }
            draw_centered(canvas, 650, path_line, 2);
        } else if (youth_evolution) {
            const char *path_line = "MY YOUTH PATH BEGINS!";
            if (state.pet.branch == PetPersonalityBranch::Foodie) {
                path_line = "MY FOODIE PATH BEGINS!";
            } else if (state.pet.branch ==
                       PetPersonalityBranch::Affectionate) {
                path_line = "MY HEART PATH BEGINS!";
            } else if (state.pet.branch == PetPersonalityBranch::Active) {
                path_line = "MY ACTIVE PATH BEGINS!";
            }
            draw_centered(canvas, 650, path_line, 2);
        } else {
            char stage_line[32] = {};
            std::snprintf(stage_line, sizeof(stage_line),
                          "WELCOME TO THE %s STAGE!",
                          desktop_pet_state_stage_label(state));
            draw_centered(canvas, 650, stage_line, 2);
        }
    }
}

void desktop_pet_page_render_personality_choice(
    Canvas &canvas,
    const DesktopPetState &state)
{
    (void)state;
    canvas.set_rotation(CanvasRotation::Deg90CounterClockwise);
    canvas.clear(GrayLevel::White);

    draw_centered(canvas, 55, "CHOOSE OUR NEXT", 4);
    draw_centered(canvas, 100, "ADVENTURE", 4);
    draw_centered(canvas, 165, "ONE LAST MOMENT TOGETHER", 2);

    draw_button(canvas, kFoodieChoiceRect, "SHARE A SNACK", false);
    draw_centered(canvas, kFoodieChoiceRect.y + 87, "FOODIE PATH", 2);
    draw_button(canvas, kAffectionateChoiceRect, "STAY CLOSE", false);
    draw_centered(canvas, kAffectionateChoiceRect.y + 87,
                  "AFFECTIONATE PATH", 2);
    draw_button(canvas, kActiveChoiceRect, "RACE OUTSIDE", false);
    draw_centered(canvas, kActiveChoiceRect.y + 87, "ACTIVE PATH", 2);

    draw_centered(canvas, 690, "THIS CHOICE SHAPES THE YOUTH STAGE", 2);
    draw_centered(canvas, 735, "TAP THE MOMENT THAT FEELS LIKE US", 2);
}

void desktop_pet_page_render_name_editor(
    Canvas &canvas,
    const char *text,
    DesktopPetKeyboardMode keyboard_mode,
    bool input_error,
    bool can_cancel)
{
    canvas.set_rotation(CanvasRotation::Deg90CounterClockwise);
    canvas.clear(GrayLevel::White);

    if (can_cancel) {
        draw_name_back_arrow(canvas);
    }
    draw_centered(canvas, 30, "NAME YOUR FRIEND", 3);
    draw_centered(canvas, 72,
                  can_cancel ? "CHOOSE A NEW NAME" : "A NAME MAKES IT YOURS",
                  2);
    canvas.draw_rect(kNameInputRect.x, kNameInputRect.y,
                     kNameInputRect.width, kNameInputRect.height,
                     GrayLevel::Black);
    canvas.draw_rect(kNameInputRect.x + 3, kNameInputRect.y + 3,
                     kNameInputRect.width - 6, kNameInputRect.height - 6,
                     GrayLevel::Black);

    const char *safe_text = text == nullptr ? "" : text;
    char preview[kDesktopPetNameMaximumLength + 12U] = {};
    std::snprintf(preview, sizeof(preview),
                  safe_text[0] == '\0' ? "YOUR PET_" : "%s_",
                  safe_text);
    draw_centered_in_rect(canvas, kNameInputRect, preview, 4);

    char count_text[16] = {};
    std::snprintf(count_text, sizeof(count_text), "%u / %u",
                  static_cast<unsigned>(std::strlen(safe_text)),
                  static_cast<unsigned>(kDesktopPetNameMaximumLength));
    canvas.draw_text(kNameInputRect.x + kNameInputRect.width -
                         text_width(count_text, 2) - 10,
                     232, count_text, 2);
    if (input_error) {
        canvas.draw_text(26, 232, "TYPE AT LEAST 1 CHARACTER", 1);
    }

    constexpr const char *kRow1Labels[] = {
        "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P",
    };
    constexpr const char *kRow2Labels[] = {
        "A", "S", "D", "F", "G", "H", "J", "K", "L",
    };
    constexpr const char *kRow3Labels[] = {
        "Z", "X", "C", "V", "B", "N", "M",
    };
    constexpr const char *kDigitLabels[] = {
        "1", "2", "3", "4", "5", "6", "7", "8", "9", "0",
    };

    if (keyboard_mode == DesktopPetKeyboardMode::Letters) {
        for (size_t index = 0U; index < 10U; ++index) {
            draw_name_key(canvas, kNameLetterRow1[index],
                          kRow1Labels[index]);
        }
        for (size_t index = 0U; index < 9U; ++index) {
            draw_name_key(canvas, kNameLetterRow2[index],
                          kRow2Labels[index]);
        }
        for (size_t index = 0U; index < 7U; ++index) {
            draw_name_key(canvas, kNameLetterRow3[index],
                          kRow3Labels[index]);
        }
    } else {
        for (size_t index = 0U; index < 5U; ++index) {
            draw_name_key(canvas, kNameDigitRow1[index],
                          kDigitLabels[index], 4);
            draw_name_key(canvas, kNameDigitRow2[index],
                          kDigitLabels[index + 5U], 4);
        }
    }

    draw_name_key(canvas, kNameToggleRect,
                  keyboard_mode == DesktopPetKeyboardMode::Letters
                      ? "123"
                      : "ABC");
    draw_name_key(canvas, kNameSpaceRect, "SPACE");
    draw_name_key(canvas, kNameDeleteRect, "DELETE", 2);
    draw_name_key(canvas, kNameClearRect, "CLEAR", 2);
    canvas.fill_rect(kNameApplyRect.x, kNameApplyRect.y,
                     kNameApplyRect.width, kNameApplyRect.height,
                     GrayLevel::Black);
    draw_centered_in_rect(canvas, kNameApplyRect, "SAVE NAME", 3,
                          GrayLevel::White);
    draw_centered(canvas, 712,
                  can_cancel ? "BACK KEEPS THE CURRENT NAME"
                             : "THIS FRIEND IS WAITING FOR A NAME",
                  2);
    draw_centered(canvas, 753, "LETTERS  NUMBERS  ONE SPACE", 1);
}

DesktopPetAction desktop_pet_page_action_at(bool test_open, int x, int y)
{
    if (!test_open) {
#if STICKY_DESKTOP_PET_TEST_MODE
        if (kTestBadgeRect.contains(x, y)) {
            return DesktopPetAction::OpenTest;
        }
#endif
        if (kNameRect.contains(x, y)) {
            return DesktopPetAction::OpenNameEditor;
        }
        if (kPetBodyRect.contains(x, y)) {
            return DesktopPetAction::Pet;
        }
        if (kFeedRect.contains(x, y)) {
            return DesktopPetAction::Feed;
        }
        if (kTalkRect.contains(x, y)) {
            return DesktopPetAction::Talk;
        }
        if (kPlayRect.contains(x, y)) {
            return DesktopPetAction::Play;
        }
        return DesktopPetAction::None;
    }

    if (kCloseTestRect.contains(x, y)) {
        return DesktopPetAction::CloseTest;
    }
    if (kNextDayRect.contains(x, y)) {
        return DesktopPetAction::NextDay;
    }
    if (kAddGrowthRect.contains(x, y)) {
        return DesktopPetAction::AddGrowth;
    }
    if (kAddLoveRect.contains(x, y)) {
        return DesktopPetAction::AddLove;
    }
    if (kReduceEnergyRect.contains(x, y)) {
        return DesktopPetAction::ReduceEnergy;
    }
    if (kSleepNowRect.contains(x, y)) {
        return DesktopPetAction::Sleep;
    }
    if (kGoOutRect.contains(x, y)) {
        return DesktopPetAction::StartOuting;
    }
    if (kResetRect.contains(x, y)) {
        return DesktopPetAction::Reset;
    }
    return DesktopPetAction::None;
}

DesktopPetAction desktop_pet_page_egg_action_at(int x, int y)
{
#if STICKY_DESKTOP_PET_TEST_MODE
    if (kTestBadgeRect.contains(x, y)) {
        return DesktopPetAction::OpenTest;
    }
#endif
    return kEggBodyRect.contains(x, y)
               ? DesktopPetAction::TapEgg
               : DesktopPetAction::None;
}

DesktopPetAction desktop_pet_page_personality_action_at(int x, int y)
{
    if (kFoodieChoiceRect.contains(x, y)) {
        return DesktopPetAction::ChooseFoodie;
    }
    if (kAffectionateChoiceRect.contains(x, y)) {
        return DesktopPetAction::ChooseAffectionate;
    }
    if (kActiveChoiceRect.contains(x, y)) {
        return DesktopPetAction::ChooseActive;
    }
    return DesktopPetAction::None;
}

DesktopPetAction desktop_pet_page_sleep_action_at(int x, int y)
{
    return kSleepWakeRect.contains(x, y)
               ? DesktopPetAction::Wake
               : DesktopPetAction::None;
}

DesktopPetAction desktop_pet_page_outing_action_at(
    DesktopPetOutingPhase phase,
    int x,
    int y)
{
    return phase == DesktopPetOutingPhase::Away &&
                   kOutingCallHomeRect.contains(x, y)
               ? DesktopPetAction::CallHome
               : DesktopPetAction::None;
}

DesktopPetNameAction desktop_pet_page_name_action_at(
    DesktopPetKeyboardMode keyboard_mode,
    bool can_cancel,
    int x,
    int y)
{
    if (x < 0 || y < 0 || x >= 480 || y >= 800) {
        return DesktopPetNameAction::None;
    }
    if (can_cancel && kNameBackRect.contains(x, y)) {
        return DesktopPetNameAction::Back;
    }

    if (keyboard_mode == DesktopPetKeyboardMode::Letters) {
        DesktopPetNameAction action = name_action_in_rects(
            kNameLetterRow1, kNameLetterRow1Actions, 10U, x, y);
        if (action != DesktopPetNameAction::None) {
            return action;
        }
        action = name_action_in_rects(
            kNameLetterRow2, kNameLetterRow2Actions, 9U, x, y);
        if (action != DesktopPetNameAction::None) {
            return action;
        }
        action = name_action_in_rects(
            kNameLetterRow3, kNameLetterRow3Actions, 7U, x, y);
        if (action != DesktopPetNameAction::None) {
            return action;
        }
    } else {
        DesktopPetNameAction action = name_action_in_rects(
            kNameDigitRow1, kNameDigitActions, 5U, x, y);
        if (action != DesktopPetNameAction::None) {
            return action;
        }
        action = name_action_in_rects(
            kNameDigitRow2, kNameDigitActions + 5, 5U, x, y);
        if (action != DesktopPetNameAction::None) {
            return action;
        }
    }

    if (kNameToggleRect.contains(x, y)) {
        return DesktopPetNameAction::ToggleKeyboard;
    }
    if (kNameSpaceRect.contains(x, y)) {
        return DesktopPetNameAction::Space;
    }
    if (kNameDeleteRect.contains(x, y)) {
        return DesktopPetNameAction::Delete;
    }
    if (kNameClearRect.contains(x, y)) {
        return DesktopPetNameAction::Clear;
    }
    if (kNameApplyRect.contains(x, y)) {
        return DesktopPetNameAction::Apply;
    }
    return DesktopPetNameAction::None;
}

bool desktop_pet_name_action_character(DesktopPetNameAction action,
                                       char &character)
{
    if (action >= DesktopPetNameAction::KeyA &&
        action <= DesktopPetNameAction::KeyZ) {
        character = static_cast<char>(
            'A' + static_cast<int>(action) -
            static_cast<int>(DesktopPetNameAction::KeyA));
        return true;
    }
    if (action >= DesktopPetNameAction::Digit0 &&
        action <= DesktopPetNameAction::Digit9) {
        character = static_cast<char>(
            '0' + static_cast<int>(action) -
            static_cast<int>(DesktopPetNameAction::Digit0));
        return true;
    }
    return false;
}

bool desktop_pet_name_action_can_batch(DesktopPetNameAction action)
{
    char character = '\0';
    if (desktop_pet_name_action_character(action, character)) {
        return true;
    }
    return action == DesktopPetNameAction::Space ||
           action == DesktopPetNameAction::Delete ||
           action == DesktopPetNameAction::Clear;
}

const char *desktop_pet_name_action_name(DesktopPetNameAction action)
{
    char character = '\0';
    if (desktop_pet_name_action_character(action, character)) {
        return character >= '0' && character <= '9'
                   ? "key_digit"
                   : "key_letter";
    }
    switch (action) {
    case DesktopPetNameAction::Back:
        return "back";
    case DesktopPetNameAction::ToggleKeyboard:
        return "toggle_keyboard";
    case DesktopPetNameAction::Space:
        return "space";
    case DesktopPetNameAction::Delete:
        return "delete";
    case DesktopPetNameAction::Clear:
        return "clear";
    case DesktopPetNameAction::Apply:
        return "apply";
    case DesktopPetNameAction::None:
    default:
        return "none";
    }
}
