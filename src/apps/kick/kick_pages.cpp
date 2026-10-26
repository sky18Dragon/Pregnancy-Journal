#include "kick_pages.h"

#include <algorithm>
#include <cstdio>

#include "canvas.h"
#include "font.h"
#include "prototype_ui.h"
#include "text_layout.h"
#include "ui_language.h"
#include "ui_tokens.h"

namespace {

using prototype_ui::Rect;

bool portrait(int width, int height) { return height > width; }

Rect primary_button_rect(int width, int height) {
  return portrait(width, height) ? Rect{24, 720, 208, ui_tokens::kButtonHeight}
                                 : Rect{24, 400, 368, ui_tokens::kButtonHeight};
}

Rect secondary_button_rect(int width, int height) {
  return portrait(width, height) ? Rect{248, 720, 208, ui_tokens::kButtonHeight}
                                 : Rect{408, 400, 368, ui_tokens::kButtonHeight};
}

// Today's method row: the current method on the left, a CHANGE button on the
// right. The button is the hit target and is drawn by the same rectangle.
Rect method_change_rect(int width, int height) {
  return portrait(width, height)
             ? Rect{336, 656, 120, ui_tokens::kMinimumTouchHeight}
             : Rect{256, 340, 120, ui_tokens::kMinimumTouchHeight};
}

Rect full_button_rect(int width, int height) {
  return portrait(width, height) ? Rect{24, 720, 432, ui_tokens::kButtonHeight}
                                 : Rect{24, 400, 752, ui_tokens::kButtonHeight};
}

Rect mode_option_rect(int width, int height, size_t index) {
  return portrait(width, height)
             ? Rect{24, 112 + static_cast<int>(index) * 176, 432, 156}
             : Rect{24, 96 + static_cast<int>(index) * 100, 752, 92};
}

Rect feel_option_rect(int width, int height, size_t index) {
  if (portrait(width, height))
    return Rect{24, 112 + static_cast<int>(index) * 148, 432, 128};
  return Rect{index % 2U == 0U ? 24 : 408,
              index < 2U ? 104 : 256, 368, 136};
}

constexpr KickMode kModeOptions[] = {KickMode::Timed, KickMode::CountToTen,
                                     KickMode::Awareness};
constexpr KickFeel kFeelOptions[] = {KickFeel::Usual, KickFeel::Weaker,
                                     KickFeel::Fewer, KickFeel::Changed};

// The counter circle as its bounding square: drawn as a circle, hit-tested as
// a circle (KickTarget::circle), and used for press feedback as the square.
Rect counter_circle(int width, int height) {
  const bool is_portrait = portrait(width, height);
  const int radius = is_portrait ? 128 : 112;
  const int center_x = width / 2; // centred on the screen in both orientations
  const int center_y = is_portrait ? 332 : 234;
  return {center_x - radius, center_y - radius, 2 * radius, 2 * radius};
}

void draw_centered(Canvas &canvas, int center_x, int y, const char *text,
                   uint8_t pixels, FontWeight weight = FontWeight::Regular) {
  const int width = font_text_width_pixels(text, pixels, weight);
  canvas.draw_text_pixels(center_x - width / 2, y, text, pixels,
                          GrayLevel::Black, weight);
}


const char *period_label(KickPeriod period) {
  if (period == KickPeriod::Morning)
    return ui_text("MORNING");
  if (period == KickPeriod::Afternoon)
    return ui_text("AFTERNOON");
  return ui_text("EVENING");
}

void draw_period_values(Canvas &canvas, int x, int y, int column_width,
                        const KickDaySummary &summary, bool compact,
                        uint8_t label_size = ui_tokens::reading_size()) {
  const uint16_t values[] = {summary.morning, summary.afternoon,
                             summary.evening};
  const KickPeriod periods[] = {KickPeriod::Morning, KickPeriod::Afternoon,
                                KickPeriod::Evening};
  for (size_t index = 0U; index < 3U; ++index) {
    const int left = x + static_cast<int>(index) * column_width;
    canvas.draw_text_pixels(left, y, period_label(periods[index]), label_size,
                            GrayLevel::Black, FontWeight::Regular);
    char value[16] = {};
    std::snprintf(value, sizeof(value), "%u",
                  static_cast<unsigned>(values[index]));
    canvas.draw_text_pixels(left, y + 28, value, compact ? ui_tokens::kSectionHeading : ui_tokens::kPageHeading,
                            GrayLevel::Black, FontWeight::Medium);
  }
}

// Landscape insights: one row per period, label left and value right-aligned.
// Long English period names do not fit three columns beside the chart.
void draw_period_rows(Canvas &canvas, int x, int y, int width,
                      const KickDaySummary &summary) {
  const uint16_t values[] = {summary.morning, summary.afternoon,
                             summary.evening};
  const KickPeriod periods[] = {KickPeriod::Morning, KickPeriod::Afternoon,
                                KickPeriod::Evening};
  for (size_t index = 0U; index < 3U; ++index) {
    const int row_y = y + static_cast<int>(index) * 36;
    canvas.draw_text_pixels(x, row_y + 6, period_label(periods[index]), ui_tokens::reading_size(),
                            GrayLevel::Black, FontWeight::Regular);
    char value[16] = {};
    std::snprintf(value, sizeof(value), "%u",
                  static_cast<unsigned>(values[index]));
    const int value_width =
        font_text_width_pixels(value, ui_tokens::kSectionHeading, FontWeight::Medium);
    canvas.draw_text_pixels(x + width - value_width, row_y, value, ui_tokens::kSectionHeading,
                            GrayLevel::Black, FontWeight::Medium);
  }
}

void draw_bar_chart(Canvas &canvas, const Rect &rect,
                    const KickTrendSummary &trend) {
  canvas.draw_line(rect.x, rect.y + rect.height - 30,
                   rect.x + rect.width - 1, rect.y + rect.height - 30,
                   GrayLevel::Black);
  uint16_t maximum = 1U;
  for (uint16_t value : trend.totals)
    maximum = std::max(maximum, value);

  const int slot = rect.width / static_cast<int>(kKickTrendDays);
  const int bar_width = std::max(16, std::min(32, slot - 12));
  const int available_height = rect.height - 74;
  for (size_t index = 0U; index < kKickTrendDays; ++index) {
    const int center = rect.x + static_cast<int>(index) * slot + slot / 2;
    const uint16_t value = trend.totals[index];
    const int bar_height = value == 0U
                               ? 0
                               : std::max(2, available_height * value /
                                                 static_cast<int>(maximum));
    if (bar_height > 0) {
      canvas.fill_rect(center - bar_width / 2,
                       rect.y + available_height - bar_height, bar_width,
                       bar_height, GrayLevel::Black);
      char count[12] = {};
      std::snprintf(count, sizeof(count), "%u",
                    static_cast<unsigned>(value));
      draw_centered(canvas, center,
                    rect.y + available_height - bar_height - 20, count, ui_tokens::reading_size(),
                    FontWeight::Regular);
    }
    char day[12] = {};
    if (index + 1U == kKickTrendDays) {
      std::snprintf(day, sizeof(day), "%s", ui_text("TODAY"));
    } else {
      std::snprintf(day, sizeof(day), "%u/%u",
                    static_cast<unsigned>(trend.dates[index].month),
                    static_cast<unsigned>(trend.dates[index].day));
    }
    draw_centered(canvas, center, rect.y + rect.height - 22, day, ui_tokens::reading_size(),
                  index + 1U == kKickTrendDays ? FontWeight::Medium
                                              : FontWeight::Regular);
  }
}

void draw_option(Canvas &canvas, const Rect &rect, const char *title,
                 const char *description, bool compact, bool selected) {
  if (selected)
    canvas.fill_rect(rect.x, rect.y, rect.width, rect.height, GrayLevel::Black);
  prototype_ui::draw_frame(canvas, rect, ui_tokens::kBorderStroke);
  const GrayLevel ink = selected ? GrayLevel::White : GrayLevel::Black;
  canvas.draw_text_pixels(rect.x + 20, rect.y + 14, ui_text(title), ui_tokens::kEmphasis,
                          ink, FontWeight::Medium);
  ui_draw_wrapped_text_pixels(canvas, rect.x + 20, rect.y + 46,
                              rect.width - 40, ui_text(description),
                              ui_tokens::reading_size(), ink,
                              compact ? 2 : 4, 4, FontWeight::Regular);
}

const char *mode_description(KickMode mode) {
  if (mode == KickMode::Timed)
    return "UP TO ONE HOUR · MOVEMENTS PER HOUR VS THE SAME PERIOD IN THE LAST 7 DAYS";
  if (mode == KickMode::CountToTen)
    return "STOPS AT TEN · TIME TAKEN VS THE LAST 7 DAYS";
  return "NO COUNTING · HOW TODAY'S MOVEMENTS FELT";
}

void draw_comparison_block(Canvas &canvas, int x, int y, int width,
                           const KickComparison &comparison, bool compact) {
  KickComparisonText text = {};
  kick_format_comparison(comparison, text);
  int line = y;
  canvas.draw_text_pixels(x, line, text.value, compact ? ui_tokens::kSectionHeading : ui_tokens::kPageHeading,
                          GrayLevel::Black, FontWeight::Medium);
  line += compact ? 44 : 52;
  if (text.baseline[0] != '\0') {
    line += 2 + ui_draw_wrapped_text_pixels(canvas, x, line, width,
                                            text.baseline, ui_tokens::reading_size(),
                                            GrayLevel::Black, 2, 6,
                                            FontWeight::Regular) *
                    26;
  }
  if (text.difference[0] != '\0') {
    ui_draw_wrapped_text_pixels(canvas, x, line + 8, width, text.difference,
                                compact ? ui_tokens::kEmphasis : ui_tokens::kCardTitle, GrayLevel::Black, 2, 6,
                                FontWeight::Medium);
  }
}

// A label of up to two lines (long English labels wrap inside `width`) over a
// value. The value always sits at y + kStatValueOffset so a row of stats aligns.
constexpr int kStatValueOffset = 40;

void draw_insight_stat(Canvas &canvas, int x, int y, int width,
                       const char *label, const char *value, uint8_t value_size,
                       uint8_t label_size = ui_tokens::reading_size()) {
  ui_draw_wrapped_text_pixels(canvas, x, y, width, ui_text(label),
                              label_size, GrayLevel::Black, 2, 2,
                              FontWeight::Regular);
  canvas.draw_text_pixels(x, y + kStatValueOffset, value, value_size,
                          GrayLevel::Black, FontWeight::Medium);
}

void draw_insight_stat(Canvas &canvas, int x, int y, int width,
                       const char *label, uint16_t value,
                       uint8_t label_size = ui_tokens::reading_size()) {
  char text[16] = {};
  std::snprintf(text, sizeof(text), "%u", static_cast<unsigned>(value));
  draw_insight_stat(canvas, x, y, width, label, text,
                    ui_tokens::kSectionHeading, label_size);
}

} // namespace

void kick_page_render_today(Canvas &canvas, const KickDaySummary &today,
                            bool save_failed, KickMethod method) {
  // Use the shared Chinese typography policy and keep numeric displays stable.
  const bool chinese = ui_language_is_chinese();
  const uint8_t reading_size = ui_tokens::reading_size();
  const uint8_t heading_size = chinese ? ui_tokens::kEmphasis
                                      : ui_tokens::reading_size();
  const uint8_t button_size = ui_tokens::button_size();
  prototype_ui::begin_page(canvas);
  prototype_ui::draw_header(canvas, "KICK COUNTER",
                            "NOTICE YOUR BABY'S USUAL PATTERN", "APPS");
  const bool is_portrait = portrait(canvas.width(), canvas.height());
  if (is_portrait) {
    canvas.draw_text_pixels(ui_tokens::kContentMargin, 108, ui_text("TODAY'S MOVEMENTS"), heading_size,
                            GrayLevel::Black, FontWeight::Medium);
    char total[16] = {};
    std::snprintf(total, sizeof(total), "%u",
                  static_cast<unsigned>(today.total));
    canvas.draw_text_pixels(ui_tokens::kContentMargin, 136, total, ui_tokens::kDisplayLarge, GrayLevel::Black,
                            FontWeight::Medium);
    canvas.draw_text_pixels(24 + font_text_width_pixels(total, ui_tokens::kDisplayLarge,
                                                        FontWeight::Medium) +
                                12,
                            170, ui_text("MOVEMENTS"), reading_size, GrayLevel::Black,
                            FontWeight::Regular);
    draw_insight_stat(canvas, 320, 128, 136, "SESSIONS", today.session_count, reading_size);
    canvas.draw_line(24, 220, 456, 220, GrayLevel::Black);
    draw_period_values(canvas, 24, 244, 148, today, false, reading_size);
    canvas.draw_line(24, 320, 456, 320, GrayLevel::Black);
    canvas.draw_text_pixels(ui_tokens::kContentMargin, 344, ui_text("LATEST SESSION"), heading_size,
                            GrayLevel::Black, FontWeight::Medium);
    if (today.has_latest) {
      char latest[64] = {};
      std::snprintf(latest, sizeof(latest),
                    ui_language_is_chinese() ? "%02u:%02u · %u 次 · %u 分钟"
                                             : "%02u:%02u · %u MOVES · %u MIN",
                    static_cast<unsigned>(today.latest_hour),
                    static_cast<unsigned>(today.latest_minute),
                    static_cast<unsigned>(today.latest_count),
                    static_cast<unsigned>(today.latest_duration_minutes));
      canvas.draw_text_pixels(ui_tokens::kContentMargin, 376, latest, ui_tokens::kBody, GrayLevel::Black,
                              FontWeight::Regular);
    } else {
      canvas.draw_text_pixels(ui_tokens::kContentMargin, 376, ui_text("NO SESSIONS YET"), ui_tokens::kBody,
                              GrayLevel::Black, FontWeight::Regular);
    }
    ui_draw_wrapped_text_pixels(
        canvas, ui_tokens::kContentMargin, 432, 432,
        ui_text("RECORDS HELP REVIEW PATTERNS; THEY DO NOT REPLACE CARE"),
        reading_size, GrayLevel::Black, 2, chinese ? ui_tokens::kBodyLineGap : 4,
        FontWeight::Regular);
  } else {
    canvas.draw_text_pixels(ui_tokens::kContentMargin, 104, ui_text("TODAY'S MOVEMENTS"), heading_size,
                            GrayLevel::Black, FontWeight::Medium);
    char total[16] = {};
    std::snprintf(total, sizeof(total), "%u",
                  static_cast<unsigned>(today.total));
    canvas.draw_text_pixels(ui_tokens::kContentMargin, 132, total, ui_tokens::kDisplayLarge, GrayLevel::Black,
                            FontWeight::Medium);
    canvas.draw_text_pixels(24 + font_text_width_pixels(total, ui_tokens::kDisplayLarge,
                                                        FontWeight::Medium) +
                                12,
                            166, ui_text("MOVEMENTS"), reading_size, GrayLevel::Black,
                            FontWeight::Regular);
    draw_insight_stat(canvas, 248, 132, 208, "SESSIONS", today.session_count, reading_size);
    canvas.draw_text_pixels(ui_tokens::kContentMargin, 226, ui_text("LATEST SESSION"), heading_size,
                            GrayLevel::Black, FontWeight::Medium);
    char latest[64] = {};
    if (today.has_latest) {
      std::snprintf(latest, sizeof(latest),
                    ui_language_is_chinese() ? "%02u:%02u · %u 次 · %u 分钟"
                                             : "%02u:%02u · %u MOVES · %u MIN",
                    static_cast<unsigned>(today.latest_hour),
                    static_cast<unsigned>(today.latest_minute),
                    static_cast<unsigned>(today.latest_count),
                    static_cast<unsigned>(today.latest_duration_minutes));
    } else {
      std::snprintf(latest, sizeof(latest), "%s", ui_text("NO SESSIONS YET"));
    }
    canvas.draw_text_pixels(ui_tokens::kContentMargin, 254, latest, ui_tokens::kBody, GrayLevel::Black,
                            FontWeight::Regular);
    canvas.draw_line(384, 100, 384, 336, GrayLevel::Black);
    draw_period_values(canvas, 416, 108, 120, today, true, reading_size);
  }
  if (save_failed) {
    canvas.draw_text_pixels(ui_tokens::kContentMargin, is_portrait ? 612 : 308,
                            ui_text("SAVE FAILED · TRY AGAIN"), reading_size,
                            GrayLevel::Black, FontWeight::Medium);
  }
  const Rect change = method_change_rect(canvas.width(), canvas.height());
  canvas.draw_text_pixels(ui_tokens::kContentMargin, change.y, ui_text("METHOD"),
                          reading_size, GrayLevel::Black,
                          FontWeight::Regular);
  canvas.draw_text_pixels(ui_tokens::kContentMargin, change.y + (chinese ? 28 : 22),
                          ui_text(method.set ? kick_mode_label(method.mode)
                                             : "NOT SET"),
                          ui_tokens::kBody, GrayLevel::Black, FontWeight::Medium);
  prototype_ui::draw_button_pixels(canvas, change, "CHANGE", false, button_size);
  const Rect start = primary_button_rect(canvas.width(), canvas.height());
  const Rect insights = secondary_button_rect(canvas.width(), canvas.height());
  // A remembered check-in opens the feel page, so the button says so.
  prototype_ui::draw_button_pixels(canvas, start,
                            method.set && method.mode == KickMode::Awareness
                                ? kick_mode_label(KickMode::Awareness)
                                : "START COUNTING",
                            true, button_size);
  prototype_ui::draw_button_pixels(canvas, insights, "VIEW 7-DAY TREND",
                                   false, button_size);
}

void kick_page_render_insights(Canvas &canvas, const KickDaySummary &today,
                               const KickTrendSummary &trend) {
  prototype_ui::begin_page(canvas);
  prototype_ui::draw_header(canvas, "KICK TREND",
                            "7-DAY RECORDS · NOT A DIAGNOSIS", "BACK");
  const bool is_portrait = portrait(canvas.width(), canvas.height());
  if (is_portrait) {
    canvas.draw_text_pixels(ui_tokens::kContentMargin, 100, ui_text("LAST 7 DAYS"), ui_tokens::reading_size(),
                            GrayLevel::Black, FontWeight::Medium);
    if (trend.has_data) {
      draw_bar_chart(canvas, {24, 126, 432, 220}, trend);
    } else {
      prototype_ui::draw_frame(canvas, {24, 126, 432, 220}, ui_tokens::kBorderStroke);
      draw_centered(canvas, 240, 214, ui_text("NO RECORDS IN 7 DAYS"), ui_tokens::kBody,
                    FontWeight::Regular);
    }
    draw_insight_stat(canvas, 24, 366, 136, "RECORDED DAYS", trend.recorded_days);
    draw_insight_stat(canvas, 174, 366, 136, "SESSIONS", trend.session_count);
    draw_insight_stat(canvas, 324, 366, 132, "DAILY AVERAGE",
                      trend.average_per_recorded_day);
    canvas.draw_line(24, 448, 456, 448, GrayLevel::Black);
    canvas.draw_text_pixels(ui_tokens::kContentMargin, 472, ui_text("TODAY BY PERIOD"), ui_tokens::reading_size(),
                            GrayLevel::Black, FontWeight::Medium);
    draw_period_values(canvas, 24, 504, 148, today, true);
  } else {
    canvas.draw_text_pixels(ui_tokens::kContentMargin, 94, ui_text("LAST 7 DAYS"), ui_tokens::reading_size(),
                            GrayLevel::Black, FontWeight::Medium);
    if (trend.has_data) {
      draw_bar_chart(canvas, {24, 118, 428, 226}, trend);
    } else {
      prototype_ui::draw_frame(canvas, {24, 118, 428, 226}, ui_tokens::kBorderStroke);
      draw_centered(canvas, 238, 212, ui_text("NO RECORDS IN 7 DAYS"), ui_tokens::kBody,
                    FontWeight::Regular);
    }
    draw_insight_stat(canvas, 476, 92, 136, "RECORDED DAYS", trend.recorded_days);
    draw_insight_stat(canvas, 628, 92, 148, "SESSIONS", trend.session_count);
    draw_insight_stat(canvas, 476, 160, 136, "DAILY AVERAGE",
                      trend.average_per_recorded_day);
    draw_insight_stat(canvas, 628, 160, 148, "MOST RECORDED",
                      trend.has_data ? period_label(trend.busiest_period) : "--",
                      20);
    canvas.draw_line(476, 232, 776, 232, GrayLevel::Black);
    canvas.draw_text_pixels(476, 248, ui_text("TODAY BY PERIOD"), ui_tokens::reading_size(),
                            GrayLevel::Black, FontWeight::Medium);
    draw_period_rows(canvas, 476, 274, 300, today);
  }
  prototype_ui::draw_button(canvas, primary_button_rect(canvas.width(), canvas.height()),
                "START COUNTING", true);
  prototype_ui::draw_button(canvas, secondary_button_rect(canvas.width(), canvas.height()),
                "COMPARE WITH USUAL", false);
}

void kick_page_render_counter(Canvas &canvas, KickPeriod period,
                              uint16_t count, uint16_t elapsed_minutes,
                              KickMode mode) {
  prototype_ui::begin_page(canvas);
  char title[48] = {};
  std::snprintf(title, sizeof(title),
                ui_language_is_chinese() ? "%s · %s" : "%s · %s",
                ui_text(kick_mode_label(mode)), period_label(period));
  prototype_ui::draw_header(canvas, title,
                            "TAP ONCE FOR EACH DISTINCT MOVEMENT", "CANCEL");
  const bool is_portrait = portrait(canvas.width(), canvas.height());
  const Rect circle = counter_circle(canvas.width(), canvas.height());
  const int radius = circle.width / 2;
  const int center_x = circle.x + radius;
  const int center_y = circle.y + radius;
  canvas.draw_circle(center_x, center_y, radius, GrayLevel::Black);
  canvas.draw_circle(center_x, center_y, radius - 1, GrayLevel::Black);
  char count_text[16] = {};
  if (mode == KickMode::CountToTen)
    std::snprintf(count_text, sizeof(count_text), "%u/%u",
                  static_cast<unsigned>(count),
                  static_cast<unsigned>(kKickCountToTenTarget));
  else
    std::snprintf(count_text, sizeof(count_text), "%u",
                  static_cast<unsigned>(count));
  draw_centered(canvas, center_x, center_y - 56, count_text, ui_tokens::kCounterDisplay,
                FontWeight::Medium);
  draw_centered(canvas, center_x, center_y + 34, ui_text("TAP OR TOP KEY"), ui_tokens::kBody,
                FontWeight::Medium);
  char elapsed[48] = {};
  std::snprintf(elapsed, sizeof(elapsed),
                ui_language_is_chinese() ? "已计时 %u 分钟" : "%u MIN ELAPSED",
                static_cast<unsigned>(elapsed_minutes));
  draw_centered(canvas, center_x, center_y + 70, elapsed, ui_tokens::reading_size(),
                FontWeight::Regular);

  if (is_portrait) {
    ui_draw_wrapped_text_pixels(
        canvas, 48, 526, 384,
        ui_text("A CONTINUOUS SERIES OF MOTIONS CAN BE RECORDED AS ONE MOVEMENT"),
        ui_tokens::reading_size(), GrayLevel::Black, 3, 6, FontWeight::Regular);
  } else {
    // The circle is centred, so the hint sits on one line below it.
    draw_centered(canvas, canvas.width() / 2, circle.y + circle.height + 16,
                  ui_text("A CONTINUOUS SERIES OF MOTIONS CAN BE RECORDED AS ONE MOVEMENT"),
                  ui_tokens::reading_size(), FontWeight::Regular);
  }
  prototype_ui::draw_button(canvas, primary_button_rect(canvas.width(), canvas.height()),
                "UNDO ONE", false);
  prototype_ui::draw_button(canvas, secondary_button_rect(canvas.width(), canvas.height()),
                "FINISH AND SAVE", true);
}

bool kick_page_back_target(KickPage page, KickPage compare_origin,
                           KickPage &target) {
  switch (page) {
  case KickPage::Today:
    return false;
  case KickPage::Feel:
    target = KickPage::ModeSelect;
    return true;
  case KickPage::Compare:
    target = compare_origin == KickPage::Result ? KickPage::Result
                                                : KickPage::Insights;
    return true;
  case KickPage::Insights:
  case KickPage::ModeSelect:
  case KickPage::Counter:
  case KickPage::Result:
    target = KickPage::Today;
    return true;
  }
  return false;
}

size_t kick_page_targets(KickPage page, int width, int height,
                         KickTarget *out, size_t capacity) {
  size_t count = 0U;
  const auto add = [&](const Rect &rect, KickAction action, bool circle = false) {
    if (out != nullptr && count < capacity) out[count] = {rect, action, circle};
    ++count;
  };
  add(prototype_ui::header_action_rect(width, height), KickAction::Back);
  const Rect primary = primary_button_rect(width, height);
  const Rect secondary = secondary_button_rect(width, height);
  if (page == KickPage::Today) {
    add(primary, KickAction::Start);
    add(secondary, KickAction::ShowInsights);
    add(method_change_rect(width, height), KickAction::ChangeMethod);
  } else if (page == KickPage::Insights) {
    add(primary, KickAction::Start);
    add(secondary, KickAction::ShowCompare);
  } else if (page == KickPage::ModeSelect) {
    constexpr KickAction kActions[] = {KickAction::StartTimed,
                                       KickAction::StartCountToTen,
                                       KickAction::StartAwareness};
    for (size_t index = 0U; index < 3U; ++index)
      add(mode_option_rect(width, height, index), kActions[index]);
  } else if (page == KickPage::Feel) {
    constexpr KickAction kActions[] = {KickAction::FeelUsual,
                                       KickAction::FeelWeaker,
                                       KickAction::FeelFewer,
                                       KickAction::FeelChanged};
    for (size_t index = 0U; index < 4U; ++index)
      add(feel_option_rect(width, height, index), kActions[index]);
  } else if (page == KickPage::Result) {
    add(primary, KickAction::Done);
    add(secondary, KickAction::ShowCompare);
  } else if (page == KickPage::Compare) {
    add(full_button_rect(width, height), KickAction::Start);
  } else {
    add(primary, KickAction::Undo);
    add(secondary, KickAction::Finish);
    add(counter_circle(width, height), KickAction::Count, true);
  }
  return count < capacity ? count : capacity;
}

KickAction kick_page_action_at(KickPage page, int width, int height, int x,
                               int y) {
  if (x < 0 || y < 0 || x >= width || y >= height)
    return KickAction::None;
  KickTarget targets[16];
  const size_t count = kick_page_targets(page, width, height, targets, 16U);
  for (size_t index = 0U; index < count; ++index) {
    if (targets[index].contains(x, y))
      return targets[index].action;
  }
  return KickAction::None;
}

const char *kick_mode_label(KickMode mode) {
  if (mode == KickMode::CountToTen)
    return "COUNT TO 10";
  if (mode == KickMode::Awareness)
    return "FEEL CHECK-IN";
  return "1-HOUR COUNT";
}

const char *kick_feel_label(KickFeel feel) {
  switch (feel) {
  case KickFeel::Usual:
    return "AS USUAL";
  case KickFeel::Weaker:
    return "WEAKER";
  case KickFeel::Fewer:
    return "FEWER";
  case KickFeel::Changed:
    return "PATTERN CHANGED";
  case KickFeel::None:
    break;
  }
  return "--";
}

void kick_format_duration(uint16_t seconds, char *output, size_t size) {
  if (output == nullptr || size == 0U)
    return;
  const unsigned minutes = seconds / 60U;
  const unsigned remainder = seconds % 60U;
  std::snprintf(output, size,
                ui_language_is_chinese() ? "%u分%02u秒" : "%u:%02u", minutes,
                remainder);
}

namespace {

const char *timed_unit() {
  return ui_language_is_chinese() ? "次/小时" : "PER HOUR";
}

void format_percent(const KickComparison &comparison, char *output,
                    size_t size) {
  output[0] = '\0';
  if (!comparison.percent_known || comparison.direction == KickDirection::Same)
    return;
  const bool less = comparison.direction == KickDirection::Less;
  if (ui_language_is_chinese())
    std::snprintf(output, size, "，%s %u%%", less ? "少" : "多",
                  static_cast<unsigned>(comparison.percent));
  else
    std::snprintf(output, size, " (%c%u%%)", less ? '-' : '+',
                  static_cast<unsigned>(comparison.percent));
}

} // namespace

void kick_format_comparison(const KickComparison &comparison,
                            KickComparisonText &output) {
  output = {};
  const bool zh = ui_language_is_chinese();
  const bool timed = comparison.mode == KickMode::Timed;
  if (comparison.status == KickComparisonStatus::Unsupported)
    return;
  if (comparison.status == KickComparisonStatus::NotComparable) {
    std::snprintf(output.value, sizeof(output.value), "%s",
                  timed ? (zh ? "本次不足 15 分钟" : "UNDER 15 MINUTES")
                        : (zh ? "本次未数满 10 次" : "ENDED BEFORE TEN"));
    std::snprintf(output.baseline, sizeof(output.baseline), "%s",
                  zh ? "不参与比对" : "NOT COMPARED");
    return;
  }

  char current[24] = {};
  char baseline[24] = {};
  if (timed) {
    std::snprintf(output.value, sizeof(output.value),
                  zh ? "每小时 %u 次" : "%u PER HOUR",
                  static_cast<unsigned>(comparison.current));
  } else if (comparison.current_capped) {
    std::snprintf(output.value, sizeof(output.value), "%s",
                  zh ? "60 分钟内未数满 10 次" : "UNDER 10 IN 60 MIN");
  } else {
    kick_format_duration(comparison.current, current, sizeof(current));
    std::snprintf(output.value, sizeof(output.value),
                  zh ? "数到 10 用时 %s" : "TEN IN %s", current);
  }

  if (comparison.status == KickComparisonStatus::CollectingBaseline) {
    std::snprintf(output.baseline, sizeof(output.baseline),
                  zh ? "基线积累中：过去 7 天已有 %u 条，需 %u 条"
                     : "BUILDING BASELINE: %u OF %u RECORDS IN 7 DAYS",
                  static_cast<unsigned>(comparison.baseline_sessions),
                  static_cast<unsigned>(kKickBaselineMinimumSessions));
    return;
  }

  if (timed) {
    std::snprintf(output.baseline, sizeof(output.baseline),
                  zh ? "过去 7 天同时段中位数：每小时 %u 次（%u 条记录）"
                     : "LAST 7 DAYS SAME-PERIOD MEDIAN: %u PER HOUR (%u RECORDS)",
                  static_cast<unsigned>(comparison.baseline),
                  static_cast<unsigned>(comparison.baseline_sessions));
  } else {
    kick_format_duration(comparison.baseline, baseline, sizeof(baseline));
    std::snprintf(output.baseline, sizeof(output.baseline),
                  zh ? "过去 7 天中位数：%s（%u 条记录）"
                     : "LAST 7 DAYS MEDIAN: %s (%u RECORDS)",
                  baseline,
                  static_cast<unsigned>(comparison.baseline_sessions));
  }

  if (comparison.direction == KickDirection::Same) {
    std::snprintf(output.difference, sizeof(output.difference), "%s",
                  zh ? "与平时持平" : "SAME AS USUAL");
    return;
  }
  char percent[24] = {};
  format_percent(comparison, percent, sizeof(percent));
  const bool less = comparison.direction == KickDirection::Less;
  if (timed) {
    if (zh)
      std::snprintf(output.difference, sizeof(output.difference),
                    "比平时%s %u %s%s", less ? "少" : "多",
                    static_cast<unsigned>(comparison.difference), timed_unit(),
                    percent);
    else
      std::snprintf(output.difference, sizeof(output.difference),
                    "%u %s %s THAN USUAL%s",
                    static_cast<unsigned>(comparison.difference), timed_unit(),
                    less ? "FEWER" : "MORE", percent);
    return;
  }
  char difference[24] = {};
  kick_format_duration(comparison.difference, difference, sizeof(difference));
  if (comparison.current_capped) {
    std::snprintf(output.difference, sizeof(output.difference),
                  zh ? "比平时至少多用 %s" : "AT LEAST %s LONGER THAN USUAL",
                  difference);
    return;
  }
  if (zh)
    std::snprintf(output.difference, sizeof(output.difference),
                  "比平时%s用 %s%s", less ? "少" : "多", difference, percent);
  else
    std::snprintf(output.difference, sizeof(output.difference),
                  "%s %s THAN USUAL%s", difference,
                  less ? "SHORTER" : "LONGER", percent);
}

void kick_format_comparison_compact(const KickComparison &comparison,
                                    char *output, size_t size) {
  if (output == nullptr || size == 0U)
    return;
  KickComparisonText text = {};
  kick_format_comparison(comparison, text);
  if (comparison.status == KickComparisonStatus::Ready)
    std::snprintf(output, size, "%s · %s", text.value, text.difference);
  else
    std::snprintf(output, size, "%s · %s", text.value, text.baseline);
}

void kick_format_awareness(const KickAwarenessSummary &summary, char *output,
                           size_t size) {
  if (output == nullptr || size == 0U)
    return;
  if (ui_language_is_chinese())
    std::snprintf(output, size, "一样 %u · 变弱 %u · 变少 %u · 规律变了 %u",
                  static_cast<unsigned>(summary.usual),
                  static_cast<unsigned>(summary.weaker),
                  static_cast<unsigned>(summary.fewer),
                  static_cast<unsigned>(summary.changed));
  else
    std::snprintf(output, size, "USUAL %u · WEAKER %u · FEWER %u · CHANGED %u",
                  static_cast<unsigned>(summary.usual),
                  static_cast<unsigned>(summary.weaker),
                  static_cast<unsigned>(summary.fewer),
                  static_cast<unsigned>(summary.changed));
}

void kick_page_render_mode_select(Canvas &canvas, KickMethod method) {
  prototype_ui::begin_page(canvas);
  prototype_ui::draw_header(canvas, "CHOOSE A RECORD TYPE",
                            "COMPARED WITH YOUR OWN LAST 7 DAYS", "BACK");
  const bool compact = !portrait(canvas.width(), canvas.height());
  for (size_t index = 0U; index < 3U; ++index) {
    draw_option(canvas, mode_option_rect(canvas.width(), canvas.height(), index),
                kick_mode_label(kModeOptions[index]),
                mode_description(kModeOptions[index]), compact,
                method.set && method.mode == kModeOptions[index]);
  }
}

void kick_page_render_feel(Canvas &canvas) {
  prototype_ui::begin_page(canvas);
  prototype_ui::draw_header(canvas, "FEEL CHECK-IN",
                            "TODAY'S MOVEMENTS COMPARED WITH USUAL", "BACK");
  for (size_t index = 0U; index < 4U; ++index) {
    prototype_ui::draw_button(
        canvas, feel_option_rect(canvas.width(), canvas.height(), index),
        kick_feel_label(kFeelOptions[index]), false, 3U);
  }
}

void kick_page_render_result(Canvas &canvas, const KickSession &session,
                             const KickComparison &comparison,
                             const KickAwarenessSummary &awareness) {
  prototype_ui::begin_page(canvas);
  prototype_ui::draw_header(canvas, "THIS RECORD",
                            kick_mode_label(session.mode), "BACK");
  const bool is_portrait = portrait(canvas.width(), canvas.height());
  const int width = is_portrait ? 432 : 752;
  char line[96] = {};
  if (session.mode == KickMode::Awareness) {
    std::snprintf(line, sizeof(line),
                  ui_language_is_chinese() ? "已记录：%s" : "RECORDED: %s",
                  ui_text(kick_feel_label(session.feel)));
    canvas.draw_text_pixels(ui_tokens::kContentMargin, 112, line, is_portrait ? ui_tokens::kSectionHeading : ui_tokens::kPageHeading,
                            GrayLevel::Black, FontWeight::Medium);
    canvas.draw_text_pixels(ui_tokens::kContentMargin, is_portrait ? 176 : 172,
                            ui_text("LAST 7 DAYS"), ui_tokens::reading_size(), GrayLevel::Black,
                            FontWeight::Medium);
    kick_format_awareness(awareness, line, sizeof(line));
    ui_draw_wrapped_text_pixels(canvas, ui_tokens::kContentMargin, is_portrait ? 208 : 202, width,
                                line, ui_tokens::kEmphasis, GrayLevel::Black, 2, 6,
                                FontWeight::Regular);
  } else {
    char duration[24] = {};
    kick_format_duration(session.elapsed_seconds, duration, sizeof(duration));
    std::snprintf(line, sizeof(line),
                  ui_language_is_chinese() ? "%02u:%02u 开始 · %u 次 · 用时 %s"
                                           : "%02u:%02u · %u MOVES · %s",
                  static_cast<unsigned>(session.hour),
                  static_cast<unsigned>(session.minute),
                  static_cast<unsigned>(session.count), duration);
    canvas.draw_text_pixels(ui_tokens::kContentMargin, 100, line, ui_tokens::reading_size(), GrayLevel::Black,
                            FontWeight::Regular);
    canvas.draw_line(24, 132, 24 + width, 132, GrayLevel::Black);
    draw_comparison_block(canvas, 24, 152, width, comparison, is_portrait);
  }
  prototype_ui::draw_button(canvas, primary_button_rect(canvas.width(), canvas.height()),
                "DONE", true);
  prototype_ui::draw_button(canvas, secondary_button_rect(canvas.width(), canvas.height()),
                "COMPARE WITH USUAL", false);
}

void kick_page_render_compare(Canvas &canvas, const KickComparison periods[3],
                              const bool has_period[3], bool has_ten,
                              const KickComparison &ten, bool has_awareness,
                              const KickAwarenessSummary &awareness) {
  prototype_ui::begin_page(canvas);
  prototype_ui::draw_header(canvas, "COMPARE WITH USUAL",
                            "TODAY VS YOUR LAST 7 DAYS · MEDIAN", "BACK");
  const bool is_portrait = portrait(canvas.width(), canvas.height());
  const int label_width = is_portrait ? 0 : 152;
  const int text_x = 24 + label_width;
  const int text_width = (is_portrait ? 432 : 752) - label_width;
  const int row_height = is_portrait ? 112 : 58;
  int y = is_portrait ? 108 : 96;
  char text[160] = {};

  // Portrait rows stack the value over its comparison; landscape keeps one
  // line per row after the label column.
  const auto draw_row = [&](const char *label, const char *first,
                            const char *second) {
    canvas.draw_text_pixels(ui_tokens::kContentMargin, y, label, ui_tokens::reading_size(), GrayLevel::Black,
                            FontWeight::Medium);
    if (is_portrait) {
      ui_draw_wrapped_text_pixels(canvas, ui_tokens::kContentMargin, y + 28, text_width, first, ui_tokens::reading_size(),
                                  GrayLevel::Black, 2, 2, FontWeight::Regular);
      if (second != nullptr && second[0] != '\0')
        ui_draw_wrapped_text_pixels(canvas, ui_tokens::kContentMargin, y + 54, text_width, second,
                                    ui_tokens::reading_size(), GrayLevel::Black, 1, 4,
                                    FontWeight::Medium);
    } else {
      std::snprintf(text, sizeof(text), "%s%s%s", first,
                    second != nullptr && second[0] != '\0' ? " · " : "",
                    second != nullptr ? second : "");
      ui_draw_wrapped_text_pixels(canvas, text_x, y, text_width, text, ui_tokens::reading_size(),
                                  GrayLevel::Black, 2, 4,
                                  FontWeight::Regular);
    }
    canvas.draw_line(24, y + row_height - 10, is_portrait ? 456 : 776,
                     y + row_height - 10, GrayLevel::Black);
    y += row_height;
  };

  const auto draw_comparison_row = [&](const char *label, bool has,
                                       const KickComparison &comparison,
                                       const char *empty) {
    if (!has) {
      draw_row(label, ui_text(empty), nullptr);
      return;
    }
    KickComparisonText lines = {};
    kick_format_comparison(comparison, lines);
    draw_row(label, lines.value,
             comparison.status == KickComparisonStatus::Ready
                 ? lines.difference
                 : lines.baseline);
  };

  const KickPeriod order[] = {KickPeriod::Morning, KickPeriod::Afternoon,
                              KickPeriod::Evening};
  for (size_t index = 0U; index < 3U; ++index)
    draw_comparison_row(period_label(order[index]), has_period[index],
                        periods[index], "NO 1-HOUR COUNT TODAY");
  draw_comparison_row(ui_text("COUNT TO 10"), has_ten, ten,
                      "NO COUNT-TO-10 IN 7 DAYS");
  char awareness_text[96] = {};
  if (has_awareness)
    kick_format_awareness(awareness, awareness_text, sizeof(awareness_text));
  draw_row(ui_text("FEEL CHECK-IN"),
           has_awareness ? awareness_text : ui_text("NO CHECK-INS IN 7 DAYS"),
           nullptr);
  prototype_ui::draw_button(canvas, full_button_rect(canvas.width(), canvas.height()),
                "START COUNTING", true);
}
