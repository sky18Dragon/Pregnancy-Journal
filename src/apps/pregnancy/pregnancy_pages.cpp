#include "pregnancy_pages.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "battery_status_overlay.h"
#include "canvas.h"
#include "digit_fields.h"
#include "font.h"
#include "pixel_asset.h"
#include "pregnancy_plate_assets.h"
#include "prototype_ui.h"
#include "text_layout.h"
#include "ui_language.h"
#include "ui_tokens.h"

namespace {

constexpr int kScreenWidth = 800;
constexpr int kScreenHeight = 480;

using prototype_ui::Rect;

// One PageLayout per orientation is the single source of geometry for the
// pregnancy pages: renderers draw these rectangles and pregnancy_page_targets()
// (and so pregnancy_page_action_at()) hit-tests the very same ones.
// 每个方向只有一份布局：页面绘制与点击命中都使用这里的同一组矩形。
struct PageLayout {
  Rect keys[12];      // 1-9, DEL, 0, NEXT
  Rect back;          // CANCEL on the setup pages
  Rect home;          // HOME on the Baby/Mom pages
  Rect due_source;    // source setup: estimated due date
  Rect lmp_source;    // source setup: last menstrual period
  Rect tabs[3];       // Baby/Mom pages: overview, baby, mom
  Rect menu;          // dashboard menu icon (56 px hit area)
  Rect hero;          // dashboard engraving -> Baby page
  Rect numeral;       // dashboard week number block -> Mom page
  Rect input;         // setup pages: box holding the digit fields
  uint8_t input_size; // text size of the digit fields
  int caption_y;      // dashboard caption row
  Rect grid;          // dashboard 8 x 5 week grid (display only)
  int grid_cell;      // grid cell size
  int grid_pitch;     // grid cell spacing
  int numeral_x;      // dashboard numeral: left edge, or right edge when aligned right
  int numeral_y;      // ... top of the digits; WEEKS follows kWeeksGap below
  bool numeral_right; // true: numeral and WEEKS are right-aligned to numeral_x
  int status_x;       // dashboard status row (battery, moon): left edge, hugging the top-left corner
  int status_y;       // ... top of the 22 px battery
};

constexpr PageLayout kLandscape = {
    {{500, 70, 84, 66},  {594, 70, 84, 66},  {688, 70, 84, 66},
     {500, 146, 84, 66}, {594, 146, 84, 66}, {688, 146, 84, 66},
     {500, 222, 84, 66}, {594, 222, 84, 66}, {688, 222, 84, 66},
     {500, 298, 84, 66}, {594, 298, 84, 66}, {688, 298, 84, 66}},
    {30, 390, 180, ui_tokens::kButtonHeight},
    {656, 20, 120, ui_tokens::kMinimumTouchHeight},
    {54, 150, 320, 170},
    {426, 150, 320, 170},
    {{24, 408, 128, ui_tokens::kMinimumTouchHeight},
     {152, 408, 128, ui_tokens::kMinimumTouchHeight},
     {280, 408, 128, ui_tokens::kMinimumTouchHeight}},
    {720, 24, ui_tokens::kButtonHeight, ui_tokens::kButtonHeight},
    {410, 80, 340, 340},
    {36, 76, 200, 140},
    {30, 118, 430, 86},
    28,
    430,
    {48, 252, 252, 156},
    28,
    32,
    48,
    88,
    false,
    12,
    12,
};

constexpr PageLayout kPortrait = {
    {{24, 278, 132, 72}, {174, 278, 132, 72}, {324, 278, 132, 72},
     {24, 366, 132, 72}, {174, 366, 132, 72}, {324, 366, 132, 72},
     {24, 454, 132, 72}, {174, 454, 132, 72}, {324, 454, 132, 72},
     {24, 542, 132, 72}, {174, 542, 132, 72}, {324, 542, 132, 72}},
    {24, 678, 432, ui_tokens::kButtonHeight},
    {336, 20, 120, ui_tokens::kMinimumTouchHeight},
    {24, 150, 432, 190},
    {24, 370, 432, 190},
    {{24, 728, 144, ui_tokens::kMinimumTouchHeight},
     {168, 728, 144, ui_tokens::kMinimumTouchHeight},
     {312, 728, 144, ui_tokens::kMinimumTouchHeight}},
    {400, 24, ui_tokens::kButtonHeight, ui_tokens::kButtonHeight},
    {70, 130, 340, 340},
    {330, 646, 138, 130},
    {24, 102, 432, 76},
    21,
    488,
    {24, 630, 236, 146},
    26,
    30,
    456,
    660,
    true,
    12,
    12,
};

const PageLayout &layout_for(bool portrait) {
  return portrait ? kPortrait : kLandscape;
}

constexpr PregnancyAction kKeyActions[] = {
    PregnancyAction::Digit1, PregnancyAction::Digit2, PregnancyAction::Digit3,
    PregnancyAction::Digit4, PregnancyAction::Digit5, PregnancyAction::Digit6,
    PregnancyAction::Digit7, PregnancyAction::Digit8, PregnancyAction::Digit9,
    PregnancyAction::Delete, PregnancyAction::Digit0, PregnancyAction::Continue,
};

constexpr const char *kKeyLabels[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "DEL", "0", "NEXT",
};
bool portrait_layout(const Canvas &canvas) {
  return canvas.height() > canvas.width();
}

// The week page is one plate of an engraved week-by-week sheet: a big condensed
// numeral with WEEKS under it, the stipple engraving, a letter-spaced caption.
// 孕周首页：与参考图的单张图版一致——大号数字、WEEKS、点刻插画、字距拉开的英文名。
constexpr int kPlateTracking = 6;       // caption letter spacing
constexpr int kWeeksTracking = 3;
constexpr int kChineseTracking = 10;    // Chinese captions and the week label need more air
constexpr int kWeeksGap = 12;           // numeral bottom to WEEKS top

// Letter-spaced text. Walks UTF-8 so Chinese captions are spaced per character.
size_t utf8_length(unsigned char lead) {
  return lead < 0x80U ? 1U : (lead < 0xE0U ? 2U : (lead < 0xF0U ? 3U : 4U));
}

int tracked_text_width(const char *text, uint8_t pixel_size, int tracking,
                       FontWeight weight) {
  int width = 0;
  char one[5] = {};
  for (const char *c = text; *c != '\0';) {
    const size_t length = utf8_length(static_cast<unsigned char>(*c));
    std::memcpy(one, c, length);
    one[length] = '\0';
    width += font_text_width_pixels(one, pixel_size, weight) + tracking;
    c += length;
  }
  return width > 0 ? width - tracking : 0;
}

void draw_tracked_text(Canvas &canvas, int x, int y, const char *text,
                       uint8_t pixel_size, int tracking, FontWeight weight) {
  char one[5] = {};
  for (const char *c = text; *c != '\0';) {
    const size_t length = utf8_length(static_cast<unsigned char>(*c));
    std::memcpy(one, c, length);
    one[length] = '\0';
    canvas.draw_text_pixels(x, y, one, pixel_size, GrayLevel::Black, weight);
    x += font_text_width_pixels(one, pixel_size, weight) + tracking;
    c += length;
  }
}

void draw_plate_numeral(Canvas &canvas, int x, int y, const PlateNumeral &font,
                        const char *digits) {
  for (const char *c = digits; *c >= '0' && *c <= '9'; ++c) {
    const int index = *c - '0';
    pixel_asset_draw(canvas, x, y, font.digit[index], 1, GrayLevel::Black);
    x += font.advance[index];
  }
}

int plate_numeral_width(const PlateNumeral &font, const char *digits) {
  int width = 0, gap = 0;
  for (const char *c = digits; *c >= '0' && *c <= '9'; ++c) {
    const int index = *c - '0';
    width += font.advance[index];
    gap = font.advance[index] - static_cast<int>(font.digit[index].width);
  }
  return width - gap;
}

// 8 x 5 week grid, one cell per week. Solid = a finished week, ring with a dot =
// the week in progress, outline = still to come. Display only.
void draw_week_grid(Canvas &canvas, const PageLayout &layout, int finished_weeks) {
  for (int week = 1; week <= 40; ++week) {
    const int column = (week - 1) % 8, row = (week - 1) / 8;
    const Rect cell = {layout.grid.x + column * layout.grid_pitch,
                       layout.grid.y + row * layout.grid_pitch, layout.grid_cell,
                       layout.grid_cell};
    if (week <= finished_weeks) {
      canvas.fill_rect(cell.x, cell.y, cell.width, cell.height, GrayLevel::Black);
    } else if (week == finished_weeks + 1) {
      prototype_ui::draw_frame(canvas, cell, ui_tokens::kEmphasisStroke);
      const int inset = layout.grid_cell / 3;
      canvas.fill_rect(cell.x + inset, cell.y + inset, cell.width - 2 * inset,
                       cell.height - 2 * inset, GrayLevel::Black);
    } else {
      prototype_ui::draw_frame(canvas, cell, ui_tokens::kBorderStroke);
    }
  }
}

// Quiet menu: three strokes, no frame. The hit rectangle is the 56 px square.
void draw_menu_icon(Canvas &canvas, const Rect &rect) {
  for (int line = 0; line < 3; ++line)
    canvas.fill_rect(rect.x + 16, rect.y + 16 + line * 10, 24, 2,
                     GrayLevel::Black);
}

void render_keypad(Canvas &canvas, bool portrait) {
  for (size_t index = 0U; index < 12U; ++index) {
    const bool primary = index == 11U;
    const int scale = index == 9U || index == 11U ? 2 : 4;
    prototype_ui::draw_button(canvas, layout_for(portrait).keys[index],
                kKeyLabels[index], primary, scale);
  }
}

void render_setup(Canvas &canvas, const char *title, const char *caption,
                  const DigitFields &fields, bool input_error, bool can_cancel) {
  canvas.clear(GrayLevel::White);
  const bool portrait = portrait_layout(canvas);
  if (portrait) {
    canvas.draw_text(24, 24, title, 4, GrayLevel::Black);
    canvas.draw_text(24, 66, caption, 1, GrayLevel::Black);
    prototype_ui::draw_frame(canvas, kPortrait.input, ui_tokens::kBorderStroke);
    digit_fields_draw(canvas, kPortrait.input, fields, kPortrait.input_size);
    canvas.draw_text(24, 204,
                     input_error ? "CHECK THE DATE AND TRY AGAIN"
                                 : "TAP A FIELD TO EDIT ONLY THAT PART",
                     2, GrayLevel::Black);
    canvas.draw_text(24, 242, "SAVED ONLY ON THIS DEVICE", 1, GrayLevel::Black);
    if (can_cancel)
      prototype_ui::draw_button(canvas, kPortrait.back, "CANCEL", false, 3);
    render_keypad(canvas, true);
    return;
  }
  canvas.draw_text(30, 28, title, 4, GrayLevel::Black);
  canvas.draw_text(30, 72, caption, 2, GrayLevel::Black);

  prototype_ui::draw_frame(canvas, kLandscape.input, ui_tokens::kBorderStroke);
  digit_fields_draw(canvas, kLandscape.input, fields, kLandscape.input_size);

  canvas.draw_text(30, 235,
                   input_error ? "CHECK THE DATE AND TRY AGAIN"
                               : "TAP A FIELD TO EDIT ONLY THAT PART",
                   2, GrayLevel::Black);
  canvas.draw_text(30, 278, "SAVED ONLY ON THIS DEVICE", 2, GrayLevel::Black);

  if (can_cancel) {
    prototype_ui::draw_button(canvas, kLandscape.back, "CANCEL", false, 3);
  }
  render_keypad(canvas, false);
}

} // namespace

void pregnancy_page_render_clock_setup(Canvas &canvas,
                                       const DigitFields &fields,
                                       bool input_error, bool can_cancel) {
  render_setup(canvas, "SET DEVICE TIME", "YEAR / MONTH / DAY / HOUR / MINUTE",
               fields, input_error, can_cancel);
}

void pregnancy_page_render_due_date_setup(Canvas &canvas,
                                          const DigitFields &fields,
                                          bool input_error, bool can_cancel) {
  render_setup(canvas, "SET DUE DATE", "USE YOUR CONFIRMED ESTIMATED DUE DATE",
               fields, input_error, can_cancel);
}

void pregnancy_page_render_source_setup(Canvas &canvas, bool can_cancel) {
  canvas.clear(GrayLevel::White);
  if (portrait_layout(canvas)) {
    canvas.draw_text(24, 26, ui_text("PREGNANCY SETUP"), 4, GrayLevel::Black);
    canvas.draw_text(24, 72, ui_text("CHOOSE THE DATE YOU KNOW"), 2,
                     GrayLevel::Black);
    prototype_ui::draw_button(canvas, kPortrait.due_source, ui_text("ESTIMATED DUE DATE"),
                true, 3);
    prototype_ui::draw_button(canvas, kPortrait.lmp_source,
                ui_text("LAST MENSTRUAL PERIOD"), false, 3);
    ui_draw_wrapped_text_pixels(
        canvas, ui_tokens::kContentMargin, 594, 432,
        ui_text("A CLINICIAN-CONFIRMED DUE DATE IS PREFERRED"),
        ui_tokens::kSecondaryBody, GrayLevel::Black, 2, 2, FontWeight::Regular);
    if (can_cancel)
      prototype_ui::draw_button(canvas, kPortrait.back, "CANCEL", false, 3);
    return;
  }
  canvas.draw_text(42, 28, ui_text("PREGNANCY SETUP"), 5, GrayLevel::Black);
  canvas.draw_text(42, 86, ui_text("CHOOSE THE DATE YOU KNOW"), 2,
                   GrayLevel::Black);
  prototype_ui::draw_button(canvas, kLandscape.due_source, ui_text("ESTIMATED DUE DATE"), true, 3);
  prototype_ui::draw_button(canvas, kLandscape.lmp_source, ui_text("LAST MENSTRUAL PERIOD"), false,
              3);
  canvas.draw_text(54, 350,
                   ui_text("A CLINICIAN-CONFIRMED DUE DATE IS PREFERRED"), 2,
                   GrayLevel::Black);
  if (can_cancel)
    prototype_ui::draw_button(canvas, kLandscape.back, "CANCEL", false, 3);
}

void pregnancy_page_render_profile_date_setup(Canvas &canvas,
                                              const DigitFields &fields,
                                              bool input_error,
                                              bool due_date_primary) {
  render_setup(canvas,
               due_date_primary ? ui_text("SET DUE DATE")
                                : ui_text("SET LAST MENSTRUAL PERIOD"),
               due_date_primary
                   ? ui_text("USE YOUR CONFIRMED ESTIMATED DUE DATE")
                   : ui_text("USE THE FIRST DAY OF YOUR LAST PERIOD"),
               fields, input_error, true);
}

void pregnancy_page_render_dashboard(Canvas &canvas,
                                     const PregnancyDate &due_date,
                                     const PregnancyProgress &progress) {
  (void)due_date;
  prototype_ui::begin_page(canvas);
  const bool portrait = portrait_layout(canvas);
  const PageLayout &layout = layout_for(portrait);
  const PlateNumeral &numeral = pregnancy_plate_numeral();
  const int weeks = std::max(0, std::min(99, static_cast<int>(progress.weeks)));
  const uint8_t week = static_cast<uint8_t>(weeks);

  // Portrait is a desk calendar: picture on top, week grid bottom left, week
  // number bottom right. Landscape puts the picture on the right and stacks the
  // number, WEEKS and the grid in one left column. The object's name is centred
  // under the picture in both.
  // 竖屏台历式；横屏图案在右、周数/WEEKS/周图同在左栏；英文名都在图案正下方。
  draw_week_grid(canvas, layout, weeks);

  const int hero_x = layout.hero.x + layout.hero.width / 2;
  pixel_asset_draw_centered(
      canvas, hero_x, layout.hero.y + layout.hero.height / 2,
      pregnancy_plate_asset(week), 1, GrayLevel::Black);

  // Chinese mode shows the Chinese name and 孕周; English mode keeps the plate's
  // English caption and WEEKS.
  const bool chinese = ui_language_is_chinese();
  const char *caption =
      chinese ? pregnancy_plate_caption_zh(week) : pregnancy_plate_caption(week);
  const int caption_tracking = chinese ? kChineseTracking : kPlateTracking;
  const int caption_width = tracked_text_width(
      caption, ui_tokens::kBody, caption_tracking, FontWeight::Medium);
  draw_tracked_text(canvas, hero_x - caption_width / 2, layout.caption_y, caption,
                    ui_tokens::kBody, caption_tracking, FontWeight::Medium);

  char digits[8] = {};
  std::snprintf(digits, sizeof(digits), "%d", weeks);
  const int weeks_y = layout.numeral_y + numeral.height + kWeeksGap;
  const int numeral_width = plate_numeral_width(numeral, digits);
  const char *weeks_label = chinese ? "孕周" : "WEEKS";
  const int weeks_tracking = chinese ? kChineseTracking : kWeeksTracking;
  const int weeks_width = tracked_text_width(weeks_label, ui_tokens::kEmphasis,
                                             weeks_tracking, FontWeight::Medium);
  const int numeral_left =
      layout.numeral_right ? layout.numeral_x - numeral_width : layout.numeral_x;
  const int weeks_left =
      layout.numeral_right ? layout.numeral_x - weeks_width : layout.numeral_x + 2;
  draw_plate_numeral(canvas, numeral_left, layout.numeral_y, numeral, digits);
  draw_tracked_text(canvas, weeks_left, weeks_y, weeks_label, ui_tokens::kEmphasis,
                    weeks_tracking, FontWeight::Medium);

  draw_menu_icon(canvas, layout.menu);
}

void pregnancy_page_status_battery_origin(bool portrait, int &x, int &y) {
  const PageLayout &layout = layout_for(portrait);
  x = layout.status_x;
  y = layout.status_y;
}

void pregnancy_page_render_sleep_moon(Canvas &canvas) {
  constexpr int kGapAfterBattery = 12;
  const bool portrait = portrait_layout(canvas);
  int bx = 0;
  int by = 0;
  pregnancy_page_status_battery_origin(portrait, bx, by);
  const int left = bx + kBatteryStatusCompactWidth + kGapAfterBattery;
  const int top = by + kBatteryStatusCompactHeight / 2 - 17;   // the moon's 35 px box is centred on the battery
  canvas.fill_circle(left + 16, top + 17, 11, GrayLevel::Black);
  canvas.fill_circle(left + 21, top + 12, 10, GrayLevel::White);
}

void pregnancy_page_render_detail(Canvas &canvas, PregnancyPage page,
                                  const PregnancyProgress &progress,
                                  const WeekContent &content) {
  prototype_ui::begin_page(canvas);
  const bool chinese = ui_language_is_chinese();
  char title[48] = {};
  std::snprintf(title, sizeof(title),
                chinese ? "第 %u 周 · %s" : "WEEK %u · %s",
                static_cast<unsigned>(progress.weeks),
                chinese ? (page == PregnancyPage::Baby ? "宝宝" : "妈妈")
                        : (page == PregnancyPage::Baby ? "BABY" : "MOM"));
  if (portrait_layout(canvas)) {
    canvas.draw_text(24, 22, title, 3, GrayLevel::Black);
    prototype_ui::draw_button(canvas, kPortrait.home, chinese ? "返回" : "HOME", false, 2);
    canvas.draw_line(24, 82, 456, 82, GrayLevel::Black);
    canvas.draw_text(24, 108, chinese ? "孕周" : "WEEK", 2, GrayLevel::Black);
    char stat[20] = {};
    std::snprintf(stat, sizeof(stat), "%u+%u",
                  static_cast<unsigned>(progress.weeks),
                  static_cast<unsigned>(progress.days));
    canvas.draw_text(24, 140, stat, 6, GrayLevel::Black);
    canvas.draw_text(230, 150, ui_text(pregnancy_stage_name(progress.stage)), 2,
                     GrayLevel::Black);
    char remaining[48] = {};
    std::snprintf(remaining, sizeof(remaining),
                  chinese ? "距离预产期约 %d 天" : "ABOUT %d DAYS TO DUE DATE",
                  progress.days_until_due_date);
    ui_draw_wrapped_text_pixels(canvas, 230, 176, 226, remaining,
                                ui_tokens::kSecondaryBody, GrayLevel::Black, 2, 2,
                                FontWeight::Regular);
    canvas.draw_line(24, 220, 456, 220, GrayLevel::Black);

    if (page == PregnancyPage::Baby) {
      canvas.draw_text(24, 246,
                       chinese ? "宝宝正在感知外面的世界"
                               : "BABY IS DISCOVERING THE WORLD",
                       3, GrayLevel::Black);
      ui_draw_wrapped_text(canvas, 24, 292, 432,
                           chinese ? content.baby_development_zh
                                   : content.baby_development,
                           2, GrayLevel::Black, 5, 8);
      canvas.draw_line(24, 450, 456, 450, GrayLevel::Black);
      canvas.draw_text(24, 474, chinese ? "身长参考" : "SIZE GUIDE", 1,
                       GrayLevel::Black);
      canvas.draw_text(24, 502, ui_text(content.baby_size_text), 3, GrayLevel::Black);
      canvas.draw_text(250, 474, chinese ? "体重参考" : "WEIGHT GUIDE", 1,
                       GrayLevel::Black);
      canvas.draw_text(250, 502, ui_text(content.baby_weight_text), 3, GrayLevel::Black);
    } else {
      canvas.draw_text(24, 246,
                       chinese ? "身体负担正在逐渐增加"
                               : "YOUR BODY IS CARRYING MORE",
                       3, GrayLevel::Black);
      ui_draw_wrapped_text(canvas, 24, 292, 432,
                           chinese ? content.mother_changes_zh
                                   : content.mother_changes,
                           2, GrayLevel::Black, 5, 8);
      canvas.draw_line(24, 450, 456, 450, GrayLevel::Black);
      canvas.draw_text(24, 474, chinese ? "本周建议" : "SUGGESTION", 1,
                       GrayLevel::Black);
      ui_draw_wrapped_text(canvas, 24, 504, 432,
                           chinese ? content.daily_advice_zh
                                   : content.daily_advice,
                           2, GrayLevel::Black, 4, 8);
    }
    prototype_ui::draw_button(canvas, kPortrait.tabs[0], ui_text("OVERVIEW"), false, 2);
    prototype_ui::draw_button(canvas, kPortrait.tabs[1], ui_text("BABY"),
                page == PregnancyPage::Baby, 2);
    prototype_ui::draw_button(canvas, kPortrait.tabs[2], ui_text("MOM"),
                page == PregnancyPage::Mom, 2);
    return;
  }
  canvas.draw_text(24, 22, title, 4, GrayLevel::Black);
  canvas.draw_text(
      24, 57,
      chinese ? (page == PregnancyPage::Baby ? "本周发育变化"
                                             : "常见变化与温和提醒")
              : (page == PregnancyPage::Baby ? "THIS WEEK'S DEVELOPMENT"
                                             : "CHANGES AND GENTLE REMINDERS"),
      2, GrayLevel::Black);
  prototype_ui::draw_button(canvas, kLandscape.home, chinese ? "返回首页" : "HOME", false, 2);
  canvas.draw_line(214, 86, 214, 348, GrayLevel::Black);
  if (page == PregnancyPage::Baby) {
    canvas.draw_text(24, 108, chinese ? "孕周" : "WEEK", 2, GrayLevel::Black);
    char stat[20] = {};
    std::snprintf(stat, sizeof(stat), "%u+%u",
                  static_cast<unsigned>(progress.weeks),
                  static_cast<unsigned>(progress.days));
    canvas.draw_text(24, 142, stat, 7, GrayLevel::Black);
    canvas.draw_text(24, 210, ui_text(pregnancy_stage_name(progress.stage)), 2,
                     GrayLevel::Black);
    char remaining[48] = {};
    std::snprintf(remaining, sizeof(remaining),
                  chinese ? "距离预产期约 %d 天" : "ABOUT %d DAYS TO DUE DATE",
                  progress.days_until_due_date);
    canvas.draw_text(24, 240, remaining, 1, GrayLevel::Black);
    canvas.draw_text(238, 94,
                     chinese ? "宝宝正在感知外面的世界"
                             : "BABY IS DISCOVERING THE WORLD",
                     3, GrayLevel::Black);
    ui_draw_wrapped_text(canvas, 238, 136, 538,
                         chinese ? content.baby_development_zh
                                 : content.baby_development,
                         2, GrayLevel::Black, 4, 8);
    canvas.draw_line(238, 252, 410, 252, GrayLevel::Black);
    canvas.draw_line(426, 252, 598, 252, GrayLevel::Black);
    canvas.draw_line(614, 252, 776, 252, GrayLevel::Black);
    canvas.draw_text(238, 266, chinese ? "身长参考" : "SIZE GUIDE", 1,
                     GrayLevel::Black);
    canvas.draw_text(238, 287, ui_text(content.baby_size_text), 2, GrayLevel::Black);
    canvas.draw_text(426, 266, chinese ? "体重参考" : "WEIGHT GUIDE", 1,
                     GrayLevel::Black);
    canvas.draw_text(426, 287, ui_text(content.baby_weight_text), 2, GrayLevel::Black);
    canvas.draw_text(614, 266, chinese ? "本周关键词" : "KEYWORD", 1,
                     GrayLevel::Black);
    ui_draw_wrapped_text_pixels(canvas, 614, 287, 162,
                                chinese ? "感知 · 活动" : "SENSES · MOTION",
                                ui_tokens::kBody, GrayLevel::Black, 2, 2,
                                FontWeight::Medium);
  } else {
    canvas.draw_text(24, 108, chinese ? "本周重点" : "THIS WEEK", 2,
                     GrayLevel::Black);
    canvas.draw_text(24, 146, chinese ? "休息" : "REST", 5, GrayLevel::Black);
    canvas.draw_text(24, 194, chinese ? "补水" : "HYDRATE", 5,
                     GrayLevel::Black);
    ui_draw_wrapped_text(canvas, 24, 254, 166,
                         chinese ? "关注身体变化，按医护建议安排产检。"
                                 : "NOTICE CHANGES AND FOLLOW YOUR CARE PLAN.",
                         1, GrayLevel::Black, 4, 5);
    canvas.draw_text(238, 94,
                     chinese ? "身体负担正在逐渐增加"
                             : "YOUR BODY IS CARRYING MORE",
                     3, GrayLevel::Black);
    ui_draw_wrapped_text(canvas, 238, 136, 538,
                         chinese ? content.mother_changes_zh
                                 : content.mother_changes,
                         2, GrayLevel::Black, 4, 8);
    canvas.draw_line(238, 252, 410, 252, GrayLevel::Black);
    canvas.draw_line(426, 252, 598, 252, GrayLevel::Black);
    canvas.draw_line(614, 252, 776, 252, GrayLevel::Black);
    canvas.draw_text(238, 266, chinese ? "建议" : "SUGGESTION", 1,
                     GrayLevel::Black);
    canvas.draw_text(238, 287, chinese ? "分段活动" : "PACE ACTIVITY", 2,
                     GrayLevel::Black);
    canvas.draw_text(426, 266, chinese ? "记录" : "RECORD", 1,
                     GrayLevel::Black);
    canvas.draw_text(426, 287, chinese ? "体重趋势" : "WEIGHT TREND", 2,
                     GrayLevel::Black);
    canvas.draw_text(614, 266, chinese ? "如感不适" : "IF UNWELL", 1,
                     GrayLevel::Black);
    canvas.draw_text(614, 287, chinese ? "联系医护" : "CONTACT CARE", 2,
                     GrayLevel::Black);
  }
  prototype_ui::draw_button(canvas, kLandscape.tabs[0], ui_text("OVERVIEW"), false, 2);
  prototype_ui::draw_button(canvas, kLandscape.tabs[1], ui_text("BABY"), page == PregnancyPage::Baby,
              2);
  prototype_ui::draw_button(canvas, kLandscape.tabs[2], ui_text("MOM"), page == PregnancyPage::Mom, 2);
}

size_t pregnancy_page_targets(PregnancyPage page, bool can_cancel, int width,
                              int height, PregnancyTarget *out,
                              size_t capacity) {
  const PageLayout &layout = layout_for(height > width);
  size_t count = 0U;
  const auto add = [&](const Rect &rect, PregnancyAction action) {
    if (out != nullptr && count < capacity)
      out[count] = {rect, action};
    ++count;
  };
  if (page == PregnancyPage::SourceSetup) {
    add(layout.due_source, PregnancyAction::SelectDueDate);
    add(layout.lmp_source, PregnancyAction::SelectLmp);
    if (can_cancel)
      add(layout.back, PregnancyAction::Back);
  } else if (page == PregnancyPage::Dashboard) {
    add(layout.menu, PregnancyAction::OpenLauncher);
    add(layout.hero, PregnancyAction::ShowBaby);
    add(layout.numeral, PregnancyAction::ShowMom);
  } else if (page == PregnancyPage::Baby || page == PregnancyPage::Mom) {
    add(layout.home, PregnancyAction::ShowOverview);
    add(layout.tabs[0], PregnancyAction::ShowOverview);
    add(layout.tabs[1], PregnancyAction::ShowBaby);
    add(layout.tabs[2], PregnancyAction::ShowMom);
  } else {
    if (can_cancel)
      add(layout.back, PregnancyAction::Back);
    // The value fields (year, month, ...) select which part the keys edit.
    Rect cells[DigitFields::kMaxFields];
    const size_t fields = digit_field_cells(
        layout.input,
        page == PregnancyPage::ClockSetup ? DigitFieldKind::DateTime
                                          : DigitFieldKind::Date,
        layout.input_size, cells, DigitFields::kMaxFields);
    for (size_t index = 0U; index < fields; ++index)
      add(cells[index], static_cast<PregnancyAction>(
                            static_cast<int>(PregnancyAction::Field0) +
                            static_cast<int>(index)));
    for (size_t index = 0U; index < 12U; ++index)
      add(layout.keys[index], kKeyActions[index]);
  }
  return count < capacity ? count : capacity;
}

PregnancyAction pregnancy_page_action_at(PregnancyPage page, bool can_cancel,
                                         int width, int height, int x, int y) {
  if (x < 0 || y < 0 || x >= width || y >= height) {
    return PregnancyAction::None;
  }
  PregnancyTarget targets[kMaxPregnancyTargets];
  const size_t count = pregnancy_page_targets(
      page, can_cancel, width, height, targets, kMaxPregnancyTargets);
  for (size_t index = 0U; index < count; ++index) {
    if (targets[index].rect.contains(x, y))
      return targets[index].action;
  }
  return PregnancyAction::None;
}

PregnancyAction pregnancy_page_action_at(PregnancyPage page, bool can_cancel,
                                         int x, int y) {
  return pregnancy_page_action_at(page, can_cancel, kScreenWidth, kScreenHeight,
                                  x, y);
}

bool pregnancy_action_field(PregnancyAction action, size_t &field) {
  if (action < PregnancyAction::Field0 || action > PregnancyAction::Field4) {
    return false;
  }
  field = static_cast<size_t>(static_cast<int>(action) -
                              static_cast<int>(PregnancyAction::Field0));
  return true;
}

bool pregnancy_action_digit(PregnancyAction action, char &digit) {
  if (action < PregnancyAction::Digit0 || action > PregnancyAction::Digit9) {
    return false;
  }
  digit = static_cast<char>('0' + static_cast<int>(action) -
                            static_cast<int>(PregnancyAction::Digit0));
  return true;
}

const char *pregnancy_page_name(PregnancyPage page) {
  switch (page) {
  case PregnancyPage::ClockSetup:
    return "clock_setup";
  case PregnancyPage::SourceSetup:
    return "source_setup";
  case PregnancyPage::DateSetup:
    return "date_setup";
  case PregnancyPage::Dashboard:
    return "dashboard";
  case PregnancyPage::Baby:
    return "baby";
  case PregnancyPage::Mom:
    return "mom";
  }
  return "unknown";
}

const char *pregnancy_action_name(PregnancyAction action) {
  switch (action) {
  case PregnancyAction::None:
    return "none";
  case PregnancyAction::Digit0:
    return "digit_0";
  case PregnancyAction::Digit1:
    return "digit_1";
  case PregnancyAction::Digit2:
    return "digit_2";
  case PregnancyAction::Digit3:
    return "digit_3";
  case PregnancyAction::Digit4:
    return "digit_4";
  case PregnancyAction::Digit5:
    return "digit_5";
  case PregnancyAction::Digit6:
    return "digit_6";
  case PregnancyAction::Digit7:
    return "digit_7";
  case PregnancyAction::Digit8:
    return "digit_8";
  case PregnancyAction::Digit9:
    return "digit_9";
  case PregnancyAction::Delete:
    return "delete";
  case PregnancyAction::Continue:
    return "continue";
  case PregnancyAction::Back:
    return "back";
  case PregnancyAction::SelectDueDate:
    return "select_due_date";
  case PregnancyAction::SelectLmp:
    return "select_lmp";
  case PregnancyAction::ShowOverview:
    return "show_overview";
  case PregnancyAction::ShowBaby:
    return "show_baby";
  case PregnancyAction::ShowMom:
    return "show_mom";
  case PregnancyAction::OpenLauncher:
    return "open_launcher";
  case PregnancyAction::Field0:
    return "field_0";
  case PregnancyAction::Field1:
    return "field_1";
  case PregnancyAction::Field2:
    return "field_2";
  case PregnancyAction::Field3:
    return "field_3";
  case PregnancyAction::Field4:
    return "field_4";
  }
  return "unknown";
}
