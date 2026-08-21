#include "app_pages.h"

#include <array>
#include <cstring>

#include "canvas.h"
#include "pet_animation_assets.h"
#include "pixel_asset.h"
#include "status_bunny_assets.h"

namespace {

struct LauncherCard {
    StickyAppId app;
    int x;
    int y;
    int width;
    int height;
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
    const bool portrait = height > width;
    if (portrait) {
        constexpr int kCardWidth = 196;
        constexpr int kCardHeight = 226;
        constexpr int kColumnGap = 20;
        constexpr int kRowGap = 24;
        const int left = (width - kCardWidth * 2 - kColumnGap) / 2;
        const int top = (height - kCardHeight * 2 - kRowGap) / 2 + 16;
        return {{{StickyAppId::DesktopPet,
                  left, top, kCardWidth, kCardHeight},
                 {StickyAppId::Pomodoro,
                  left + kCardWidth + kColumnGap,
                  top, kCardWidth, kCardHeight},
                 {StickyAppId::StatusBoard,
                  left, top + kCardHeight + kRowGap,
                  kCardWidth, kCardHeight},
                 {StickyAppId::BookOfAnswers,
                  left + kCardWidth + kColumnGap,
                  top + kCardHeight + kRowGap,
                  kCardWidth, kCardHeight}}};
    }

    constexpr int kCardWidth = 178;
    constexpr int kCardHeight = 244;
    constexpr int kGap = 16;
    const int left = (width - kCardWidth * 4 - kGap * 3) / 2;
    const int top = (height - kCardHeight) / 2 + 16;
    return {{{StickyAppId::DesktopPet,
              left, top, kCardWidth, kCardHeight},
             {StickyAppId::Pomodoro,
              left + (kCardWidth + kGap), top,
              kCardWidth, kCardHeight},
             {StickyAppId::StatusBoard,
              left + (kCardWidth + kGap) * 2, top,
              kCardWidth, kCardHeight},
             {StickyAppId::BookOfAnswers,
              left + (kCardWidth + kGap) * 3, top,
              kCardWidth, kCardHeight}}};
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

void draw_timer_icon(Canvas &canvas, int center_x, int center_y)
{
    canvas.draw_circle(center_x, center_y + 4, 42, GrayLevel::Black);
    canvas.draw_circle(center_x, center_y + 4, 37, GrayLevel::Black);
    canvas.fill_rect(center_x - 13, center_y - 48,
                     26, 7, GrayLevel::Black);
    canvas.draw_line(center_x, center_y + 4,
                     center_x, center_y - 23, GrayLevel::Black);
    canvas.draw_line(center_x, center_y + 4,
                     center_x + 22, center_y + 16, GrayLevel::Black);
    canvas.fill_circle(center_x, center_y + 4, 4, GrayLevel::Black);
    canvas.draw_line(center_x - 26, center_y - 28,
                     center_x - 36, center_y - 38, GrayLevel::Black);
    canvas.draw_line(center_x + 26, center_y - 28,
                     center_x + 36, center_y - 38, GrayLevel::Black);
}

void draw_crystal_icon(Canvas &canvas, int center_x, int center_y)
{
    canvas.draw_circle(center_x, center_y - 6, 43, GrayLevel::Black);
    canvas.draw_circle(center_x, center_y - 6, 37, GrayLevel::Black);
    canvas.fill_circle(center_x - 15, center_y - 18, 4,
                       GrayLevel::Black);
    canvas.draw_line(center_x + 13, center_y - 29,
                     center_x + 13, center_y - 13,
                     GrayLevel::Black);
    canvas.draw_line(center_x + 5, center_y - 21,
                     center_x + 21, center_y - 21,
                     GrayLevel::Black);
    canvas.draw_line(center_x - 30, center_y + 31,
                     center_x - 40, center_y + 47,
                     GrayLevel::Black);
    canvas.draw_line(center_x + 30, center_y + 31,
                     center_x + 40, center_y + 47,
                     GrayLevel::Black);
    canvas.fill_rect(center_x - 40, center_y + 44,
                     80, 7, GrayLevel::Black);
    canvas.fill_rect(center_x - 31, center_y + 51,
                     62, 5, GrayLevel::Black);
}

void draw_app_icon(Canvas &canvas,
                   StickyAppId app,
                   int center_x,
                   int center_y)
{
    switch (app) {
    case StickyAppId::DesktopPet:
        pixel_asset_draw_centered(
            canvas, center_x, center_y,
            pet_animation_asset(PetAnimationPose::Wave));
        break;
    case StickyAppId::Pomodoro:
        draw_timer_icon(canvas, center_x, center_y);
        break;
    case StickyAppId::StatusBoard:
        pixel_asset_draw_centered(
            canvas, center_x, center_y,
            status_bunny_asset(StatusBunnyAssetId::Welcome));
        break;
    case StickyAppId::BookOfAnswers:
        draw_crystal_icon(canvas, center_x, center_y);
        break;
    }
}

void draw_card(Canvas &canvas,
               const LauncherCard &card,
               StickyAppId current_app)
{
    const bool active = card.app == current_app;
    const int center_x = card.x + card.width / 2;
    constexpr int kLabelHeight = 44;
    canvas.draw_rect(card.x, card.y,
                     card.width, card.height, GrayLevel::Black);
    canvas.draw_rect(card.x + 4, card.y + 4,
                     card.width - 8, card.height - 8,
                     GrayLevel::Black);
    if (active) {
        canvas.fill_circle(card.x + card.width - 18,
                           card.y + 18, 6, GrayLevel::Black);
    }

    draw_app_icon(canvas,
                  card.app,
                  center_x,
                  card.y + (card.height - kLabelHeight) / 2);

    const int label_y = card.y + card.height - kLabelHeight;
    if (active) {
        canvas.fill_rect(card.x + 4, label_y,
                         card.width - 8, kLabelHeight - 4,
                         GrayLevel::Black);
    } else {
        canvas.draw_line(card.x + 4, label_y,
                         card.x + card.width - 5, label_y,
                         GrayLevel::Black);
    }
    draw_centered_text(canvas,
                       center_x,
                       label_y + 10,
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
    draw_centered_text(canvas,
                       canvas.width() / 2,
                       42,
                       "CHOOSE AN APP",
                       3);
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
