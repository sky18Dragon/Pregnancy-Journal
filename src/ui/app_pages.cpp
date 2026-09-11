#include "app_pages.h"

#include "canvas.h"
#include "ui_language.h"

namespace {

struct Card {
    StickyAppId app;
    int x;
    int y;
    int width;
    int height;
    const char *label;
};

constexpr Card kCards[] = {
    {StickyAppId::Home, 45, 142, 220, 220, "Home"},
    {StickyAppId::Pregnancy, 290, 142, 220, 220, "Baby Week"},
    {StickyAppId::Settings, 535, 142, 220, 220, "Settings"},
};

void bevel(Canvas &canvas, const Card &card, bool selected)
{
    constexpr int cut = 12;
    if (selected) {
        canvas.fill_rect(card.x + cut, card.y, card.width - cut * 2,
                         card.height, GrayLevel::Black);
        canvas.fill_rect(card.x, card.y + cut, card.width,
                         card.height - cut * 2, GrayLevel::Black);
        return;
    }
    canvas.draw_line(card.x + cut, card.y,
                     card.x + card.width - cut, card.y, GrayLevel::Black);
    canvas.draw_line(card.x + card.width - cut, card.y,
                     card.x + card.width, card.y + cut, GrayLevel::Black);
    canvas.draw_line(card.x + card.width, card.y + cut,
                     card.x + card.width, card.y + card.height - cut,
                     GrayLevel::Black);
    canvas.draw_line(card.x + card.width, card.y + card.height - cut,
                     card.x + card.width - cut, card.y + card.height,
                     GrayLevel::Black);
    canvas.draw_line(card.x + card.width - cut, card.y + card.height,
                     card.x + cut, card.y + card.height, GrayLevel::Black);
    canvas.draw_line(card.x + cut, card.y + card.height,
                     card.x, card.y + card.height - cut, GrayLevel::Black);
    canvas.draw_line(card.x, card.y + card.height - cut,
                     card.x, card.y + cut, GrayLevel::Black);
    canvas.draw_line(card.x, card.y + cut, card.x + cut, card.y,
                     GrayLevel::Black);
}

void draw_home_icon(Canvas &canvas, int cx, int cy, GrayLevel color)
{
    canvas.draw_line(cx - 52, cy, cx, cy - 44, color);
    canvas.draw_line(cx, cy - 44, cx + 52, cy, color);
    canvas.draw_rect(cx - 40, cy, 80, 62, color);
    canvas.fill_rect(cx - 9, cy + 30, 18, 32, color);
}

void draw_settings_icon(Canvas &canvas, int cx, int cy, GrayLevel color)
{
    canvas.draw_circle(cx, cy + 5, 48, color);
    canvas.draw_circle(cx, cy + 5, 20, color);
    canvas.fill_rect(cx - 8, cy - 60, 16, 20, color);
    canvas.fill_rect(cx - 8, cy + 50, 16, 20, color);
    canvas.fill_rect(cx - 60, cy - 3, 20, 16, color);
    canvas.fill_rect(cx + 50, cy - 3, 20, 16, color);
}

void draw_pregnancy_icon(Canvas &canvas, int cx, int cy, GrayLevel color)
{
    canvas.draw_circle(cx, cy, 48, color);
    canvas.draw_circle(cx - 15, cy - 8, 4, color);
    canvas.draw_circle(cx + 15, cy - 8, 4, color);
    canvas.draw_line(cx - 18, cy + 17, cx, cy + 30, color);
    canvas.draw_line(cx, cy + 30, cx + 18, cy + 17, color);
    canvas.draw_line(cx - 18, cy + 17, cx - 10, cy + 8, color);
    canvas.draw_line(cx + 18, cy + 17, cx + 10, cy + 8, color);
}

}  // namespace

void app_page_render_launcher(Canvas &canvas, StickyAppId current_app)
{
    canvas.set_rotation(CanvasRotation::Deg0);
    canvas.clear(GrayLevel::White);
    canvas.draw_text(46, 32, "STICKY CORE", 3, GrayLevel::Black);
    canvas.draw_text(46, 78, "CHOOSE AN APP", 4, GrayLevel::Black);
    const char *language = ui_language_is_chinese() ? "EN" : "中文";
    canvas.draw_rect(704, 34, 64, 42, GrayLevel::Black);
    canvas.draw_text(720, 47, language, 2, GrayLevel::Black);

    for (const Card &card : kCards) {
        const bool selected = card.app == current_app;
        const GrayLevel color = selected ? GrayLevel::White : GrayLevel::Black;
        bevel(canvas, card, selected);
        const int cx = card.x + card.width / 2;
        if (card.app == StickyAppId::Home) {
            draw_home_icon(canvas, cx, 220, color);
        } else if (card.app == StickyAppId::Pregnancy) {
            draw_pregnancy_icon(canvas, cx, 220, color);
        } else {
            draw_settings_icon(canvas, cx, 218, color);
        }
        const char *label = ui_text(card.label);
        const int label_width = ui_text_width(label, 3U);
        canvas.draw_text(cx - label_width / 2, 315, label, 3, color);
    }
    canvas.draw_text(46, 410, "TAP A CARD  /  SWIPE DOWN TO CLOSE", 2,
                     GrayLevel::Black);
}

bool app_page_launcher_language_at(int width, int height, int x, int y)
{
    (void)width;
    (void)height;
    return x >= 690 && x < 780 && y >= 20 && y < 92;
}

bool app_page_launcher_app_at(int width, int height, int x, int y,
                              StickyAppId &selected_app)
{
    (void)width;
    (void)height;
    for (const Card &card : kCards) {
        if (x >= card.x && x < card.x + card.width &&
            y >= card.y && y < card.y + card.height) {
            selected_app = card.app;
            return true;
        }
    }
    return false;
}

bool app_page_launcher_tutorial_at(int width, int height, int x, int y)
{
    (void)width;
    (void)height;
    (void)x;
    (void)y;
    return false;
}
