#include <cassert>

#include "canvas.h"
#include "settings_pages.h"
#include "ui_language.h"

int main()
{
    uint8_t framebuffer[800U * 480U / 2U] = {};
    Canvas canvas(800U, 480U, framebuffer, sizeof(framebuffer));
    ui_language_set(UiLanguage::English);
    settings_page_render_main(canvas, true);
    assert(settings_page_action_at(SettingsPage::Main, 100, 170) ==
           SettingsAction::ToggleLanguage);
    assert(settings_page_action_at(SettingsPage::Main, 500, 170) ==
           SettingsAction::EditTime);
    assert(settings_page_action_at(SettingsPage::Main, 100, 310) ==
           SettingsAction::RefreshDisplay);
    settings_page_render_time_editor(canvas, "202609111530", false);
    assert(settings_page_action_at(SettingsPage::TimeEditor, 520, 90) ==
           SettingsAction::Digit1);
    assert(settings_page_action_at(SettingsPage::TimeEditor, 720, 320) ==
           SettingsAction::Save);
    char digit = 0;
    assert(settings_action_digit(SettingsAction::Digit7, digit));
    assert(digit == '7');
    return 0;
}
