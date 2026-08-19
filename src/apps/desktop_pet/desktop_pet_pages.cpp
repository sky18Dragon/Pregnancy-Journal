#include "desktop_pet_pages.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "canvas.h"
#include "desktop_pet_assets.h"
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

constexpr Rect kTestBadgeRect = {390, 12, 90, 72};
constexpr Rect kFeedRect = {0, 600, 160, 170};
constexpr Rect kPetRect = {160, 600, 160, 170};
constexpr Rect kPlayRect = {320, 600, 160, 170};

constexpr Rect kCloseTestRect = {360, 20, 100, 55};
constexpr Rect kNextDayRect = {35, 255, 410, 70};
constexpr Rect kAddGrowthRect = {35, 345, 410, 70};
constexpr Rect kAddLoveRect = {35, 435, 410, 70};
constexpr Rect kResetRect = {35, 555, 410, 80};

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

DesktopPetAssetId pose_asset(DesktopPetPose pose)
{
    switch (pose) {
    case DesktopPetPose::Feed:
        return DesktopPetAssetId::Feed;
    case DesktopPetPose::Pet:
        return DesktopPetAssetId::Pet;
    case DesktopPetPose::Play:
        return DesktopPetAssetId::Play;
    case DesktopPetPose::Idle:
    default:
        return DesktopPetAssetId::Idle;
    }
}

DesktopPetAssetId pose_mask_asset(DesktopPetPose pose)
{
    switch (pose) {
    case DesktopPetPose::Feed:
        return DesktopPetAssetId::FeedMask;
    case DesktopPetPose::Pet:
        return DesktopPetAssetId::PetMask;
    case DesktopPetPose::Play:
        return DesktopPetAssetId::PlayMask;
    case DesktopPetPose::Idle:
    default:
        return DesktopPetAssetId::IdleMask;
    }
}

void draw_progress(Canvas &canvas, uint16_t growth)
{
    constexpr int segment_count = 6;
    constexpr int segment_width = 39;
    constexpr int gap = 4;
    constexpr int x = 24;
    constexpr int y = 105;
    const int filled = std::min(
        segment_count,
        static_cast<int>((growth * segment_count +
                          kDesktopPetHatchlingGrowthLimit - 1U) /
                         kDesktopPetHatchlingGrowthLimit));
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

}  // namespace

void desktop_pet_page_render_home(Canvas &canvas,
                                  const DesktopPetState &state,
                                  DesktopPetPose pose,
                                  const char *message)
{
    canvas.set_rotation(CanvasRotation::Deg90CounterClockwise);
    canvas.clear(GrayLevel::White);

    canvas.draw_text(22, 24, "HATCHLING", 3);
#if STICKY_DESKTOP_PET_TEST_MODE
    canvas.draw_rect(411, 18, 53, 35, GrayLevel::Black);
    canvas.draw_rect(413, 20, 49, 31, GrayLevel::Black);
    canvas.draw_text(419, 27, "TEST", 2);
#endif

    char growth_label[24] = {};
    std::snprintf(growth_label, sizeof(growth_label), "GROWTH %u / %u",
                  static_cast<unsigned>(state.growth),
                  static_cast<unsigned>(kDesktopPetHatchlingGrowthLimit));
    canvas.draw_text(24, 72, growth_label, 2);
    draw_progress(canvas, state.growth);

    pixel_asset_draw(canvas, 310, 67,
                     desktop_pet_asset(DesktopPetAssetId::LoveIcon));
    char love_label[16] = {};
    std::snprintf(love_label, sizeof(love_label), "LOVE %u",
                  static_cast<unsigned>(state.love));
    canvas.draw_text(380, 76, love_label, 2);

    pixel_asset_draw(canvas, 20, 226,
                     desktop_pet_asset(DesktopPetAssetId::Room));
    pixel_asset_draw_centered(canvas, 240, 411,
                              desktop_pet_asset(pose_mask_asset(pose)),
                              1, GrayLevel::White);
    pixel_asset_draw_centered(canvas, 240, 411,
                              desktop_pet_asset(pose_asset(pose)));
    draw_speech_bubble(canvas, message);

    canvas.fill_rect(14, 599, 452, 3, GrayLevel::Black);
    for (int y = 616; y < 748; y += 8) {
        canvas.draw_line(160, y, 160, y + 3, GrayLevel::Black);
        canvas.draw_line(320, y, 320, y + 3, GrayLevel::Black);
    }
    draw_action(canvas, 80, DesktopPetAssetId::FeedIcon, "FEED");
    draw_action(canvas, 240, DesktopPetAssetId::PetIcon, "PET");
    draw_action(canvas, 400, DesktopPetAssetId::PlayIcon, "PLAY");

    char day_label[16] = {};
    std::snprintf(day_label, sizeof(day_label), "DAY %u",
                  static_cast<unsigned>(state.day));
    draw_centered(canvas, 775, day_label, 2);
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
                  "DAY %u  GROWTH %u  LOVE %u",
                  static_cast<unsigned>(state.day),
                  static_cast<unsigned>(state.growth),
                  static_cast<unsigned>(state.love));
    draw_centered(canvas, 115, scores, 2);
    std::snprintf(scores, sizeof(scores), "FOOD %u  PET %u  PLAY %u",
                  static_cast<unsigned>(state.foodie_score),
                  static_cast<unsigned>(state.affectionate_score),
                  static_cast<unsigned>(state.active_score));
    draw_centered(canvas, 155, scores, 2);

    draw_button(canvas, kNextDayRect, "NEXT DAY", false);
    draw_button(canvas, kAddGrowthRect, "+30 GROWTH", false);
    draw_button(canvas, kAddLoveRect, "+20 LOVE", false);
    draw_button(canvas, kResetRect,
                reset_confirmation ? "CONFIRM RESET" : "RESET PET",
                reset_confirmation);
    draw_centered(canvas, 690,
                  reset_confirmation
                      ? "TAP RESET AGAIN TO CONFIRM"
                      : "TEST CHANGES SAVE TO THE PET RECORD",
                  2);
}

DesktopPetAction desktop_pet_page_action_at(bool test_open, int x, int y)
{
    if (!test_open) {
#if STICKY_DESKTOP_PET_TEST_MODE
        if (kTestBadgeRect.contains(x, y)) {
            return DesktopPetAction::OpenTest;
        }
#endif
        if (kFeedRect.contains(x, y)) {
            return DesktopPetAction::Feed;
        }
        if (kPetRect.contains(x, y)) {
            return DesktopPetAction::Pet;
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
    if (kResetRect.contains(x, y)) {
        return DesktopPetAction::Reset;
    }
    return DesktopPetAction::None;
}
