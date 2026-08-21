#include "app_pages.h"

#include <array>
#include <cstring>

#include "app_launcher_assets.h"
#include "canvas.h"
#include "pixel_asset.h"

namespace {

struct LauncherCard {
    StickyAppId app;
    int x;
    int y;
    int width;
    int height;
    int asset_y;
    int label_y;
};

constexpr size_t kCardCount = 4U;

int text_width(const char *text, int scale)
{
    return static_cast<int>(std::strlen(text)) * 6 * scale;
}

void draw_centered_text(Canvas &canvas,
                        int center_x,
                        int y,
                        const char *text,
                        int scale,
                        GrayLevel color = GrayLevel::Black)
{
    canvas.draw_text(center_x - text_width(text, scale) / 2,
                     y,
                     text,
                     scale,
                     color);
}

std::array<LauncherCard, kCardCount> launcher_cards(int width, int height)
{
    // Keeps visible placement and invisible touch targets in one geometry table.
    // 使用同一份几何表管理画面位置与透明触摸区域。
    const bool portrait = height > width;
    if (portrait) {
        constexpr int kCardWidth = 212;
        constexpr int kCardHeight = 244;
        constexpr int kColumnGap = 16;
        const int left = (width - kCardWidth * 2 - kColumnGap) / 2;
        constexpr int kTopRowY = 154;
        constexpr int kBottomRowY = 414;
        return {{{StickyAppId::DesktopPet,
                  left, kTopRowY, kCardWidth, kCardHeight,
                  kTopRowY + 10, kTopRowY + 200},
                 {StickyAppId::Pomodoro,
                  left + kCardWidth + kColumnGap,
                  kTopRowY, kCardWidth, kCardHeight,
                  kTopRowY + 10, kTopRowY + 200},
                 {StickyAppId::StatusBoard,
                  left, kBottomRowY, kCardWidth, kCardHeight,
                  kBottomRowY + 10, kBottomRowY + 200},
                 {StickyAppId::BookOfAnswers,
                  left + kCardWidth + kColumnGap,
                  kBottomRowY, kCardWidth, kCardHeight,
                  kBottomRowY + 10, kBottomRowY + 200}}};
    }

    constexpr int kCardWidth = 184;
    constexpr int kCardHeight = 300;
    constexpr int kGap = 12;
    const int left = (width - kCardWidth * 4 - kGap * 3) / 2;
    constexpr int kTop = 124;
    return {{{StickyAppId::DesktopPet,
              left, kTop, kCardWidth, kCardHeight,
              kTop + 22, kTop + 212},
             {StickyAppId::Pomodoro,
              left + (kCardWidth + kGap), kTop,
              kCardWidth, kCardHeight,
              kTop + 22, kTop + 212},
             {StickyAppId::StatusBoard,
              left + (kCardWidth + kGap) * 2, kTop,
              kCardWidth, kCardHeight,
              kTop + 22, kTop + 212},
             {StickyAppId::BookOfAnswers,
              left + (kCardWidth + kGap) * 3, kTop,
              kCardWidth, kCardHeight,
              kTop + 22, kTop + 212}}};
}

const char *app_label(StickyAppId app)
{
    switch (app) {
    case StickyAppId::DesktopPet:
        return "PET";
    case StickyAppId::Pomodoro:
        return "FOCUS";
    case StickyAppId::StatusBoard:
        return "STATUS";
    case StickyAppId::BookOfAnswers:
        return "ANSWERS";
    }
    return "APP";
}

AppLauncherAssetId launcher_asset_id(StickyAppId app)
{
    switch (app) {
    case StickyAppId::DesktopPet:
        return AppLauncherAssetId::Pet;
    case StickyAppId::Pomodoro:
        return AppLauncherAssetId::Focus;
    case StickyAppId::StatusBoard:
        return AppLauncherAssetId::Status;
    case StickyAppId::BookOfAnswers:
        return AppLauncherAssetId::Answers;
    }
    return AppLauncherAssetId::Pet;
}

void draw_sticker(Canvas &canvas,
                  StickyAppId app,
                  int center_x,
                  int top)
{
    // Paints gray halftone first so the black silhouette remains crisp.
    // 先绘制浅灰网点，再覆盖黑色轮廓，保证电子纸上的边缘清晰。
    const AppLauncherStickerAsset &asset =
        app_launcher_sticker_asset(launcher_asset_id(app));
    const int x = center_x - static_cast<int>(asset.black.width) / 2;
    pixel_asset_draw(canvas, x, top, asset.gray, 1, GrayLevel::LightGray);
    pixel_asset_draw(canvas, x, top, asset.black, 1, GrayLevel::Black);
}

void draw_selection_marker(Canvas &canvas, int center_x, int center_y)
{
    canvas.fill_rect(center_x - 2, center_y - 8,
                     5, 17, GrayLevel::Black);
    canvas.fill_rect(center_x - 8, center_y - 2,
                     17, 5, GrayLevel::Black);
    canvas.fill_rect(center_x - 4, center_y - 4,
                     9, 9, GrayLevel::Black);
}

void draw_beveled_label(Canvas &canvas,
                        int x,
                        int y,
                        int width,
                        int height,
                        bool active)
{
    constexpr int kCut = 6;
    if (active) {
        canvas.fill_rect(x + kCut, y,
                         width - kCut * 2, height, GrayLevel::Black);
        canvas.fill_rect(x, y + kCut,
                         width, height - kCut * 2, GrayLevel::Black);
        return;
    }

    canvas.draw_line(x + kCut, y,
                     x + width - kCut - 1, y, GrayLevel::Black);
    canvas.draw_line(x + width - kCut - 1, y,
                     x + width - 1, y + kCut, GrayLevel::Black);
    canvas.draw_line(x + width - 1, y + kCut,
                     x + width - 1, y + height - kCut - 1,
                     GrayLevel::Black);
    canvas.draw_line(x + width - 1, y + height - kCut - 1,
                     x + width - kCut - 1, y + height - 1,
                     GrayLevel::Black);
    canvas.draw_line(x + width - kCut - 1, y + height - 1,
                     x + kCut, y + height - 1, GrayLevel::Black);
    canvas.draw_line(x + kCut, y + height - 1,
                     x, y + height - kCut - 1, GrayLevel::Black);
    canvas.draw_line(x, y + height - kCut - 1,
                     x, y + kCut, GrayLevel::Black);
    canvas.draw_line(x, y + kCut,
                     x + kCut, y, GrayLevel::Black);
}

void draw_title_divider(Canvas &canvas, int y)
{
    const int half_width = canvas.height() > canvas.width() ? 154 : 204;
    const int center_x = canvas.width() / 2;
    for (int x = center_x - half_width;
         x <= center_x + half_width;
         x += 12) {
        canvas.fill_rect(x, y, 3, 3, GrayLevel::Black);
    }
    canvas.fill_rect(center_x - 4, y - 3, 9, 9, GrayLevel::Black);
}

void draw_title_sparkle(Canvas &canvas, int center_x, int center_y)
{
    canvas.fill_rect(center_x, center_y - 6,
                     2, 14, GrayLevel::Black);
    canvas.fill_rect(center_x - 6, center_y,
                     14, 2, GrayLevel::Black);
    canvas.fill_rect(center_x - 2, center_y - 2,
                     6, 6, GrayLevel::Black);
}

void draw_card(Canvas &canvas,
               const LauncherCard &card,
               StickyAppId current_app)
{
    const bool active = card.app == current_app;
    const int center_x = card.x + card.width / 2;
    constexpr int kLabelWidth = 150;
    constexpr int kLabelHeight = 42;
    const int label_x = center_x - kLabelWidth / 2;

    draw_sticker(canvas, card.app, center_x, card.asset_y);
    if (active) {
        draw_selection_marker(canvas, card.x + 14, card.asset_y + 8);
    }
    draw_beveled_label(canvas, label_x, card.label_y,
                       kLabelWidth, kLabelHeight, active);
    draw_centered_text(canvas,
                       center_x,
                       card.label_y + 9,
                       app_label(card.app),
                       2,
                       active ? GrayLevel::White : GrayLevel::Black);
}

bool contains(const LauncherCard &card, int x, int y)
{
    return x >= card.x && y >= card.y &&
           x < card.x + card.width &&
           y < card.y + card.height;
}

}  // namespace

void app_page_render_launcher(Canvas &canvas, StickyAppId current_app)
{
    canvas.clear(GrayLevel::White);
    const bool portrait = canvas.height() > canvas.width();
    const int title_y = portrait ? 58 : 30;
    const int sparkle_y = portrait ? 77 : 49;
    const int divider_y = portrait ? 110 : 82;
    draw_centered_text(canvas,
                       canvas.width() / 2,
                       title_y,
                       "CHOOSE AN APP",
                       4);
    constexpr int title_sparkle_offset = 190;
    draw_title_sparkle(canvas,
                       canvas.width() / 2 - title_sparkle_offset,
                       sparkle_y);
    draw_title_sparkle(canvas,
                       canvas.width() / 2 + title_sparkle_offset,
                       sparkle_y);
    draw_title_divider(canvas, divider_y);
    const auto cards = launcher_cards(canvas.width(), canvas.height());
    for (const LauncherCard &card : cards) {
        draw_card(canvas, card, current_app);
    }
}

bool app_page_launcher_app_at(int width,
                              int height,
                              int x,
                              int y,
                              StickyAppId &selected_app)
{
    const auto cards = launcher_cards(width, height);
    for (const LauncherCard &card : cards) {
        if (contains(card, x, y)) {
            selected_app = card.app;
            return true;
        }
    }
    return false;
}
