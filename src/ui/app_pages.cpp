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
    {StickyAppId::Pregnancy, 34, 170, 166, 132, "Baby Week"},
    {StickyAppId::Reminder, 222, 170, 166, 132, "Reminders"},
    {StickyAppId::Checkup, 410, 170, 166, 132, "Checkups"},
    {StickyAppId::Settings, 598, 170, 166, 132, "Settings"},
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

void draw_letter_icon(Canvas &canvas, int cx, int cy, const char *letter,
                      GrayLevel color)
{
    canvas.draw_circle(cx, cy, 31, color);
    canvas.draw_text(cx - 12, cy - 17, letter, 5, color);
}

}  // namespace

void app_page_render_launcher(Canvas &canvas, StickyAppId current_app)
{
    canvas.set_rotation(CanvasRotation::Deg0);
    canvas.clear(GrayLevel::White);
    canvas.draw_text(46, 32, ui_text("PREGNANCY JOURNAL"), 3,
                     GrayLevel::Black);
    canvas.draw_text(46, 78, ui_text("CHOOSE AN APP"), 4, GrayLevel::Black);
    const char *language = ui_language_is_chinese() ? "EN" : "中文";
    canvas.draw_rect(704, 34, 64, 42, GrayLevel::Black);
    canvas.draw_text(720, 47, language, 2, GrayLevel::Black);

    for (const Card &card : kCards) {
        const bool selected = card.app == current_app;
        const GrayLevel color = selected ? GrayLevel::White : GrayLevel::Black;
        bevel(canvas, card, selected);
        const int cx = card.x + card.width / 2;
        const int icon_y = card.y + 47;
        const char *letter = card.app == StickyAppId::Pregnancy ? "P" :
                             card.app == StickyAppId::Reminder ? "R" :
                             card.app == StickyAppId::Checkup ? "C" : "S";
        draw_letter_icon(canvas, cx, icon_y, letter, color);
        const char *label = ui_text(card.label);
        const int label_width = ui_text_width(card.label, 2U);
        canvas.draw_text(cx - label_width / 2, card.y + 104, label, 2, color);
    }
    canvas.draw_text(46, 438, ui_text("TAP A CARD  /  SWIPE DOWN TO CLOSE"), 2,
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
