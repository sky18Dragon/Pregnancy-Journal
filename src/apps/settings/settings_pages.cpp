#include "settings_pages.h"

#include <cstdio>
#include <cstring>

#include "canvas.h"
#include "prototype_ui.h"
#include "text_layout.h"
#include "ui_language.h"

namespace {

using prototype_ui::Rect;

constexpr Rect kLandscapeRows[] = {
    {24, 98, 752, 62},  {24, 160, 752, 62}, {24, 222, 752, 62},
    {24, 284, 752, 62}, {24, 346, 752, 62},
};
constexpr Rect kLandscapeKeys[] = {
    {476, 100, 94, 62}, {578, 100, 94, 62}, {680, 100, 96, 62},
    {476, 170, 94, 62}, {578, 170, 94, 62}, {680, 170, 96, 62},
    {476, 240, 94, 62}, {578, 240, 94, 62}, {680, 240, 96, 62},
    {476, 310, 94, 62}, {578, 310, 94, 62}, {680, 310, 96, 62},
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
bool portrait(int width, int height) { return height > width; }

Rect setting_row(size_t index, int width, int height) {
  if (portrait(width, height))
    return {24, 112 + static_cast<int>(index) * 106, 432, 106};
  return kLandscapeRows[index];
}

Rect key_rect(size_t index, int width, int height) {
  if (portrait(width, height))
    return {24 + static_cast<int>(index % 3U) * 148,
            326 + static_cast<int>(index / 3U) * 74, 136, 62};
  return kLandscapeKeys[index];
}

Rect pregnancy_button(size_t index, int width, int height) {
  if (portrait(width, height))
    return {24 + static_cast<int>(index) * 224, 720, 208, 56};
  return index == 0U ? Rect{24, 400, 368, 56} : Rect{408, 400, 368, 56};
}

void draw_setting(Canvas &canvas, size_t index, const char *title,
                  const char *description, const char *value) {
  const Rect row = setting_row(index, canvas.width(), canvas.height());
  if (index == 0U)
    canvas.draw_line(row.x, row.y, row.x + row.width, row.y, GrayLevel::Black);
  canvas.draw_line(row.x, row.y + row.height - 1, row.x + row.width,
                   row.y + row.height - 1, GrayLevel::Black);
  canvas.draw_text(row.x, row.y + 12, ui_text(title), 2, GrayLevel::Black);
  canvas.draw_text(row.x, row.y + 36, ui_text(description), 1,
                   GrayLevel::Black);
  if (value != nullptr) {
    const int width = ui_text_width(value, 2U);
    canvas.draw_text(row.x + row.width - 34 - width,
                     row.y +
                         (portrait(canvas.width(), canvas.height()) ? 68 : 22),
                     ui_text(value), 2, GrayLevel::Black);
  }
  prototype_ui::draw_chevron(canvas, row.x + row.width - 18,
                             row.y + (row.height - 16) / 2);
}

void draw_form_row(Canvas &canvas, int index, const char *label,
                   const char *sub, const char *value) {
  const bool is_portrait = portrait(canvas.width(), canvas.height());
  const int y = is_portrait ? 120 + index * 116 : 96 + index * 66;
  const int row_height = is_portrait ? 116 : 66;
  canvas.draw_line(24, y + row_height - 1, canvas.width() - 24,
                   y + row_height - 1, GrayLevel::Black);
  canvas.draw_text(24, y + 20, ui_text(label), 2, GrayLevel::Black);
  if (sub != nullptr)
    canvas.draw_text(24, y + 42, ui_text(sub), 1, GrayLevel::Black);
  const int width = ui_text_width(value, 2U);
  canvas.draw_text(canvas.width() - 24 - width, y + (is_portrait ? 70 : 24),
                   ui_text(value), 2, GrayLevel::Black);
}

} // namespace

void settings_page_render_main(Canvas &canvas, bool rtc_ready,
                               const char *clock_text, const char *due_text) {
  prototype_ui::begin_page(canvas);
  prototype_ui::draw_header(canvas, "SETTINGS",
                            "DEVICE SETTINGS AND PREGNANCY PROFILE", "HOME");
  draw_setting(canvas, 0, "LANGUAGE", "SAVED ON THIS DEVICE",
               ui_language_is_chinese() ? "简体中文" : "ENGLISH");
  draw_setting(canvas, 1, "DEVICE TIME", "USED BY ALL DAILY RECORDS",
               rtc_ready ? clock_text : "UNAVAILABLE");
  draw_setting(canvas, 2, "PREGNANCY SETTINGS", "DUE DATE / LAST PERIOD",
               due_text);
  draw_setting(canvas, 3, "DISPLAY CLEAN REFRESH", "CLEARS E-INK GHOSTING",
               "RUN");
  draw_setting(canvas, 4, "RETURN TO BABY WEEK", "DEFAULT HOME SCREEN", "");
  const bool is_portrait = portrait(canvas.width(), canvas.height());
  ui_draw_wrapped_text_pixels(
      canvas, 24, is_portrait ? 660 : 424, canvas.width() - 48,
      ui_language_is_chinese()
          ? "所有孕期记录仅保存在设备。本产品用于信息整理，不替代专业医疗建议。"
          : "ALL RECORDS STAY ON DEVICE. INFORMATION ONLY; NOT MEDICAL ADVICE.",
      is_portrait ? 14 : 12, GrayLevel::Black, is_portrait ? 3 : 2, 5,
      FontWeight::Regular);
}

void settings_page_render_time_editor(Canvas &canvas, const char *digits,
                                      bool input_error) {
  prototype_ui::begin_page(canvas);
  prototype_ui::draw_header(canvas, "SET DEVICE TIME",
                            "YYYY / MM / DD · HH : MM", "CANCEL");
  const bool is_portrait = portrait(canvas.width(), canvas.height());
  canvas.draw_line(24, is_portrait ? 108 : 100, canvas.width() - 24,
                   is_portrait ? 108 : 100, GrayLevel::Black);
  canvas.draw_text(24, is_portrait ? 130 : 124, ui_text("CURRENT INPUT"), 2,
                   GrayLevel::Black);
  char padded[13] = "____________";
  if (digits != nullptr) {
    const size_t count = std::strlen(digits) < 12U ? std::strlen(digits) : 12U;
    std::memcpy(padded, digits, count);
  }
  char formatted[24] = {};
  std::snprintf(formatted, sizeof(formatted), "%.4s-%.2s-%.2s  %.2s:%.2s",
                padded, padded + 4, padded + 6, padded + 8, padded + 10);
  const uint8_t formatted_scale = is_portrait ? 3U : 4U;
  canvas.draw_text(
      is_portrait
          ? (canvas.width() - ui_text_width(formatted, formatted_scale)) / 2
          : 24,
      is_portrait ? 176 : 170, formatted, formatted_scale, GrayLevel::Black);
  ui_draw_wrapped_text(
      canvas, 24, is_portrait ? 226 : 238,
      is_portrait ? canvas.width() - 48 : 424,
      input_error
          ? ui_text("INVALID DATE OR RTC WRITE FAILED")
          : ui_text(
                "DEVICE WILL NOT SLEEP WHILE EDITING. CONFIRM BEFORE SAVE."),
      2, GrayLevel::Black, 4, 8);
  for (size_t index = 0U; index < 12U; ++index) {
    prototype_ui::draw_button(
        canvas, key_rect(index, canvas.width(), canvas.height()),
        kKeyLabels[index], index == 11U, index == 9U || index == 11U ? 2 : 3);
  }
}

void settings_page_render_pregnancy(Canvas &canvas,
                                    const PregnancyProfile &profile,
                                    const PregnancyProgress &progress,
                                    bool configured) {
  prototype_ui::begin_page(canvas);
  prototype_ui::draw_header(canvas, "PREGNANCY SETTINGS",
                            "PREFER A CLINICIAN-CONFIRMED DUE DATE", "BACK");
  if (!configured) {
    canvas.draw_text(24, 150, ui_text("PREGNANCY PROFILE NOT SET"), 4,
                     GrayLevel::Black);
  } else {
    char due[24] = {}, week[32] = {}, percent[16] = {};
    std::snprintf(due, sizeof(due), "%04u.%02u.%02u",
                  static_cast<unsigned>(profile.estimated_due_date.year),
                  static_cast<unsigned>(profile.estimated_due_date.month),
                  static_cast<unsigned>(profile.estimated_due_date.day));
    if (ui_language_is_chinese())
      std::snprintf(week, sizeof(week), "%u 周 + %u 天",
                    static_cast<unsigned>(progress.weeks),
                    static_cast<unsigned>(progress.days));
    else
      std::snprintf(week, sizeof(week), "%u WEEKS + %u DAYS",
                    static_cast<unsigned>(progress.weeks),
                    static_cast<unsigned>(progress.days));
    std::snprintf(percent, sizeof(percent), "%u%%",
                  static_cast<unsigned>(progress.percent));
    draw_form_row(canvas, 0, "CALCULATION SOURCE", "CURRENT PROFILE",
                  profile.use_due_date_as_primary ? "DUE DATE" : "LAST PERIOD");
    draw_form_row(canvas, 1, "DUE DATE", nullptr, due);
    draw_form_row(canvas, 2, "CURRENT WEEK", nullptr, week);
    draw_form_row(canvas, 3, "40-WEEK PROGRESS", nullptr, percent);
  }
  prototype_ui::draw_button(
      canvas, pregnancy_button(0U, canvas.width(), canvas.height()),
      "EDIT DUE DATE", false, 2);
  prototype_ui::draw_button(
      canvas, pregnancy_button(1U, canvas.width(), canvas.height()), "DONE",
      true, 2);
}

SettingsAction settings_page_action_at(SettingsPage page, int width, int height,
                                       int x, int y) {
  if (prototype_ui::header_action_rect(width, height).contains(x, y))
    return SettingsAction::Back;
  if (page == SettingsPage::Main) {
    if (setting_row(0U, width, height).contains(x, y))
      return SettingsAction::ToggleLanguage;
    if (setting_row(1U, width, height).contains(x, y))
      return SettingsAction::EditTime;
    if (setting_row(2U, width, height).contains(x, y))
      return SettingsAction::PregnancySettings;
    if (setting_row(3U, width, height).contains(x, y))
      return SettingsAction::RefreshDisplay;
    if (setting_row(4U, width, height).contains(x, y))
      return SettingsAction::Home;
    return SettingsAction::None;
  }
  if (page == SettingsPage::PregnancySummary) {
    if (pregnancy_button(0U, width, height).contains(x, y))
      return SettingsAction::EditPregnancy;
    if (pregnancy_button(1U, width, height).contains(x, y))
      return SettingsAction::Home;
    return SettingsAction::None;
  }
  for (size_t index = 0U; index < 12U; ++index) {
    if (key_rect(index, width, height).contains(x, y))
      return kKeyActions[index];
  }
  return SettingsAction::None;
}

bool settings_action_digit(SettingsAction action, char &digit) {
  if (action < SettingsAction::Digit0 || action > SettingsAction::Digit9)
    return false;
  digit = static_cast<char>('0' + static_cast<int>(action) -
                            static_cast<int>(SettingsAction::Digit0));
  return true;
}
