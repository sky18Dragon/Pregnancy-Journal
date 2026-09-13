#include <cassert>

#include "app_pages.h"
#include "canvas.h"
#include "ui_language.h"

int main()
{
    uint8_t framebuffer[800U * 480U / 2U] = {};
    Canvas canvas(800U, 480U, framebuffer, sizeof(framebuffer));
    ui_language_set(UiLanguage::English);
    app_page_render_launcher(canvas, StickyAppId::Pregnancy);
    StickyAppId selected = StickyAppId::Settings;
    assert(app_page_launcher_app_at(800, 480, 100, 220, selected));
    assert(selected == StickyAppId::Pregnancy);
    assert(app_page_launcher_app_at(800, 480, 300, 220, selected));
    assert(selected == StickyAppId::Reminder);
    assert(app_page_launcher_app_at(800, 480, 480, 220, selected));
    assert(selected == StickyAppId::Checkup);
    assert(app_page_launcher_app_at(800, 480, 680, 220, selected));
    assert(selected == StickyAppId::Settings);
    assert(!app_page_launcher_app_at(800, 480, 50, 350, selected));
    assert(app_page_launcher_language_at(800, 480, 730, 50));
    return 0;
}
