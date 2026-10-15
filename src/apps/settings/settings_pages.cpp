#include "settings_pages.h"

#include <cstdio>
#include <cstring>

#include "canvas.h"
#include "prototype_ui.h"
#include "text_layout.h"
#include "ui_language.h"
#include "ui_tokens.h"

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

Rect form_row(size_t index, int width, int height) {
  const bool is_portrait = portrait(width, height);
  return {24, (is_portrait ? 120 : 96) +
                  static_cast<int>(index) * (is_portrait ? 116 : 66),
          width - 2 * ui_tokens::kContentMargin, is_portrait ? 116 : 66};
}

Rect key_rect(size_t index, int width, int height) {
  if (portrait(width, height))
    return {24 + static_cast<int>(index % 3U) * 148,
            326 + static_cast<int>(index / 3U) * 74, 136, 62};
  return kLandscapeKeys[index];
}

Rect pregnancy_button(size_t index, int width, int height) {
  if (portrait(width, height))
    return {24 + static_cast<int>(index) * 224, 720, 208, ui_tokens::kButtonHeight};
  return index == 0U ? Rect{24, 400, 368, ui_tokens::kButtonHeight} : Rect{408, 400, 368, ui_tokens::kButtonHeight};
}

Rect storage_split_button(size_t index, int width, int height) {
  if (portrait(width, height))
    return {24 + static_cast<int>(index) * 224, 720, 208, ui_tokens::kButtonHeight};
  return index == 0U ? Rect{24, 400, 368, ui_tokens::kButtonHeight} : Rect{408, 400, 368, ui_tokens::kButtonHeight};
}

Rect storage_confirm_button(size_t index, int width, int height) {
  if (portrait(width, height))
    return {24 + static_cast<int>(index) * 224, 720, 208, ui_tokens::kButtonHeight};
  return index == 0U ? Rect{24, 400, 368, ui_tokens::kButtonHeight} : Rect{408, 400, 368, ui_tokens::kButtonHeight};
}

const char *storage_card_value(SettingsStorageCardView card) {
  switch (card) {
  case SettingsStorageCardView::Missing:
    return "MISSING";
  case SettingsStorageCardView::ReadWrite:
  case SettingsStorageCardView::ReadOnly:
  case SettingsStorageCardView::Full:
    return "INSERTED";
  case SettingsStorageCardView::Unavailable:
    return "UNAVAILABLE";
  }
  return "UNAVAILABLE";
}

const char *storage_access_value(SettingsStorageCardView card) {
  switch (card) {
  case SettingsStorageCardView::ReadWrite:
    return "READ + WRITE";
  case SettingsStorageCardView::ReadOnly:
    return "READ ONLY";
  case SettingsStorageCardView::Full:
    return "FULL";
  case SettingsStorageCardView::Missing:
  case SettingsStorageCardView::Unavailable:
    return "UNAVAILABLE";
  }
  return "UNAVAILABLE";
}

const char *storage_dataset_value(SettingsStorageDatasetView dataset) {
  switch (dataset) {
  case SettingsStorageDatasetView::Unknown:
    return "UNAVAILABLE";
  case SettingsStorageDatasetView::NotInitialized:
    return "NOT INITIALIZED";
  case SettingsStorageDatasetView::Ready:
    return "READY";
  case SettingsStorageDatasetView::Degraded:
    return "DEGRADED";
  case SettingsStorageDatasetView::Corrupt:
    return "CORRUPT";
  case SettingsStorageDatasetView::ReadError:
    return "READ ERROR";
  }
  return "UNAVAILABLE";
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
                               const char *clock_text, const char *due_text,
                               bool save_failed) {
  prototype_ui::begin_page(canvas);
  prototype_ui::draw_header(canvas, "SETTINGS",
                            "DEVICE SETTINGS AND PREGNANCY PROFILE", "APPS");
  draw_setting(canvas, 0, "LANGUAGE",
               save_failed ? "SAVE FAILED - RETRYING" : "SAVED ON MICROSD",
               ui_language_is_chinese() ? "简体中文" : "ENGLISH");
  draw_setting(canvas, 1, "DEVICE TIME", "USED BY ALL DAILY RECORDS",
               rtc_ready ? clock_text : "UNAVAILABLE");
  draw_setting(canvas, 2, "PREGNANCY SETTINGS", "DUE DATE / LAST PERIOD",
               due_text);
  draw_setting(canvas, 3, "DISPLAY CLEAN REFRESH", "CLEARS E-INK GHOSTING",
               "RUN");
  draw_setting(canvas, 4, "MICROSD STORAGE", "USER DATA", "VIEW");
  const bool is_portrait = portrait(canvas.width(), canvas.height());
  ui_draw_wrapped_text_pixels(
      canvas, ui_tokens::kContentMargin, is_portrait ? 660 : 424, canvas.width() - 2 * ui_tokens::kContentMargin,
      ui_language_is_chinese()
          ? "所有孕期记录仅保存在设备。本产品用于信息整理，不替代专业医疗建议。"
          : "ALL RECORDS STAY ON DEVICE. INFORMATION ONLY; NOT MEDICAL ADVICE.",
      ui_tokens::kSecondaryBody, GrayLevel::Black, is_portrait ? 3 : 2, 5,
      FontWeight::Regular);
}

void settings_page_render_storage(Canvas &canvas,
                                  const SettingsStoragePageState &state) {
  prototype_ui::begin_page(canvas);
  const bool is_portrait = portrait(canvas.width(), canvas.height());
  prototype_ui::draw_header(canvas, "MICROSD STORAGE",
                            is_portrait
                                ? "USER DATA"
                                : "USER DATA ON THE MEMORY CARD",
                            "BACK");
  draw_form_row(canvas, 0, "MEMORY CARD", "CURRENT MEDIA",
                storage_card_value(state.card));
  draw_form_row(canvas, 1, "ACCESS", nullptr,
                storage_access_value(state.card));
  draw_form_row(canvas, 2, "DATA ON CARD", nullptr,
                storage_dataset_value(state.dataset));

  char capacity[36] = "CAPACITY UNAVAILABLE";
  if (state.capacity_known) {
    std::snprintf(capacity, sizeof(capacity), "FREE %u / %u MiB",
                  static_cast<unsigned>(state.free_mib),
                  static_cast<unsigned>(state.total_mib));
  }
  ui_draw_wrapped_text_pixels(
      canvas, ui_tokens::kContentMargin, is_portrait ? 480 : 300, canvas.width() - 2 * ui_tokens::kContentMargin,
      ui_text(state.operation_message != nullptr
                  ? state.operation_message
                  : capacity),
      ui_tokens::kBody, GrayLevel::Black, 2, 4, FontWeight::Regular);
  prototype_ui::draw_button(
      canvas, storage_split_button(0U, canvas.width(), canvas.height()),
      state.initialize_enabled
          ? "INITIALIZE CARD"
          : (state.eject_enabled ? "SAFE EJECT" : "STORAGE UNAVAILABLE"),
      state.initialize_enabled || state.eject_enabled, 2);
  prototype_ui::draw_button(
      canvas, storage_split_button(1U, canvas.width(), canvas.height()),
      "MORE", false, 2);
}

// A navigation row of the "more" page: label, one-line explanation, and a
// chevron, or UNAVAILABLE when the action cannot run now. The rectangle is the
// same form_row() the hit test uses.
void draw_nav_row(Canvas &canvas, size_t index, const char *label,
                  const char *sub, bool enabled) {
  const Rect row = form_row(index, canvas.width(), canvas.height());
  canvas.draw_line(row.x, row.y + row.height - 1, row.x + row.width,
                   row.y + row.height - 1, GrayLevel::Black);
  canvas.draw_text_pixels(row.x, row.y + 10, ui_text(label), ui_tokens::kBody,
                          GrayLevel::Black, FontWeight::Medium);
  canvas.draw_text_pixels(row.x, row.y + 10 + ui_tokens::kBody + 6, ui_text(sub),
                          ui_tokens::kSecondaryBody, GrayLevel::Black,
                          FontWeight::Regular);
  if (enabled) {
    prototype_ui::draw_chevron(canvas, row.x + row.width - 12,
                               row.y + row.height / 2 - 8);
  } else {
    const char *text = ui_text("UNAVAILABLE");
    const int width = font_text_width_pixels(text, ui_tokens::kSecondaryBody,
                                             FontWeight::Regular);
    canvas.draw_text_pixels(row.x + row.width - width,
                            row.y + (row.height - ui_tokens::kSecondaryBody) / 2,
                            text, ui_tokens::kSecondaryBody, GrayLevel::Black,
                            FontWeight::Regular);
  }
}

void settings_page_render_storage_more(Canvas &canvas,
                                       const SettingsStoragePageState &state) {
  prototype_ui::begin_page(canvas);
  const bool is_portrait = portrait(canvas.width(), canvas.height());
  prototype_ui::draw_header(canvas, "MORE OPTIONS", "USER DATA", "BACK");
  draw_nav_row(canvas, 0, "PROFILE / WEIGHT / SETTINGS",
               "COPY OLD DEVICE RECORDS", state.migration_enabled);
  draw_nav_row(canvas, 1, "KICKS", "COPY OLD DEVICE RECORDS",
               state.migration_enabled);
  draw_nav_row(canvas, 2, "DELETE CARD DATA", "THIS APP'S DATA ONLY",
               state.delete_enabled);
  draw_nav_row(canvas, 3, "RESET DEVICE BINDING", "RESET LOCAL DEVICE", true);
  char identity[64] = "DATASET ID UNAVAILABLE";
  if (state.dataset_id_prefix[0] != '\0') {
    std::snprintf(identity, sizeof(identity), "ID %.8s...  GEN %llu",
                  state.dataset_id_prefix,
                  static_cast<unsigned long long>(state.dataset_generation));
  }
  ui_draw_wrapped_text_pixels(
      canvas, ui_tokens::kContentMargin, is_portrait ? 600 : 368,
      canvas.width() - 2 * ui_tokens::kContentMargin, ui_text(identity),
      ui_tokens::kSecondaryBody, GrayLevel::Black, 1, 3, FontWeight::Regular);
}

void settings_page_render_storage_confirm(Canvas &canvas) {
  prototype_ui::begin_page(canvas);
  const bool is_portrait = portrait(canvas.width(), canvas.height());
  prototype_ui::draw_header(
      canvas, "INITIALIZE MICROSD",
      is_portrait ? "PREGJNL DATASET · NO FORMAT"
                  : "CREATE PREGJNL DATASET · CARD IS NOT FORMATTED",
      "BACK");
  const int rule_y = is_portrait ? 112 : 98;
  canvas.draw_line(24, rule_y, canvas.width() - 24, rule_y,
                   GrayLevel::Black);
  canvas.draw_text_pixels(ui_tokens::kContentMargin, is_portrait ? 144 : 122,
                          ui_text("CONFIRM NEW DATASET"),
                          ui_tokens::kSectionHeading, GrayLevel::Black,
                          FontWeight::Medium);
  ui_draw_wrapped_text_pixels(
      canvas, ui_tokens::kContentMargin, is_portrait ? 196 : 170, canvas.width() - 2 * ui_tokens::kContentMargin,
      ui_text("THIS CREATES THE PREGJNL DATA FOLDER. IT DOES NOT FORMAT THE "
              "CARD OR DELETE OTHER FILES."),
      ui_tokens::kBody, GrayLevel::Black, is_portrait ? 5 : 3,
      ui_tokens::kBodyLineGap, FontWeight::Regular);
  ui_draw_wrapped_text_pixels(
      canvas, ui_tokens::kContentMargin, is_portrait ? 350 : 270, canvas.width() - 2 * ui_tokens::kContentMargin,
      ui_text("FUTURE USER RECORDS AND AUDIO WILL BE SAVED ON THIS CARD."),
      ui_tokens::kBody, GrayLevel::Black, is_portrait ? 4 : 2,
      ui_tokens::kBodyLineGap, FontWeight::Medium);
  prototype_ui::draw_button(
      canvas, storage_confirm_button(0U, canvas.width(), canvas.height()),
      "CANCEL", false, 2);
  prototype_ui::draw_button(
      canvas, storage_confirm_button(1U, canvas.width(), canvas.height()),
      "CREATE DATASET", true, 2);
}


void settings_page_render_legacy_migration(
    Canvas &canvas, const SettingsMigrationPageState &state) {
  prototype_ui::begin_page(canvas);
  const bool is_portrait = portrait(canvas.width(), canvas.height());
  prototype_ui::draw_header(canvas, state.kicks_only ? "KICKS" : "OLD DEVICE DATA",
                            "NVS TO CURRENT MICROSD DATASET", "BACK");
  const char *status = "NOT SCANNED";
  switch (state.view) {
  case SettingsMigrationView::Importing: status = "IMPORTING"; break;
  case SettingsMigrationView::Verified: status = "IMPORT VERIFIED"; break;
  case SettingsMigrationView::NeedsReview: status = "REVIEW REQUIRED"; break;
  case SettingsMigrationView::SourceError: status = "NVS READ ERROR"; break;
  case SettingsMigrationView::StorageError: status = "MICROSD ERROR"; break;
  case SettingsMigrationView::Cleared: status = "OLD COPIES CLEARED"; break;
  case SettingsMigrationView::NotScanned: break;
  }
  char valid[24] = {};
  char imported[24] = {};
  char conflicts[24] = {};
  std::snprintf(valid, sizeof(valid), "%u", static_cast<unsigned>(state.valid));
  std::snprintf(imported, sizeof(imported), "%u + %u EXISTING",
                static_cast<unsigned>(state.imported),
                static_cast<unsigned>(state.already_present));
  std::snprintf(conflicts, sizeof(conflicts), "%u / %u",
                static_cast<unsigned>(state.conflicts),
                static_cast<unsigned>(state.corrupt));
  draw_form_row(canvas, 0, "STATUS", "CURRENT RESULT", status);
  draw_form_row(canvas, 1, "VALID OLD RECORDS", nullptr, valid);
  draw_form_row(canvas, 2, "IMPORTED", "NEW + ALREADY ON CARD", imported);
  draw_form_row(canvas, 3, "CONFLICT / CORRUPT", nullptr, conflicts);
  ui_draw_wrapped_text_pixels(
      canvas, ui_tokens::kContentMargin, is_portrait ? 606 : 366, canvas.width() - 2 * ui_tokens::kContentMargin,
      ui_text(state.kicks_only
                  ? "KICK COPIES ARE CLEARED ONLY AFTER VERIFIED IMPORT."
                  : state.kicks_preserved
                  ? "LEGACY KICK DATA IS PRESENT AND WILL BE PRESERVED."
                  : "LEGACY KICK DATA IS NEVER DELETED BY THIS TOOL."),
      ui_tokens::kSecondaryBody, GrayLevel::Black, is_portrait ? 3 : 1, 4,
      FontWeight::Regular);
  prototype_ui::draw_button(
      canvas, storage_split_button(0U, canvas.width(), canvas.height()),
      state.view == SettingsMigrationView::Importing ? "IMPORTING"
                                                     : "SCAN + IMPORT",
      state.import_enabled, 2);
  prototype_ui::draw_button(
      canvas, storage_split_button(1U, canvas.width(), canvas.height()),
      "CLEAR OLD COPIES", state.clear_enabled, 2);
}

void settings_page_render_legacy_clear_confirm(Canvas &canvas, bool kicks_only) {
  prototype_ui::begin_page(canvas);
  const bool is_portrait = portrait(canvas.width(), canvas.height());
  prototype_ui::draw_header(canvas, "CLEAR OLD COPIES",
                            "ONLY AFTER VERIFIED IMPORT", "BACK");
  canvas.draw_text_pixels(ui_tokens::kContentMargin, is_portrait ? 144 : 122,
                          ui_text("CONFIRM NVS CLEANUP"),
                          ui_tokens::kSectionHeading, GrayLevel::Black,
                          FontWeight::Medium);
  ui_draw_wrapped_text_pixels(
      canvas, ui_tokens::kContentMargin, is_portrait ? 196 : 170, canvas.width() - 2 * ui_tokens::kContentMargin,
      ui_text(kicks_only
          ? "REMOVES ONLY VERIFIED OLD KICK COPIES. MICROSD RECORDS ARE NOT CHANGED."
          : "REMOVES OLD PREGNANCY, WEIGHT AND DEVICE SETTING COPIES. MICROSD RECORDS ARE NOT CHANGED."),
      ui_tokens::kBody, GrayLevel::Black, is_portrait ? 6 : 3,
      ui_tokens::kBodyLineGap, FontWeight::Regular);
  ui_draw_wrapped_text_pixels(
      canvas, ui_tokens::kContentMargin, is_portrait ? 390 : 290, canvas.width() - 2 * ui_tokens::kContentMargin,
      ui_text("REMINDER AND CHECKUP COPIES ARE PRESERVED FOR V2."),
      ui_tokens::kBody, GrayLevel::Black, is_portrait ? 3 : 2,
      ui_tokens::kBodyLineGap, FontWeight::Medium);
  prototype_ui::draw_button(
      canvas, storage_split_button(0U, canvas.width(), canvas.height()),
      "CANCEL", false, 2);
  prototype_ui::draw_button(
      canvas, storage_split_button(1U, canvas.width(), canvas.height()),
      "CLEAR VERIFIED COPIES", true, 2);
}

void settings_page_render_card_data_delete_confirm(
    Canvas &canvas, const char *dataset_id_prefix) {
  prototype_ui::begin_page(canvas);
  const bool is_portrait = portrait(canvas.width(), canvas.height());
  prototype_ui::draw_header(canvas, "DELETE CARD APP DATA",
                            "CURRENT PREGJNL DATASET ONLY", "BACK");
  char identity[48] = "DATASET ID UNAVAILABLE";
  if (dataset_id_prefix != nullptr && dataset_id_prefix[0] != '\0')
    std::snprintf(identity, sizeof(identity), "DATASET %.8s...",
                  dataset_id_prefix);
  canvas.draw_text_pixels(ui_tokens::kContentMargin, is_portrait ? 144 : 122, ui_text(identity),
                          ui_tokens::kSectionHeading, GrayLevel::Black,
                          FontWeight::Medium);
  ui_draw_wrapped_text_pixels(
      canvas, ui_tokens::kContentMargin, is_portrait ? 196 : 170, canvas.width() - 2 * ui_tokens::kContentMargin,
      ui_text("PERMANENTLY REMOVES THE PREGJNL APPLICATION DIRECTORY. IT "
              "DOES NOT FORMAT THE CARD OR DELETE OTHER CARD FILES."),
      ui_tokens::kBody, GrayLevel::Black, is_portrait ? 6 : 3,
      ui_tokens::kBodyLineGap, FontWeight::Regular);
  ui_draw_wrapped_text_pixels(
      canvas, ui_tokens::kContentMargin, is_portrait ? 390 : 290, canvas.width() - 2 * ui_tokens::kContentMargin,
      ui_text("DEVICE RESET AND OLD NVS COPY CLEANUP ARE SEPARATE ACTIONS."),
      ui_tokens::kBody, GrayLevel::Black, is_portrait ? 3 : 2,
      ui_tokens::kBodyLineGap, FontWeight::Medium);
  prototype_ui::draw_button(
      canvas, storage_split_button(0U, canvas.width(), canvas.height()),
      "CANCEL", false, 2);
  prototype_ui::draw_button(
      canvas, storage_split_button(1U, canvas.width(), canvas.height()),
      "DELETE PREGJNL DATA", true, 2);
}

void settings_page_render_device_reset_confirm(Canvas &canvas) {
  prototype_ui::begin_page(canvas);
  const bool is_portrait = portrait(canvas.width(), canvas.height());
  prototype_ui::draw_header(canvas, "RESET DEVICE BINDING",
                            "LOCAL NETWORK CREDENTIAL KEY ONLY", "BACK");
  canvas.draw_text_pixels(ui_tokens::kContentMargin, is_portrait ? 144 : 122,
                          ui_text("CONFIRM LOCAL RESET"),
                          ui_tokens::kSectionHeading, GrayLevel::Black,
                          FontWeight::Medium);
  ui_draw_wrapped_text_pixels(
      canvas, ui_tokens::kContentMargin, is_portrait ? 196 : 170, canvas.width() - 2 * ui_tokens::kContentMargin,
      ui_text("FORGETS THIS DEVICE'S LOCAL NETWORK KEY AND CLEARS THE RAM-ONLY "
              "WI-FI CONFIGURATION. NETWORK AUTHORIZATION MUST BE SET UP AGAIN."),
      ui_tokens::kBody, GrayLevel::Black, is_portrait ? 7 : 3,
      ui_tokens::kBodyLineGap, FontWeight::Regular);
  ui_draw_wrapped_text_pixels(
      canvas, ui_tokens::kContentMargin, is_portrait ? 420 : 290, canvas.width() - 2 * ui_tokens::kContentMargin,
      ui_text("MICROSD PREGJNL DATA, OTHER CARD FILES AND OLD NVS RECORDS ARE "
              "NOT DELETED."),
      ui_tokens::kBody, GrayLevel::Black, is_portrait ? 4 : 2,
      ui_tokens::kBodyLineGap, FontWeight::Medium);
  prototype_ui::draw_button(
      canvas, storage_split_button(0U, canvas.width(), canvas.height()),
      "CANCEL", false, 2);
  prototype_ui::draw_button(
      canvas, storage_split_button(1U, canvas.width(), canvas.height()),
      "RESET THIS DEVICE", true, 2);
}

// The box holding the editable fields of the time editor.
Rect time_box(int width, int height) {
  return portrait(width, height) ? Rect{24, 152, 432, 76} : Rect{24, 148, 424, 76};
}
uint8_t time_text_size(int width, int height) {
  return portrait(width, height) ? 21U : 28U;
}

void settings_page_render_time_editor(Canvas &canvas, const DigitFields &fields,
                                      bool input_error) {
  prototype_ui::begin_page(canvas);
  prototype_ui::draw_header(canvas, "SET DEVICE TIME",
                            "YYYY / MM / DD · HH : MM", "CANCEL");
  const bool is_portrait = portrait(canvas.width(), canvas.height());
  canvas.draw_line(24, is_portrait ? 108 : 100, canvas.width() - 24,
                   is_portrait ? 108 : 100, GrayLevel::Black);
  canvas.draw_text(24, is_portrait ? 124 : 118, ui_text("TAP A FIELD TO EDIT ONLY THAT PART"), 1,
                   GrayLevel::Black);
  const Rect box = time_box(canvas.width(), canvas.height());
  digit_fields_draw(canvas, box, fields,
                    time_text_size(canvas.width(), canvas.height()));
  ui_draw_wrapped_text(
      canvas, 24, is_portrait ? 250 : 238,
      is_portrait ? canvas.width() - 2 * ui_tokens::kContentMargin : 424,
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

bool settings_page_back_target(SettingsPage page, SettingsPage &target) {
  switch (page) {
  case SettingsPage::Main:
    return false;
  case SettingsPage::TimeEditor:
  case SettingsPage::PregnancySummary:
  case SettingsPage::StorageSummary:
    target = SettingsPage::Main;
    return true;
  case SettingsPage::StorageConfirm:
  case SettingsPage::StorageMore:
    target = SettingsPage::StorageSummary;
    return true;
  case SettingsPage::LegacyMigration:
  case SettingsPage::CardDataDeleteConfirm:
  case SettingsPage::DeviceResetConfirm:
    target = SettingsPage::StorageMore;
    return true;
  case SettingsPage::LegacyClearConfirm:
    target = SettingsPage::LegacyMigration;
    return true;
  }
  return false;
}

size_t settings_page_targets(SettingsPage page, int width, int height,
                             bool primary_action_enabled,
                             bool secondary_action_enabled,
                             SettingsTarget *out, size_t capacity) {
  size_t count = 0U;
  const auto add = [&](const Rect &rect, SettingsAction action) {
    if (out != nullptr && count < capacity) out[count] = {rect, action};
    ++count;
  };
  add(prototype_ui::header_action_rect(width, height), SettingsAction::Back);
  const auto split = [&](SettingsAction first, SettingsAction second) {
    add(storage_split_button(0U, width, height), first);
    add(storage_split_button(1U, width, height), second);
  };
  if (page == SettingsPage::Main) {
    constexpr SettingsAction kRowActions[] = {
        SettingsAction::ToggleLanguage, SettingsAction::EditTime,
        SettingsAction::PregnancySettings, SettingsAction::RefreshDisplay,
        SettingsAction::StorageSettings};
    for (size_t index = 0U; index < 5U; ++index)
      add(setting_row(index, width, height), kRowActions[index]);
  } else if (page == SettingsPage::PregnancySummary) {
    add(pregnancy_button(0U, width, height), SettingsAction::EditPregnancy);
    add(pregnancy_button(1U, width, height), SettingsAction::Home);
  } else if (page == SettingsPage::StorageSummary) {
    if (primary_action_enabled)
      add(storage_split_button(0U, width, height),
          SettingsAction::BeginStorageInitialization);
    add(storage_split_button(1U, width, height), SettingsAction::ManageStorage);
  } else if (page == SettingsPage::StorageConfirm) {
    add(storage_confirm_button(0U, width, height),
        SettingsAction::CancelStorageInitialization);
    add(storage_confirm_button(1U, width, height),
        SettingsAction::ConfirmStorageInitialization);
  } else if (page == SettingsPage::StorageMore) {
    // primary = old-data import available, secondary = card-data delete available
    if (primary_action_enabled) {
      add(form_row(0U, width, height), SettingsAction::ManageLegacyCommon);
      add(form_row(1U, width, height), SettingsAction::ManageLegacyKicks);
    }
    if (secondary_action_enabled)
      add(form_row(2U, width, height), SettingsAction::BeginCardDataDelete);
    add(form_row(3U, width, height), SettingsAction::BeginDeviceReset);
  } else if (page == SettingsPage::LegacyMigration) {
    if (primary_action_enabled)
      add(storage_split_button(0U, width, height), SettingsAction::BeginLegacyImport);
    if (secondary_action_enabled)
      add(storage_split_button(1U, width, height), SettingsAction::BeginLegacyClear);
  } else if (page == SettingsPage::LegacyClearConfirm) {
    split(SettingsAction::CancelLegacyClear, SettingsAction::ConfirmLegacyClear);
  } else if (page == SettingsPage::CardDataDeleteConfirm) {
    split(SettingsAction::CancelCardDataDelete, SettingsAction::ConfirmCardDataDelete);
  } else if (page == SettingsPage::DeviceResetConfirm) {
    split(SettingsAction::CancelDeviceReset, SettingsAction::ConfirmDeviceReset);
  } else {
    // The time fields select which part the keys edit.
    Rect cells[DigitFields::kMaxFields];
    const size_t fields = digit_field_cells(
        time_box(width, height), DigitFieldKind::DateTime,
        time_text_size(width, height), cells, DigitFields::kMaxFields);
    for (size_t index = 0U; index < fields; ++index)
      add(cells[index], static_cast<SettingsAction>(
                            static_cast<int>(SettingsAction::Field0) +
                            static_cast<int>(index)));
    for (size_t index = 0U; index < 12U; ++index)
      add(key_rect(index, width, height), kKeyActions[index]);
  }
  return count < capacity ? count : capacity;
}

SettingsAction settings_page_action_at(SettingsPage page, int width, int height,
                                       int x, int y,
                                       bool primary_action_enabled,
                                       bool secondary_action_enabled) {
  SettingsTarget targets[kMaxSettingsTargets];
  const size_t count = settings_page_targets(
      page, width, height, primary_action_enabled, secondary_action_enabled,
      targets, kMaxSettingsTargets);
  for (size_t index = 0U; index < count; ++index) {
    if (targets[index].rect.contains(x, y))
      return targets[index].action;
  }
  return SettingsAction::None;
}

bool settings_action_field(SettingsAction action, size_t &field) {
  if (action < SettingsAction::Field0 || action > SettingsAction::Field4)
    return false;
  field = static_cast<size_t>(static_cast<int>(action) -
                              static_cast<int>(SettingsAction::Field0));
  return true;
}

bool settings_action_digit(SettingsAction action, char &digit) {
  if (action < SettingsAction::Digit0 || action > SettingsAction::Digit9)
    return false;
  digit = static_cast<char>('0' + static_cast<int>(action) -
                            static_cast<int>(SettingsAction::Digit0));
  return true;
}

bool settings_storage_state_equal(const SettingsStoragePageState &a,
                                  const SettingsStoragePageState &b) {
  return a.mount_generation == b.mount_generation && a.card == b.card &&
      a.dataset == b.dataset && a.requests_open == b.requests_open &&
      a.initialize_enabled == b.initialize_enabled && a.eject_enabled == b.eject_enabled &&
      a.migration_enabled == b.migration_enabled && a.delete_enabled == b.delete_enabled &&
      a.capacity_known == b.capacity_known && a.total_mib == b.total_mib &&
      a.free_mib == b.free_mib && a.dataset_generation == b.dataset_generation &&
      std::strcmp(a.dataset_id_prefix, b.dataset_id_prefix) == 0 &&
      std::strcmp(a.operation_message ? a.operation_message : "",
                  b.operation_message ? b.operation_message : "") == 0;
}
