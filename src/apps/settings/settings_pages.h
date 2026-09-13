#pragma once

#include <cstdint>

class Canvas;

enum class SettingsPage : uint8_t {
    Main,
    TimeEditor,
};

enum class SettingsAction : uint8_t {
    None,
    ToggleLanguage,
    EditTime,
    RefreshDisplay,
    PregnancySettings,
    Back,
    Delete,
    Save,
    Digit0,
    Digit1,
    Digit2,
    Digit3,
    Digit4,
    Digit5,
    Digit6,
    Digit7,
    Digit8,
    Digit9,
};

void settings_page_render_main(Canvas &canvas, bool rtc_ready);
void settings_page_render_time_editor(Canvas &canvas,
                                      const char *digits,
                                      bool input_error);
SettingsAction settings_page_action_at(SettingsPage page, int x, int y);
bool settings_action_digit(SettingsAction action, char &digit);
