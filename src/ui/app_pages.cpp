#include "app_pages.h"

#include <array>
#include <cstring>

#include "app_launcher_assets.h"
#include "canvas.h"
#include "chinese_font_assets.h"
#include "font.h"
#include "pixel_asset.h"
#include "ui_language.h"

#ifndef STICKY_ONBOARDING_TEST_MODE
#define STICKY_ONBOARDING_TEST_MODE 0
#endif

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

struct LauncherLabel {
    const char *first_line;
    const char *second_line;
};

struct TextInkBounds {
    int left;
    int top;
    int width;
    int height;
};

constexpr size_t kCardCount = 6U;
constexpr int kPortraitContentOffsetY = -26;
constexpr int kLandscapeContentOffsetY = 30;

int text_width(const char *text, int scale)
{
    return ui_text_width(text, static_cast<uint8_t>(scale));
}

TextInkBounds text_ink_bounds(const char *text, int scale)
{
    if (ui_language_is_chinese()) {
        return {0,
                0,
                ui_text_width(text, static_cast<uint8_t>(scale)),
                static_cast<int>(kChineseFontHeight) *
                    font_cjk_scale(static_cast<uint8_t>(scale))};
    }
    int first_column = -1;
    int last_column = -1;
    int first_row = -1;
    int last_row = -1;
    int cursor_column = 0;
    for (const char *character = text; *character != '\0'; ++character) {
        const FontGlyph &glyph = font_get_glyph(*character);
        for (int column = 0; column < kFontWidth; ++column) {
            for (int row = 0; row < kFontHeight; ++row) {
                if ((glyph.columns[column] & (1U << row)) == 0U) {
                    continue;
                }
                const int visible_column = cursor_column + column;
                if (first_column < 0 || visible_column < first_column) {
                    first_column = visible_column;
                }
                if (visible_column > last_column) {
                    last_column = visible_column;
                }
                if (first_row < 0 || row < first_row) {
                    first_row = row;
                }
                if (row > last_row) {
                    last_row = row;
                }
            }
        }
        cursor_column += kFontWidth + kFontSpacing;
    }
    if (first_column < 0 || first_row < 0) {
        return {0, 0, 0, 0};
    }
    return {
        first_column * scale,
        first_row * scale,
        (last_column - first_column + 1) * scale,
        (last_row - first_row + 1) * scale,
    };
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
        constexpr int kCardHeight = 198;
        constexpr int kColumnGap = 16;
        const int left = (width - kCardWidth * 2 - kColumnGap) / 2;
        constexpr int kTopRowY = 124;
        constexpr int kMiddleRowY = 352;
        constexpr int kBottomRowY = 580;
        return {{{StickyAppId::Home,
                  left, kTopRowY, kCardWidth, kCardHeight,
                  kTopRowY + 4, kTopRowY + 142},
                 {StickyAppId::DesktopPet,
                  left + kCardWidth + kColumnGap,
                  kTopRowY, kCardWidth, kCardHeight,
                  kTopRowY + 4, kTopRowY + 142},
                 {StickyAppId::Pomodoro,
                  left, kMiddleRowY, kCardWidth, kCardHeight,
                  kMiddleRowY + 4, kMiddleRowY + 142},
                 {StickyAppId::StatusBoard,
                  left + kCardWidth + kColumnGap,
                  kMiddleRowY, kCardWidth, kCardHeight,
                  kMiddleRowY + 4, kMiddleRowY + 142},
                 {StickyAppId::BookOfAnswers,
                  left, kBottomRowY, kCardWidth, kCardHeight,
                  kBottomRowY + 4, kBottomRowY + 142},
                 {StickyAppId::Pregnancy,
                  left + kCardWidth + kColumnGap,
                  kBottomRowY, kCardWidth, kCardHeight,
                  kBottomRowY + 4, kBottomRowY + 142}}};
    }

    constexpr int kCardWidth = 125;
    constexpr int kCardHeight = 300;
    constexpr int kGap = 6;
    const int left = (width - kCardWidth * 6 - kGap * 5) / 2;
    constexpr int kTop = 124 + kLandscapeContentOffsetY;
    return {{{StickyAppId::Home,
              left, kTop, kCardWidth, kCardHeight,
              kTop + 36, kTop + 212},
             {StickyAppId::DesktopPet,
              left + (kCardWidth + kGap), kTop,
              kCardWidth, kCardHeight,
              kTop + 36, kTop + 212},
             {StickyAppId::Pomodoro,
              left + (kCardWidth + kGap) * 2, kTop,
              kCardWidth, kCardHeight,
              kTop + 36, kTop + 212},
             {StickyAppId::StatusBoard,
              left + (kCardWidth + kGap) * 3, kTop,
              kCardWidth, kCardHeight,
              kTop + 36, kTop + 212},
             {StickyAppId::BookOfAnswers,
              left + (kCardWidth + kGap) * 4, kTop,
              kCardWidth, kCardHeight,
              kTop + 36, kTop + 212},
             {StickyAppId::Pregnancy,
              left + (kCardWidth + kGap) * 5, kTop,
              kCardWidth, kCardHeight,
              kTop + 36, kTop + 212}}};
}

LauncherLabel app_label(StickyAppId app)
{
    switch (app) {
    case StickyAppId::Home:
        return {"Home", nullptr};
    case StickyAppId::DesktopPet:
        return {"Pet", nullptr};
    case StickyAppId::Pomodoro:
        return {"Pomodoro", "Time"};
    case StickyAppId::StatusBoard:
        return {"Status", "Board"};
    case StickyAppId::BookOfAnswers:
        return {"Answers of", "book"};
    case StickyAppId::Pregnancy:
        return {"Baby", "Week"};
    }
    return {"App", nullptr};
}

AppLauncherAssetId launcher_asset_id(StickyAppId app)
{
    switch (app) {
    case StickyAppId::Home:
        return AppLauncherAssetId::Pet;
    case StickyAppId::DesktopPet:
        return AppLauncherAssetId::Pet;
    case StickyAppId::Pomodoro:
        return AppLauncherAssetId::Focus;
    case StickyAppId::StatusBoard:
        return AppLauncherAssetId::Status;
    case StickyAppId::BookOfAnswers:
        return AppLauncherAssetId::Answers;
    case StickyAppId::Pregnancy:
        return AppLauncherAssetId::Pregnancy;
    }
    return AppLauncherAssetId::Pet;
}

void draw_sticker(Canvas &canvas,
                  StickyAppId app,
                  int center_x,
                  int top,
                  bool active)
{
    if (app == StickyAppId::Home) {
        if (active) {
            canvas.draw_circle(center_x, top + 50, 49, GrayLevel::Black);
            canvas.draw_circle(center_x, top + 50, 46, GrayLevel::Black);
        }
        canvas.draw_line(center_x - 38, top + 49,
                         center_x, top + 17, GrayLevel::Black);
        canvas.draw_line(center_x, top + 17,
                         center_x + 38, top + 49, GrayLevel::Black);
        canvas.draw_rect(center_x - 30, top + 48, 60, 43,
                         GrayLevel::Black);
        canvas.fill_rect(center_x - 7, top + 66, 14, 25,
                         GrayLevel::Black);
        canvas.draw_rect(center_x + 12, top + 58, 10, 10,
                         GrayLevel::Black);
        return;
    }
    // Adds the outer ring only to the selected sticker, then paints its art.
    // 仅为选中贴纸绘制加粗外圈，再依次绘制浅灰网点与黑色图案。
    const AppLauncherStickerAsset &asset =
        app_launcher_sticker_asset(launcher_asset_id(app));
    const int x = center_x - static_cast<int>(asset.black.width) / 2;
    if (active) {
        pixel_asset_draw(canvas, x, top, asset.selection,
                         1, GrayLevel::Black);
    }
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

void draw_centered_label_text(Canvas &canvas,
                              int x,
                              int y,
                              int width,
                              int height,
                              const LauncherLabel &label,
                              GrayLevel color)
{
    constexpr int kScale = 2;
    constexpr int kLineGap = 5;
    const TextInkBounds first =
        text_ink_bounds(label.first_line, kScale);
    const bool two_lines = label.second_line != nullptr;
    const TextInkBounds second = two_lines
                                     ? text_ink_bounds(label.second_line,
                                                       kScale)
                                     : TextInkBounds{0, 0, 0, 0};
    const int content_height = two_lines
                                   ? first.height + kLineGap + second.height
                                   : first.height;
    const int content_top = y + (height - content_height) / 2;
    const int first_x = x + (width - first.width) / 2 - first.left;
    const int first_y = content_top - first.top;
    canvas.draw_text(first_x, first_y, label.first_line, kScale, color);
    if (!two_lines) {
        return;
    }
    const int second_x = x + (width - second.width) / 2 - second.left;
    const int second_y = content_top + first.height + kLineGap - second.top;
    canvas.draw_text(second_x, second_y,
                     label.second_line, kScale, color);
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
    const bool portrait = canvas.height() > canvas.width();
    const int label_width = portrait ? 190 : 120;
    constexpr int kLabelHeight = 54;
    const int label_x = center_x - label_width / 2;

    draw_sticker(canvas, card.app, center_x, card.asset_y, active);
    if (active) {
        draw_selection_marker(canvas, card.x + 14, card.asset_y + 8);
    }
    draw_beveled_label(canvas, label_x, card.label_y,
                       label_width, kLabelHeight, active);
    draw_centered_label_text(
        canvas,
        label_x,
        card.label_y,
        label_width,
        kLabelHeight,
        app_label(card.app),
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
    const int title_y = portrait ? 58 + kPortraitContentOffsetY
                                 : 30 + kLandscapeContentOffsetY;
    const int sparkle_y = portrait ? 77 + kPortraitContentOffsetY
                                    : 49 + kLandscapeContentOffsetY;
    const int divider_y = portrait ? 110 + kPortraitContentOffsetY
                                    : 82 + kLandscapeContentOffsetY;
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

    constexpr int kLanguageWidth = 74;
    constexpr int kLanguageHeight = 34;
    const int language_x = canvas.width() - kLanguageWidth - 12;
    const int language_y = portrait ? 32 : 60;
    canvas.draw_rect(language_x,
                     language_y,
                     kLanguageWidth,
                     kLanguageHeight,
                     GrayLevel::Black);
    draw_centered_text(canvas,
                       language_x + kLanguageWidth / 2,
                       language_y + 9,
                       ui_language_is_chinese() ? "EN" : "中文",
                       2);

    const auto cards = launcher_cards(canvas.width(), canvas.height());
    for (const LauncherCard &card : cards) {
        draw_card(canvas, card, current_app);
    }
#if STICKY_ONBOARDING_TEST_MODE
    const int guide_x = canvas.width() - 28;
    const int guide_y = portrait ? 112 : 74;
    canvas.draw_circle(guide_x, guide_y, 14);
    draw_centered_text(canvas, guide_x, guide_y - 8, "?", 2);
#endif
}

bool app_page_launcher_language_at(int width,
                                   int height,
                                   int x,
                                   int y)
{
    (void)height;
    constexpr int kWidth = 74;
    constexpr int kHeight = 34;
    const int left = width - kWidth - 12;
    const int top = height > width ? 32 : 60;
    return x >= left && x < left + kWidth &&
           y >= top && y < top + kHeight;
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

bool app_page_launcher_tutorial_at(int width, int height, int x, int y)
{
#if STICKY_ONBOARDING_TEST_MODE
    const bool portrait = height > width;
    const int center_y = portrait ? 112 : 74;
    return x >= width - 78 &&
           y >= center_y - 44 && y < center_y + 44;
#else
    (void)width;
    (void)height;
    (void)x;
    (void)y;
    return false;
#endif
}
