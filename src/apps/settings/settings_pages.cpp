#include "settings_pages.h"

#include <cstdio>
#include <cstring>

#include "canvas.h"
#include "ui_language.h"

namespace {

struct Rect {
    int x;
    int y;
    int width;
    int height;
    bool contains(int px, int py) const
    {
        return px >= x && py >= y && px < x + width && py < y + height;
    }
};

constexpr Rect kLanguage = {36, 130, 344, 100};
constexpr Rect kTime = {420, 130, 344, 100};
constexpr Rect kRefresh = {36, 272, 344, 100};
constexpr Rect kBack = {420, 272, 344, 100};
constexpr Rect kEditorBack = {32, 398, 170, 52};
constexpr Rect kKeys[] = {
    {488, 62, 86, 66}, {582, 62, 86, 66}, {676, 62, 86, 66},
    {488, 136, 86, 66}, {582, 136, 86, 66}, {676, 136, 86, 66},
    {488, 210, 86, 66}, {582, 210, 86, 66}, {676, 210, 86, 66},
    {488, 284, 86, 66}, {582, 284, 86, 66}, {676, 284, 86, 66},
};
constexpr SettingsAction kKeyActions[] = {
    SettingsAction::Digit1, SettingsAction::Digit2, SettingsAction::Digit3,
    SettingsAction::Digit4, SettingsAction::Digit5, SettingsAction::Digit6,
    SettingsAction::Digit7, SettingsAction::Digit8, SettingsAction::Digit9,
    SettingsAction::Delete, SettingsAction::Digit0, SettingsAction::Save,
};
constexpr const char *kKeyLabels[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "DEL", "0", "SAVE",
};

void bevel(Canvas &canvas, const Rect &rect)
{
    constexpr int cut = 8;
    canvas.draw_line(rect.x + cut, rect.y, rect.x + rect.width - cut, rect.y,
                     GrayLevel::Black);
    canvas.draw_line(rect.x + rect.width - cut, rect.y,
                     rect.x + rect.width, rect.y + cut, GrayLevel::Black);
    canvas.draw_line(rect.x + rect.width, rect.y + cut,
                     rect.x + rect.width, rect.y + rect.height - cut,
                     GrayLevel::Black);
    canvas.draw_line(rect.x + rect.width, rect.y + rect.height - cut,
                     rect.x + rect.width - cut, rect.y + rect.height,
                     GrayLevel::Black);
    canvas.draw_line(rect.x + rect.width - cut, rect.y + rect.height,
                     rect.x + cut, rect.y + rect.height, GrayLevel::Black);
    canvas.draw_line(rect.x + cut, rect.y + rect.height,
                     rect.x, rect.y + rect.height - cut, GrayLevel::Black);
    canvas.draw_line(rect.x, rect.y + rect.height - cut,
                     rect.x, rect.y + cut, GrayLevel::Black);
    canvas.draw_line(rect.x, rect.y + cut, rect.x + cut, rect.y,
                     GrayLevel::Black);
}

void centered(Canvas &canvas, const Rect &rect, const char *text, int scale)
{
    const char *localized = ui_text(text);
    const int width = ui_text_width(text, static_cast<uint8_t>(scale));
    canvas.draw_text(rect.x + (rect.width - width) / 2,
                     rect.y + (rect.height - 7 * scale) / 2,
                     localized, scale, GrayLevel::Black);
}

void button(Canvas &canvas, const Rect &rect, const char *text, int scale)
{
    bevel(canvas, rect);
    centered(canvas, rect, text, scale);
}

}  // namespace

void settings_page_render_main(Canvas &canvas, bool rtc_ready)
{
    canvas.set_rotation(CanvasRotation::Deg0);
    canvas.clear(GrayLevel::White);
    canvas.draw_text(36, 28, ui_text("SETTINGS"), 5, GrayLevel::Black);
    canvas.draw_text(36, 82, ui_text("DEVICE AND FRAMEWORK CONTROLS"), 2,
                     GrayLevel::Black);
    button(canvas, kLanguage,
           ui_language_is_chinese() ? "语言：中文" : "LANGUAGE: ENGLISH", 3);
    button(canvas, kTime, "SET DEVICE TIME", 3);
    button(canvas, kRefresh, "DISPLAY CLEAN REFRESH", 3);
    button(canvas, kBack, "RETURN HOME", 3);
    canvas.draw_text(36, 430,
                     ui_text(rtc_ready ? "RTC READY" : "RTC UNAVAILABLE"),
                     2, GrayLevel::Black);
}

void settings_page_render_time_editor(Canvas &canvas,
                                      const char *digits,
                                      bool input_error)
{
    canvas.set_rotation(CanvasRotation::Deg0);
    canvas.clear(GrayLevel::White);
    canvas.draw_text(32, 24, "SET DEVICE TIME", 4, GrayLevel::Black);
    canvas.draw_text(32, 70, "YYYY / MM / DD / HH / MM", 2,
                     GrayLevel::Black);
    char padded[13] = "____________";
    if (digits != nullptr) {
        const size_t count = std::strlen(digits) < 12U
                                 ? std::strlen(digits) : 12U;
        std::memcpy(padded, digits, count);
    }
    char formatted[24] = {};
    std::snprintf(formatted, sizeof(formatted), "%.4s-%.2s-%.2s %.2s:%.2s",
                  padded, padded + 4, padded + 6, padded + 8, padded + 10);
    const Rect input = {32, 118, 414, 92};
    bevel(canvas, input);
    canvas.draw_text(52, 150, formatted, 4, GrayLevel::Black);
    canvas.draw_text(32, 244,
                     input_error ? "INVALID DATE OR RTC WRITE FAILED"
                                 : "ENTER 12 DIGITS, THEN SAVE",
                     2, GrayLevel::Black);
    button(canvas, kEditorBack, "BACK", 3);
    for (size_t index = 0U; index < 12U; ++index) {
        button(canvas, kKeys[index], kKeyLabels[index],
               index == 9U || index == 11U ? 2 : 4);
    }
}

SettingsAction settings_page_action_at(SettingsPage page, int x, int y)
{
    if (page == SettingsPage::Main) {
        if (kLanguage.contains(x, y)) return SettingsAction::ToggleLanguage;
        if (kTime.contains(x, y)) return SettingsAction::EditTime;
        if (kRefresh.contains(x, y)) return SettingsAction::RefreshDisplay;
        if (kBack.contains(x, y)) return SettingsAction::Back;
        return SettingsAction::None;
    }
    if (kEditorBack.contains(x, y)) return SettingsAction::Back;
    for (size_t index = 0U; index < 12U; ++index) {
        if (kKeys[index].contains(x, y)) return kKeyActions[index];
    }
    return SettingsAction::None;
}

bool settings_action_digit(SettingsAction action, char &digit)
{
    if (action < SettingsAction::Digit0 || action > SettingsAction::Digit9) {
        return false;
    }
    digit = static_cast<char>('0' + static_cast<int>(action) -
                             static_cast<int>(SettingsAction::Digit0));
    return true;
}
