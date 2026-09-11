#include "pregnancy_pages.h"

#include <cstdio>
#include <cstring>

#include "app_launcher_assets.h"
#include "canvas.h"
#include "pixel_asset.h"
#include "ui_language.h"

namespace {

constexpr int kScreenWidth = 800;
constexpr int kScreenHeight = 480;

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

constexpr Rect kKeyRects[] = {
    {500, 70, 84, 66}, {594, 70, 84, 66}, {688, 70, 84, 66},
    {500, 146, 84, 66}, {594, 146, 84, 66}, {688, 146, 84, 66},
    {500, 222, 84, 66}, {594, 222, 84, 66}, {688, 222, 84, 66},
    {500, 298, 84, 66}, {594, 298, 84, 66}, {688, 298, 84, 66},
};

constexpr PregnancyAction kKeyActions[] = {
    PregnancyAction::Digit1, PregnancyAction::Digit2,
    PregnancyAction::Digit3, PregnancyAction::Digit4,
    PregnancyAction::Digit5, PregnancyAction::Digit6,
    PregnancyAction::Digit7, PregnancyAction::Digit8,
    PregnancyAction::Digit9, PregnancyAction::Delete,
    PregnancyAction::Digit0, PregnancyAction::Continue,
};

constexpr const char *kKeyLabels[] = {
    "1", "2", "3", "4", "5", "6",
    "7", "8", "9", "DEL", "0", "NEXT",
};

constexpr Rect kBackRect = {30, 390, 180, 56};
constexpr Rect kEditRect = {660, 424, 108, 42};

int text_width(const char *text, int scale)
{
    return ui_text_width(text, static_cast<uint8_t>(scale));
}

void draw_centered_text_in_rect(Canvas &canvas,
                                const Rect &rect,
                                const char *text,
                                int scale,
                                GrayLevel color)
{
    const int x = rect.x + (rect.width - text_width(text, scale)) / 2;
    const int y = rect.y + (rect.height - 7 * scale) / 2;
    canvas.draw_text(x, y, text, scale, color);
}

void draw_beveled_rect(Canvas &canvas,
                       const Rect &rect,
                       GrayLevel color)
{
    constexpr int kCut = 8;
    canvas.draw_line(rect.x + kCut, rect.y,
                     rect.x + rect.width - kCut - 1, rect.y, color);
    canvas.draw_line(rect.x + rect.width - kCut - 1, rect.y,
                     rect.x + rect.width - 1, rect.y + kCut, color);
    canvas.draw_line(rect.x + rect.width - 1, rect.y + kCut,
                     rect.x + rect.width - 1,
                     rect.y + rect.height - kCut - 1, color);
    canvas.draw_line(rect.x + rect.width - 1,
                     rect.y + rect.height - kCut - 1,
                     rect.x + rect.width - kCut - 1,
                     rect.y + rect.height - 1, color);
    canvas.draw_line(rect.x + rect.width - kCut - 1,
                     rect.y + rect.height - 1,
                     rect.x + kCut, rect.y + rect.height - 1, color);
    canvas.draw_line(rect.x + kCut, rect.y + rect.height - 1,
                     rect.x, rect.y + rect.height - kCut - 1, color);
    canvas.draw_line(rect.x, rect.y + rect.height - kCut - 1,
                     rect.x, rect.y + kCut, color);
    canvas.draw_line(rect.x, rect.y + kCut,
                     rect.x + kCut, rect.y, color);
}

void draw_button(Canvas &canvas,
                 const Rect &rect,
                 const char *label,
                 bool filled,
                 int scale)
{
    const GrayLevel foreground = filled ? GrayLevel::White
                                        : GrayLevel::Black;
    if (filled) {
        canvas.fill_rect(rect.x, rect.y, rect.width, rect.height,
                         GrayLevel::Black);
    } else {
        draw_beveled_rect(canvas, rect, GrayLevel::Black);
    }
    draw_centered_text_in_rect(canvas, rect, label, scale, foreground);
}

void format_clock_digits(const char *digits, char *output, size_t size)
{
    char padded[13] = "____________";
    if (digits != nullptr) {
        const size_t count = std::strlen(digits) < 12U
                                 ? std::strlen(digits)
                                 : 12U;
        std::memcpy(padded, digits, count);
    }
    std::snprintf(output, size,
                  "%.4s-%.2s-%.2s  %.2s:%.2s",
                  padded, padded + 4, padded + 6,
                  padded + 8, padded + 10);
}

void format_date_digits(const char *digits, char *output, size_t size)
{
    char padded[9] = "________";
    if (digits != nullptr) {
        const size_t count = std::strlen(digits) < 8U
                                 ? std::strlen(digits)
                                 : 8U;
        std::memcpy(padded, digits, count);
    }
    std::snprintf(output, size, "%.4s-%.2s-%.2s",
                  padded, padded + 4, padded + 6);
}

void render_keypad(Canvas &canvas)
{
    for (size_t index = 0U; index < 12U; ++index) {
        const bool primary = index == 11U;
        const int scale = index == 9U || index == 11U ? 2 : 4;
        draw_button(canvas, kKeyRects[index], kKeyLabels[index],
                    primary, scale);
    }
}

void render_setup(Canvas &canvas,
                  const char *title,
                  const char *caption,
                  const char *value,
                  bool input_error,
                  bool can_cancel)
{
    canvas.clear(GrayLevel::White);
    canvas.draw_text(30, 28, title, 4, GrayLevel::Black);
    canvas.draw_text(30, 72, caption, 2, GrayLevel::Black);

    constexpr Rect kInputRect = {30, 118, 430, 86};
    draw_beveled_rect(canvas, kInputRect, GrayLevel::Black);
    canvas.draw_text(kInputRect.x + 20,
                     kInputRect.y + 27,
                     value,
                     4,
                     GrayLevel::Black);

    canvas.draw_text(30, 235,
                     input_error ? "CHECK THE DATE AND TRY AGAIN"
                                 : "TAP A NUMBER TO REPLACE THE VALUE",
                     2,
                     GrayLevel::Black);
    canvas.draw_text(30, 278,
                     "SAVED ONLY ON THIS DEVICE",
                     2,
                     GrayLevel::Black);

    if (can_cancel) {
        draw_button(canvas, kBackRect, "CANCEL", false, 3);
    }
    render_keypad(canvas);
}

void draw_tracker_bunny(Canvas &canvas)
{
    const AppLauncherStickerAsset &asset =
        app_launcher_sticker_asset(AppLauncherAssetId::Pregnancy);
    pixel_asset_draw(canvas, 622, 176, asset.gray,
                     1, GrayLevel::LightGray);
    pixel_asset_draw(canvas, 622, 176, asset.black,
                     1, GrayLevel::Black);
}

}  // namespace

void pregnancy_page_render_clock_setup(Canvas &canvas,
                                       const char *digits,
                                       bool input_error,
                                       bool can_cancel)
{
    char value[24] = {};
    format_clock_digits(digits, value, sizeof(value));
    render_setup(canvas,
                 "SET DEVICE TIME",
                 "YEAR / MONTH / DAY / HOUR / MINUTE",
                 value,
                 input_error,
                 can_cancel);
}

void pregnancy_page_render_due_date_setup(Canvas &canvas,
                                          const char *digits,
                                          bool input_error,
                                          bool can_cancel)
{
    char value[16] = {};
    format_date_digits(digits, value, sizeof(value));
    render_setup(canvas,
                 "SET DUE DATE",
                 "USE YOUR CONFIRMED ESTIMATED DUE DATE",
                 value,
                 input_error,
                 can_cancel);
}

void pregnancy_page_render_dashboard(Canvas &canvas,
                                     const PregnancyDate &due_date,
                                     const PregnancyProgress &progress)
{
    canvas.clear(GrayLevel::White);
    canvas.draw_text(32, 24,
                     "CURRENT DEVELOPMENT STAGE",
                     2,
                     GrayLevel::Black);

    char headline[48] = {};
    if (ui_language_is_chinese()) {
        std::snprintf(headline, sizeof(headline),
                      "第 %u 周", static_cast<unsigned>(progress.weeks));
    } else {
        std::snprintf(headline, sizeof(headline),
                      "WEEK %u", static_cast<unsigned>(progress.weeks));
    }
    canvas.draw_text(32, 72, headline, 9, GrayLevel::Black);

    char stage_badge[24] = {};
    std::snprintf(stage_badge, sizeof(stage_badge), "%s",
                  pregnancy_stage_name(progress.stage));
    constexpr Rect kStageRect = {448, 68, 320, 54};
    draw_button(canvas, kStageRect, stage_badge, false, 2);

    constexpr Rect kInfoRect = {32, 164, 736, 158};
    draw_beveled_rect(canvas, kInfoRect, GrayLevel::DarkGray);
    canvas.fill_rect(kInfoRect.x, kInfoRect.y + 8,
                     8, kInfoRect.height - 16, GrayLevel::Black);

    char current[96] = {};
    if (ui_language_is_chinese()) {
        std::snprintf(current, sizeof(current),
                      "当前孕周：%u 周 + %u 天",
                      static_cast<unsigned>(progress.weeks),
                      static_cast<unsigned>(progress.days));
    } else {
        std::snprintf(current, sizeof(current),
                      "CURRENT: %u WEEKS + %u DAYS",
                      static_cast<unsigned>(progress.weeks),
                      static_cast<unsigned>(progress.days));
    }
    canvas.draw_text(66, 205, current, 3, GrayLevel::Black);

    char stage[80] = {};
    if (ui_language_is_chinese()) {
        std::snprintf(stage, sizeof(stage), "阶段：%s",
                      ui_text(pregnancy_stage_name(progress.stage)));
    } else {
        std::snprintf(stage, sizeof(stage), "STAGE: %s",
                      pregnancy_stage_name(progress.stage));
    }
    canvas.draw_text(66, 258, stage, 3, GrayLevel::Black);
    draw_tracker_bunny(canvas);

    canvas.draw_text(32, 348,
                     "40-WEEK DEVELOPMENT PROGRESS",
                     3,
                     GrayLevel::Black);
    constexpr int kBarX = 32;
    constexpr int kBarY = 386;
    constexpr int kBarWidth = 736;
    constexpr int kBarHeight = 20;
    canvas.fill_rect(kBarX, kBarY, kBarWidth, kBarHeight,
                     GrayLevel::LightGray);
    const int filled_width =
        static_cast<int>(progress.percent) * kBarWidth / 100;
    canvas.fill_rect(kBarX, kBarY, filled_width, kBarHeight,
                     GrayLevel::Black);

    char completion[48] = {};
    if (ui_language_is_chinese()) {
        std::snprintf(completion, sizeof(completion),
                      "已完成约 %u%%",
                      static_cast<unsigned>(progress.percent));
    } else {
        std::snprintf(completion, sizeof(completion),
                      "ABOUT %u%% COMPLETE",
                      static_cast<unsigned>(progress.percent));
    }
    canvas.draw_text(32, 434, completion, 3, GrayLevel::Black);

    char due[40] = {};
    std::snprintf(due, sizeof(due),
                  ui_language_is_chinese()
                      ? "预产期 %04u-%02u-%02u"
                      : "DUE %04u-%02u-%02u",
                  static_cast<unsigned>(due_date.year),
                  static_cast<unsigned>(due_date.month),
                  static_cast<unsigned>(due_date.day));
    canvas.draw_text(388, 442, due, 2, GrayLevel::Black);
    draw_button(canvas, kEditRect, "EDIT", false, 2);
}

PregnancyAction pregnancy_page_action_at(PregnancyPage page,
                                         bool can_cancel,
                                         int x,
                                         int y)
{
    if (x < 0 || y < 0 || x >= kScreenWidth || y >= kScreenHeight) {
        return PregnancyAction::None;
    }
    if (page == PregnancyPage::Dashboard) {
        return kEditRect.contains(x, y)
                   ? PregnancyAction::Edit
                   : PregnancyAction::None;
    }
    if (can_cancel && kBackRect.contains(x, y)) {
        return PregnancyAction::Back;
    }
    for (size_t index = 0U; index < 12U; ++index) {
        if (kKeyRects[index].contains(x, y)) {
            return kKeyActions[index];
        }
    }
    return PregnancyAction::None;
}

bool pregnancy_action_digit(PregnancyAction action, char &digit)
{
    if (action < PregnancyAction::Digit0 ||
        action > PregnancyAction::Digit9) {
        return false;
    }
    digit = static_cast<char>('0' +
        static_cast<int>(action) - static_cast<int>(PregnancyAction::Digit0));
    return true;
}

const char *pregnancy_page_name(PregnancyPage page)
{
    switch (page) {
    case PregnancyPage::ClockSetup:
        return "clock_setup";
    case PregnancyPage::DueDateSetup:
        return "due_date_setup";
    case PregnancyPage::Dashboard:
        return "dashboard";
    }
    return "unknown";
}

const char *pregnancy_action_name(PregnancyAction action)
{
    switch (action) {
    case PregnancyAction::None: return "none";
    case PregnancyAction::Digit0: return "digit_0";
    case PregnancyAction::Digit1: return "digit_1";
    case PregnancyAction::Digit2: return "digit_2";
    case PregnancyAction::Digit3: return "digit_3";
    case PregnancyAction::Digit4: return "digit_4";
    case PregnancyAction::Digit5: return "digit_5";
    case PregnancyAction::Digit6: return "digit_6";
    case PregnancyAction::Digit7: return "digit_7";
    case PregnancyAction::Digit8: return "digit_8";
    case PregnancyAction::Digit9: return "digit_9";
    case PregnancyAction::Delete: return "delete";
    case PregnancyAction::Continue: return "continue";
    case PregnancyAction::Back: return "back";
    case PregnancyAction::Edit: return "edit";
    }
    return "unknown";
}
