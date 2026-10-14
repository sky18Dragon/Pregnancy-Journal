#include "pregnancy_pages.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "canvas.h"
#include "font.h"
#include "pixel_asset.h"
#include "pregnancy_week_assets.h"
#include "pregnancy_week_visual.h"
#include "prototype_ui.h"
#include "text_layout.h"
#include "ui_language.h"
#include "ui_tokens.h"

namespace {

constexpr int kScreenWidth = 800;
constexpr int kScreenHeight = 480;

struct Rect {
  int x;
  int y;
  int width;
  int height;

  bool contains(int point_x, int point_y) const {
    return point_x >= x && point_y >= y && point_x < x + width &&
           point_y < y + height;
  }
};

constexpr Rect kKeyRects[] = {
    {500, 70, 84, 66},  {594, 70, 84, 66},  {688, 70, 84, 66},
    {500, 146, 84, 66}, {594, 146, 84, 66}, {688, 146, 84, 66},
    {500, 222, 84, 66}, {594, 222, 84, 66}, {688, 222, 84, 66},
    {500, 298, 84, 66}, {594, 298, 84, 66}, {688, 298, 84, 66},
};

constexpr PregnancyAction kKeyActions[] = {
    PregnancyAction::Digit1, PregnancyAction::Digit2, PregnancyAction::Digit3,
    PregnancyAction::Digit4, PregnancyAction::Digit5, PregnancyAction::Digit6,
    PregnancyAction::Digit7, PregnancyAction::Digit8, PregnancyAction::Digit9,
    PregnancyAction::Delete, PregnancyAction::Digit0, PregnancyAction::Continue,
};

constexpr const char *kKeyLabels[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "DEL", "0", "NEXT",
};

constexpr Rect kBackRect = {30, 390, 180, 56};
constexpr Rect kEditRect = {656, 20, 120, ui_tokens::kMinimumTouchHeight};
constexpr Rect kDueSourceRect = {54, 150, 320, 170};
constexpr Rect kLmpSourceRect = {426, 150, 320, 170};
constexpr Rect kOverviewTab = {24, 408, 128, 48};
constexpr Rect kBabyTab = {152, 408, 128, 48};
constexpr Rect kMomTab = {280, 408, 128, 48};
constexpr Rect kHomeMenu = {736, 8, 56, 56};
// Dashboard "this week" rows. The same rectangles are drawn and hit-tested:
// baby development opens Baby; mother changes and daily advice open Mom.
constexpr Rect kHomeRows[3] = {
    {32, 284, 352, 56}, {32, 340, 352, 56}, {32, 396, 352, 56}};

constexpr Rect kPortraitKeyRects[] = {
    {24, 278, 132, 72}, {174, 278, 132, 72}, {324, 278, 132, 72},
    {24, 366, 132, 72}, {174, 366, 132, 72}, {324, 366, 132, 72},
    {24, 454, 132, 72}, {174, 454, 132, 72}, {324, 454, 132, 72},
    {24, 542, 132, 72}, {174, 542, 132, 72}, {324, 542, 132, 72},
};
constexpr Rect kPortraitBackRect = {24, 678, 432, 56};
constexpr Rect kPortraitDueSourceRect = {24, 150, 432, 190};
constexpr Rect kPortraitLmpSourceRect = {24, 370, 432, 190};
constexpr Rect kPortraitEditRect = {336, 20, 120,
                                    ui_tokens::kMinimumTouchHeight};
constexpr Rect kPortraitOverviewTab = {24, 728, 144, 48};
constexpr Rect kPortraitBabyTab = {168, 728, 144, 48};
constexpr Rect kPortraitMomTab = {312, 728, 144, 48};
constexpr Rect kPortraitHomeMenu = {416, 8, 56, 56};
constexpr Rect kPortraitHomeRows[3] = {
    {24, 628, 432, 48}, {24, 676, 432, 48}, {24, 724, 432, 48}};
constexpr PregnancyAction kHomeRowActions[3] = {
    PregnancyAction::ShowBaby, PregnancyAction::ShowMom,
    PregnancyAction::ShowMom};

bool portrait_layout(const Canvas &canvas) {
  return canvas.height() > canvas.width();
}

void draw_centered_text_in_rect(Canvas &canvas, const Rect &rect,
                                const char *text, int scale, GrayLevel color) {
  prototype_ui::draw_centered_label(
      canvas, {rect.x, rect.y, rect.width, rect.height}, text,
      font_ui_pixel_size(static_cast<uint8_t>(scale)), color,
      FontWeight::Medium);
}

void draw_beveled_rect(Canvas &canvas, const Rect &rect, GrayLevel color) {
  constexpr int kCut = 8;
  canvas.draw_line(rect.x + kCut, rect.y, rect.x + rect.width - kCut - 1,
                   rect.y, color);
  canvas.draw_line(rect.x + rect.width - kCut - 1, rect.y,
                   rect.x + rect.width - 1, rect.y + kCut, color);
  canvas.draw_line(rect.x + rect.width - 1, rect.y + kCut,
                   rect.x + rect.width - 1, rect.y + rect.height - kCut - 1,
                   color);
  canvas.draw_line(rect.x + rect.width - 1, rect.y + rect.height - kCut - 1,
                   rect.x + rect.width - kCut - 1, rect.y + rect.height - 1,
                   color);
  canvas.draw_line(rect.x + rect.width - kCut - 1, rect.y + rect.height - 1,
                   rect.x + kCut, rect.y + rect.height - 1, color);
  canvas.draw_line(rect.x + kCut, rect.y + rect.height - 1, rect.x,
                   rect.y + rect.height - kCut - 1, color);
  canvas.draw_line(rect.x, rect.y + rect.height - kCut - 1, rect.x,
                   rect.y + kCut, color);
  canvas.draw_line(rect.x, rect.y + kCut, rect.x + kCut, rect.y, color);
}

void draw_rounded_rect(Canvas &canvas, const Rect &rect, int radius,
                       GrayLevel color) {
  const int safe_radius =
      std::max(1, std::min(radius, std::min(rect.width, rect.height) / 2));
  const int left_center = rect.x + safe_radius;
  const int right_center = rect.x + rect.width - safe_radius - 1;
  const int top_center = rect.y + safe_radius;
  const int bottom_center = rect.y + rect.height - safe_radius - 1;

  canvas.draw_line(left_center, rect.y, right_center, rect.y, color);
  canvas.draw_line(left_center, rect.y + rect.height - 1, right_center,
                   rect.y + rect.height - 1, color);
  canvas.draw_line(rect.x, top_center, rect.x, bottom_center, color);
  canvas.draw_line(rect.x + rect.width - 1, top_center, rect.x + rect.width - 1,
                   bottom_center, color);

  int arc_x = safe_radius;
  int arc_y = 0;
  int error = 1 - safe_radius;
  while (arc_x >= arc_y) {
    canvas.draw_pixel(left_center - arc_x, top_center - arc_y, color);
    canvas.draw_pixel(left_center - arc_y, top_center - arc_x, color);
    canvas.draw_pixel(right_center + arc_x, top_center - arc_y, color);
    canvas.draw_pixel(right_center + arc_y, top_center - arc_x, color);
    canvas.draw_pixel(left_center - arc_x, bottom_center + arc_y, color);
    canvas.draw_pixel(left_center - arc_y, bottom_center + arc_x, color);
    canvas.draw_pixel(right_center + arc_x, bottom_center + arc_y, color);
    canvas.draw_pixel(right_center + arc_y, bottom_center + arc_x, color);
    ++arc_y;
    if (error < 0) {
      error += 2 * arc_y + 1;
    } else {
      --arc_x;
      error += 2 * (arc_y - arc_x + 1);
    }
  }
}

void fill_pill(Canvas &canvas, int x, int y, int width, int height,
               GrayLevel color) {
  if (width <= 0 || height <= 0)
    return;
  const int radius = height / 2;
  if (width <= height) {
    canvas.fill_circle(x + width / 2, y + height / 2, std::max(1, width / 2),
                       color);
    return;
  }
  canvas.fill_rect(x + radius, y, width - radius * 2, height, color);
  canvas.fill_circle(x + radius, y + height / 2, radius, color);
  canvas.fill_circle(x + width - radius - 1, y + height / 2, radius, color);
}

void draw_baby_icon(Canvas &canvas, int x, int y, GrayLevel color) {
  canvas.draw_circle(x + 12, y + 12, 10, color);
  canvas.draw_circle(x + 12, y + 12, 9, color);
  canvas.fill_circle(x + 8, y + 10, 1, color);
  canvas.fill_circle(x + 16, y + 10, 1, color);
  canvas.draw_line(x + 8, y + 16, x + 11, y + 18, color);
  canvas.draw_line(x + 11, y + 18, x + 15, y + 15, color);
  canvas.draw_line(x + 10, y + 2, x + 12, y, color);
  canvas.draw_line(x + 12, y, x + 15, y + 2, color);
}

void draw_ruler_icon(Canvas &canvas, int x, int y, GrayLevel color) {
  canvas.draw_rect(x + 1, y + 6, 24, 12, color);
  canvas.draw_rect(x + 2, y + 7, 22, 10, color);
  for (int tick = 0; tick < 4; ++tick) {
    const int tick_x = x + 5 + tick * 5;
    canvas.draw_line(tick_x, y + 6, tick_x, y + (tick % 2 == 0 ? 12 : 10),
                     color);
  }
}

void draw_heart_icon(Canvas &canvas, int x, int y, GrayLevel color) {
  canvas.draw_line(x + 12, y + 22, x + 2, y + 10, color);
  canvas.draw_line(x + 2, y + 10, x + 2, y + 6, color);
  canvas.draw_line(x + 2, y + 6, x + 6, y + 2, color);
  canvas.draw_line(x + 6, y + 2, x + 10, y + 2, color);
  canvas.draw_line(x + 10, y + 2, x + 12, y + 5, color);
  canvas.draw_line(x + 12, y + 5, x + 14, y + 2, color);
  canvas.draw_line(x + 14, y + 2, x + 18, y + 2, color);
  canvas.draw_line(x + 18, y + 2, x + 22, y + 6, color);
  canvas.draw_line(x + 22, y + 6, x + 22, y + 10, color);
  canvas.draw_line(x + 22, y + 10, x + 12, y + 22, color);
  canvas.draw_line(x + 12, y + 21, x + 3, y + 10, color);
  canvas.draw_line(x + 3, y + 10, x + 3, y + 6, color);
  canvas.draw_line(x + 3, y + 6, x + 7, y + 3, color);
  canvas.draw_line(x + 7, y + 3, x + 10, y + 3, color);
  canvas.draw_line(x + 10, y + 3, x + 12, y + 6, color);
  canvas.draw_line(x + 12, y + 6, x + 14, y + 3, color);
  canvas.draw_line(x + 14, y + 3, x + 18, y + 3, color);
  canvas.draw_line(x + 18, y + 3, x + 21, y + 6, color);
  canvas.draw_line(x + 21, y + 6, x + 21, y + 10, color);
  canvas.draw_line(x + 21, y + 10, x + 12, y + 21, color);
}

void draw_reminder_icon(Canvas &canvas, int x, int y, GrayLevel color) {
  canvas.draw_circle(x + 12, y + 12, 10, color);
  canvas.draw_circle(x + 12, y + 12, 9, color);
  canvas.draw_line(x + 12, y + 6, x + 12, y + 14, color);
  canvas.fill_circle(x + 12, y + 18, 1, color);
}

void draw_centered_text_pixels(Canvas &canvas, const Rect &rect,
                               const char *text, uint8_t pixel_size,
                               GrayLevel color, FontWeight weight) {
  const int width = font_text_width_pixels(text, pixel_size, weight);
  canvas.draw_text_pixels(rect.x + (rect.width - width) / 2, rect.y, text,
                          pixel_size, color, weight);
}

void draw_button(Canvas &canvas, const Rect &rect, const char *label,
                 bool filled, int scale) {
  const GrayLevel foreground = filled ? GrayLevel::White : GrayLevel::Black;
  if (filled) {
    canvas.fill_rect(rect.x, rect.y, rect.width, rect.height, GrayLevel::Black);
  } else {
    draw_beveled_rect(canvas, rect, GrayLevel::Black);
  }
  draw_centered_text_in_rect(canvas, rect, label, scale, foreground);
}

// 56 px square menu button: the drawn box is exactly the hit rectangle.
void draw_menu_button(Canvas &canvas, const Rect &rect) {
  canvas.fill_rect(rect.x, rect.y, rect.width, rect.height, GrayLevel::White);
  for (int inset = 0; inset < 2; ++inset)
    canvas.draw_rect(rect.x + inset, rect.y + inset, rect.width - 2 * inset,
                     rect.height - 2 * inset, GrayLevel::Black);
  for (int line = 0; line < 3; ++line)
    canvas.fill_rect(rect.x + 16, rect.y + 16 + line * 10, 24, 2,
                     GrayLevel::Black);
}

// One tappable "this week" row: icon, copy, trailing chevron. `divider`
// draws the 1 px rule between rows.
void draw_home_row(Canvas &canvas, const Rect &row,
                   void (*icon)(Canvas &, int, int, GrayLevel),
                   const char *text, bool portrait, bool divider) {
  if (divider)
    canvas.draw_line(row.x + 20, row.y, row.x + row.width - 20, row.y,
                     GrayLevel::Black);
  icon(canvas, row.x + 20, row.y + 12, GrayLevel::Black);
  // Body copy: 18 px, at most two lines (every week fits; see widths in tests).
  if (portrait) {
    ui_draw_wrapped_text_pixels(canvas, row.x + 58, row.y + 4, 326, text,
                                ui_tokens::kBody, GrayLevel::Black, 2, 4,
                                FontWeight::Regular);
  } else {
    ui_draw_wrapped_text_pixels(canvas, row.x + 60, row.y + 7, 272, text,
                                ui_tokens::kBody, GrayLevel::Black, 2, 6,
                                FontWeight::Regular);
  }
  prototype_ui::draw_chevron(canvas, row.x + row.width - (portrait ? 28 : 18),
                             row.y + (portrait ? 16 : 20));
}

void format_clock_digits(const char *digits, char *output, size_t size) {
  char padded[13] = "____________";
  if (digits != nullptr) {
    const size_t count = std::strlen(digits) < 12U ? std::strlen(digits) : 12U;
    std::memcpy(padded, digits, count);
  }
  std::snprintf(output, size, "%.4s-%.2s-%.2s  %.2s:%.2s", padded, padded + 4,
                padded + 6, padded + 8, padded + 10);
}

void format_date_digits(const char *digits, char *output, size_t size) {
  char padded[9] = "________";
  if (digits != nullptr) {
    const size_t count = std::strlen(digits) < 8U ? std::strlen(digits) : 8U;
    std::memcpy(padded, digits, count);
  }
  std::snprintf(output, size, "%.4s-%.2s-%.2s", padded, padded + 4, padded + 6);
}

void render_keypad(Canvas &canvas, bool portrait) {
  for (size_t index = 0U; index < 12U; ++index) {
    const bool primary = index == 11U;
    const int scale = index == 9U || index == 11U ? 2 : 4;
    draw_button(canvas, portrait ? kPortraitKeyRects[index] : kKeyRects[index],
                kKeyLabels[index], primary, scale);
  }
}

void render_setup(Canvas &canvas, const char *title, const char *caption,
                  const char *value, bool input_error, bool can_cancel) {
  canvas.clear(GrayLevel::White);
  const bool portrait = portrait_layout(canvas);
  if (portrait) {
    canvas.draw_text(24, 24, title, 4, GrayLevel::Black);
    canvas.draw_text(24, 66, caption, 1, GrayLevel::Black);
    constexpr Rect kPortraitInputRect = {24, 102, 432, 76};
    draw_beveled_rect(canvas, kPortraitInputRect, GrayLevel::Black);
    canvas.draw_text(kPortraitInputRect.x + 18, kPortraitInputRect.y + 24,
                     value, 3, GrayLevel::Black);
    canvas.draw_text(24, 204,
                     input_error ? "CHECK THE DATE AND TRY AGAIN"
                                 : "TAP A NUMBER TO REPLACE THE VALUE",
                     2, GrayLevel::Black);
    canvas.draw_text(24, 242, "SAVED ONLY ON THIS DEVICE", 1, GrayLevel::Black);
    if (can_cancel)
      draw_button(canvas, kPortraitBackRect, "CANCEL", false, 3);
    render_keypad(canvas, true);
    return;
  }
  canvas.draw_text(30, 28, title, 4, GrayLevel::Black);
  canvas.draw_text(30, 72, caption, 2, GrayLevel::Black);

  constexpr Rect kInputRect = {30, 118, 430, 86};
  draw_beveled_rect(canvas, kInputRect, GrayLevel::Black);
  canvas.draw_text(kInputRect.x + 20, kInputRect.y + 27, value, 4,
                   GrayLevel::Black);

  canvas.draw_text(30, 235,
                   input_error ? "CHECK THE DATE AND TRY AGAIN"
                               : "TAP A NUMBER TO REPLACE THE VALUE",
                   2, GrayLevel::Black);
  canvas.draw_text(30, 278, "SAVED ONLY ON THIS DEVICE", 2, GrayLevel::Black);

  if (can_cancel) {
    draw_button(canvas, kBackRect, "CANCEL", false, 3);
  }
  render_keypad(canvas, false);
}

} // namespace

void pregnancy_page_render_clock_setup(Canvas &canvas, const char *digits,
                                       bool input_error, bool can_cancel) {
  char value[24] = {};
  format_clock_digits(digits, value, sizeof(value));
  render_setup(canvas, "SET DEVICE TIME", "YEAR / MONTH / DAY / HOUR / MINUTE",
               value, input_error, can_cancel);
}

void pregnancy_page_render_due_date_setup(Canvas &canvas, const char *digits,
                                          bool input_error, bool can_cancel) {
  char value[16] = {};
  format_date_digits(digits, value, sizeof(value));
  render_setup(canvas, "SET DUE DATE", "USE YOUR CONFIRMED ESTIMATED DUE DATE",
               value, input_error, can_cancel);
}

void pregnancy_page_render_source_setup(Canvas &canvas, bool can_cancel) {
  canvas.clear(GrayLevel::White);
  if (portrait_layout(canvas)) {
    canvas.draw_text(24, 26, ui_text("PREGNANCY SETUP"), 4, GrayLevel::Black);
    canvas.draw_text(24, 72, ui_text("CHOOSE THE DATE YOU KNOW"), 2,
                     GrayLevel::Black);
    draw_button(canvas, kPortraitDueSourceRect, ui_text("ESTIMATED DUE DATE"),
                true, 3);
    draw_button(canvas, kPortraitLmpSourceRect,
                ui_text("LAST MENSTRUAL PERIOD"), false, 3);
    ui_draw_wrapped_text_pixels(
        canvas, 24, 594, 432,
        ui_text("A CLINICIAN-CONFIRMED DUE DATE IS PREFERRED"),
        ui_tokens::kSecondaryBody, GrayLevel::Black, 2, 2, FontWeight::Regular);
    if (can_cancel)
      draw_button(canvas, kPortraitBackRect, "CANCEL", false, 3);
    return;
  }
  canvas.draw_text(42, 28, ui_text("PREGNANCY SETUP"), 5, GrayLevel::Black);
  canvas.draw_text(42, 86, ui_text("CHOOSE THE DATE YOU KNOW"), 2,
                   GrayLevel::Black);
  draw_button(canvas, kDueSourceRect, ui_text("ESTIMATED DUE DATE"), true, 3);
  draw_button(canvas, kLmpSourceRect, ui_text("LAST MENSTRUAL PERIOD"), false,
              3);
  canvas.draw_text(54, 350,
                   ui_text("A CLINICIAN-CONFIRMED DUE DATE IS PREFERRED"), 2,
                   GrayLevel::Black);
  if (can_cancel)
    draw_button(canvas, kBackRect, "CANCEL", false, 3);
}

void pregnancy_page_render_profile_date_setup(Canvas &canvas,
                                              const char *digits,
                                              bool input_error,
                                              bool due_date_primary) {
  char value[16] = {};
  format_date_digits(digits, value, sizeof(value));
  render_setup(canvas,
               due_date_primary ? ui_text("SET DUE DATE")
                                : ui_text("SET LAST MENSTRUAL PERIOD"),
               due_date_primary
                   ? ui_text("USE YOUR CONFIRMED ESTIMATED DUE DATE")
                   : ui_text("USE THE FIRST DAY OF YOUR LAST PERIOD"),
               value, input_error, true);
}

void pregnancy_page_render_dashboard(Canvas &canvas,
                                     const PregnancyDate &due_date,
                                     const PregnancyProgress &progress) {
  (void)due_date;
  prototype_ui::begin_page(canvas);
  const PregnancyWeekVisual &visual = pregnancy_week_visual(progress.weeks);
  const WeekContent content = pregnancy_content_for_week(progress.weeks);

  if (portrait_layout(canvas)) {
    char headline[32] = {};
    std::snprintf(headline, sizeof(headline), "孕 %u 周",
                  static_cast<unsigned>(progress.weeks));
    draw_centered_text_pixels(canvas, {24, 26, 432, 56}, headline, 52,
                              GrayLevel::Black, FontWeight::Medium);

    char week_progress[32] = {};
    std::snprintf(week_progress, sizeof(week_progress), "%u / 40 周",
                  static_cast<unsigned>(progress.weeks));
    draw_centered_text_pixels(canvas, {24, 96, 432, 24}, week_progress, 20,
                              GrayLevel::Black, FontWeight::Regular);

    constexpr int kProgressX = 40;
    constexpr int kProgressY = 140;
    constexpr int kProgressWidth = 336;
    constexpr int kProgressHeight = 14;
    const int percent =
        std::max(0, std::min(100, static_cast<int>(progress.percent)));
    fill_pill(canvas, kProgressX, kProgressY, kProgressWidth * percent / 100,
              kProgressHeight, GrayLevel::Black);
    draw_rounded_rect(canvas,
                      {kProgressX, kProgressY, kProgressWidth, kProgressHeight},
                      kProgressHeight / 2, GrayLevel::Black);
    char completion[16] = {};
    std::snprintf(completion, sizeof(completion), "%u%%",
                  static_cast<unsigned>(progress.percent));
    canvas.draw_text_pixels(392, 136, completion, 18, GrayLevel::Black,
                            FontWeight::Regular);

    char countdown[64] = {};
    if (progress.overdue) {
      std::snprintf(countdown, sizeof(countdown), "已超过预产期 %d 天",
                    -progress.days_until_due_date);
    } else {
      std::snprintf(countdown, sizeof(countdown), "距离预产期还有 %d 天",
                    progress.days_until_due_date);
    }
    constexpr int kCountdownHeartGap = 8;
    constexpr int kCountdownHeartWidth = 24;
    const int countdown_width =
        font_text_width_pixels(countdown, 18, FontWeight::Regular);
    const int countdown_group_width =
        countdown_width + kCountdownHeartGap + kCountdownHeartWidth;
    const int countdown_x = (canvas.width() - countdown_group_width) / 2;
    canvas.draw_text_pixels(countdown_x, 174, countdown, 18, GrayLevel::Black,
                            FontWeight::Regular);
    draw_heart_icon(canvas, countdown_x + countdown_width + kCountdownHeartGap,
                    171, GrayLevel::Black);

    pixel_asset_draw_centered(canvas, 240, 358,
                              pregnancy_week_size_asset(progress.weeks), 2,
                              GrayLevel::Black);

    char size_title[80] = {};
    std::snprintf(size_title, sizeof(size_title), "宝宝现在约有%s那么大",
                  visual.object_zh);
    const uint8_t title_pixels =
        font_text_width_pixels(size_title, 24, FontWeight::Medium) <= 432 ? 24
                                                                          : 20;
    draw_centered_text_pixels(canvas, {24, 516, 432, 28}, size_title,
                              title_pixels, GrayLevel::Black,
                              FontWeight::Medium);
    draw_centered_text_pixels(canvas, {24, 548, 432, 20},
                              "—  用熟悉的东西感受宝宝正在长大  —", 16,
                              GrayLevel::Black, FontWeight::Regular);

    constexpr Rect kGrowthCard = {24, 574, 432, 200};
    draw_rounded_rect(canvas, kGrowthCard, 12, GrayLevel::Black);
    draw_baby_icon(canvas, 44, 588, GrayLevel::Black);
    canvas.draw_text_pixels(82, 588, "本周成长", 22, GrayLevel::Black);
    canvas.draw_line(44, 624, 436, 624, GrayLevel::Black);
    draw_home_row(canvas, kPortraitHomeRows[0], draw_ruler_icon,
                  content.baby_development_zh, true, false);
    draw_home_row(canvas, kPortraitHomeRows[1], draw_heart_icon,
                  content.mother_changes_zh, true, true);
    draw_home_row(canvas, kPortraitHomeRows[2], draw_reminder_icon,
                  content.daily_advice_zh, true, true);

    draw_menu_button(canvas, kPortraitHomeMenu);
    return;
  }

  char headline[32] = {};
  std::snprintf(headline, sizeof(headline), "孕 %u 周",
                static_cast<unsigned>(progress.weeks));
  canvas.draw_text_pixels(32, 24, headline, 56, GrayLevel::Black);

  char week_progress[32] = {};
  std::snprintf(week_progress, sizeof(week_progress), "%u / 40 周",
                static_cast<unsigned>(progress.weeks));
  canvas.draw_text_pixels(32, 96, week_progress, 20, GrayLevel::Black,
                          FontWeight::Regular);

  constexpr int kProgressX = 32;
  constexpr int kProgressY = 136;
  constexpr int kProgressWidth = 272;
  constexpr int kProgressHeight = 14;
  const int percent =
      std::max(0, std::min(100, static_cast<int>(progress.percent)));
  const int filled_width = kProgressWidth * percent / 100;
  fill_pill(canvas, kProgressX, kProgressY, filled_width, kProgressHeight,
            GrayLevel::Black);
  draw_rounded_rect(canvas,
                    {kProgressX, kProgressY, kProgressWidth, kProgressHeight},
                    kProgressHeight / 2, GrayLevel::Black);
  char completion[16] = {};
  std::snprintf(completion, sizeof(completion), "%u%%",
                static_cast<unsigned>(progress.percent));
  canvas.draw_text_pixels(320, 132, completion, 20, GrayLevel::Black,
                          FontWeight::Regular);

  char countdown[64] = {};
  if (progress.overdue) {
    std::snprintf(countdown, sizeof(countdown), "已超过预产期 %d 天",
                  -progress.days_until_due_date);
  } else {
    std::snprintf(countdown, sizeof(countdown), "距离预产期还有 %d 天",
                  progress.days_until_due_date);
  }
  canvas.draw_text_pixels(32, 168, countdown, 18, GrayLevel::Black,
                          FontWeight::Regular);
  draw_heart_icon(
      canvas, 40 + font_text_width_pixels(countdown, 18, FontWeight::Regular),
      165, GrayLevel::Black);

  constexpr Rect kGrowthCard = {32, 216, 352, 240};
  draw_rounded_rect(canvas, kGrowthCard, 12, GrayLevel::Black);
  draw_baby_icon(canvas, 52, 232, GrayLevel::Black);
  canvas.draw_text_pixels(92, 232, "本周成长", 24, GrayLevel::Black);
  canvas.draw_line(52, 276, 364, 276, GrayLevel::Black);

  draw_home_row(canvas, kHomeRows[0], draw_ruler_icon,
                content.baby_development_zh, false, false);
  draw_home_row(canvas, kHomeRows[1], draw_heart_icon,
                content.mother_changes_zh, false, true);
  draw_home_row(canvas, kHomeRows[2], draw_reminder_icon,
                content.daily_advice_zh, false, true);

  pixel_asset_draw_centered(canvas, 592, 204,
                            pregnancy_week_size_asset(progress.weeks), 2,
                            GrayLevel::Black);

  char size_title[80] = {};
  std::snprintf(size_title, sizeof(size_title), "宝宝现在约有%s那么大",
                visual.object_zh);
  const uint8_t size_title_pixels =
      font_text_width_pixels(size_title, 24, FontWeight::Medium) <= 368 ? 24
                                                                        : 20;
  draw_centered_text_pixels(canvas, {408, 360, 368, 24}, size_title,
                            size_title_pixels, GrayLevel::Black,
                            FontWeight::Medium);
  draw_centered_text_pixels(canvas, {408, 408, 368, 20},
                            "—  用熟悉的东西感受宝宝正在长大  —", 16,
                            GrayLevel::Black, FontWeight::Regular);

  draw_menu_button(canvas, kHomeMenu);
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
    draw_button(canvas, kPortraitEditRect, chinese ? "返回" : "HOME", false, 2);
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
      canvas.draw_text(24, 502, content.baby_size_text, 3, GrayLevel::Black);
      canvas.draw_text(250, 474, chinese ? "体重参考" : "WEIGHT GUIDE", 1,
                       GrayLevel::Black);
      canvas.draw_text(250, 502, content.baby_weight_text, 3, GrayLevel::Black);
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
    draw_button(canvas, kPortraitOverviewTab, ui_text("OVERVIEW"), false, 2);
    draw_button(canvas, kPortraitBabyTab, ui_text("BABY"),
                page == PregnancyPage::Baby, 2);
    draw_button(canvas, kPortraitMomTab, ui_text("MOM"),
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
  draw_button(canvas, kEditRect, chinese ? "返回首页" : "HOME", false, 2);
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
    canvas.draw_text(238, 287, content.baby_size_text, 2, GrayLevel::Black);
    canvas.draw_text(426, 266, chinese ? "体重参考" : "WEIGHT GUIDE", 1,
                     GrayLevel::Black);
    canvas.draw_text(426, 287, content.baby_weight_text, 2, GrayLevel::Black);
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
  draw_button(canvas, kOverviewTab, ui_text("OVERVIEW"), false, 2);
  draw_button(canvas, kBabyTab, ui_text("BABY"), page == PregnancyPage::Baby,
              2);
  draw_button(canvas, kMomTab, ui_text("MOM"), page == PregnancyPage::Mom, 2);
}

PregnancyAction pregnancy_page_action_at(PregnancyPage page, bool can_cancel,
                                         int width, int height, int x, int y) {
  if (x < 0 || y < 0 || x >= width || y >= height) {
    return PregnancyAction::None;
  }
  const bool portrait = height > width;
  if (page == PregnancyPage::SourceSetup) {
    if ((portrait ? kPortraitDueSourceRect : kDueSourceRect).contains(x, y))
      return PregnancyAction::SelectDueDate;
    if ((portrait ? kPortraitLmpSourceRect : kLmpSourceRect).contains(x, y))
      return PregnancyAction::SelectLmp;
    return can_cancel &&
                   (portrait ? kPortraitBackRect : kBackRect).contains(x, y)
               ? PregnancyAction::Back
               : PregnancyAction::None;
  }
  if (page == PregnancyPage::Dashboard || page == PregnancyPage::Baby ||
      page == PregnancyPage::Mom) {
    if (page == PregnancyPage::Dashboard) {
      if ((portrait ? kPortraitHomeMenu : kHomeMenu).contains(x, y))
        return PregnancyAction::OpenLauncher;
      for (size_t row = 0U; row < 3U; ++row) {
        if ((portrait ? kPortraitHomeRows[row] : kHomeRows[row]).contains(x, y))
          return kHomeRowActions[row];
      }
      return PregnancyAction::None;
    }
    if ((portrait ? kPortraitEditRect : kEditRect).contains(x, y))
      return PregnancyAction::ShowOverview;
    if ((portrait ? kPortraitOverviewTab : kOverviewTab).contains(x, y))
      return PregnancyAction::ShowOverview;
    if ((portrait ? kPortraitBabyTab : kBabyTab).contains(x, y))
      return PregnancyAction::ShowBaby;
    if ((portrait ? kPortraitMomTab : kMomTab).contains(x, y))
      return PregnancyAction::ShowMom;
    return PregnancyAction::None;
  }
  if (can_cancel && (portrait ? kPortraitBackRect : kBackRect).contains(x, y)) {
    return PregnancyAction::Back;
  }
  for (size_t index = 0U; index < 12U; ++index) {
    if ((portrait ? kPortraitKeyRects[index] : kKeyRects[index])
            .contains(x, y)) {
      return kKeyActions[index];
    }
  }
  return PregnancyAction::None;
}

PregnancyAction pregnancy_page_action_at(PregnancyPage page, bool can_cancel,
                                         int x, int y) {
  return pregnancy_page_action_at(page, can_cancel, kScreenWidth, kScreenHeight,
                                  x, y);
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
  case PregnancyAction::Edit:
    return "edit";
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
  }
  return "unknown";
}
