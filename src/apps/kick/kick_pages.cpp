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
  return portrait(width, height) ? Rect{24, 720, 208, 56}
                                 : Rect{24, 400, 368, 56};
}

Rect secondary_button_rect(int width, int height) {
  return portrait(width, height) ? Rect{248, 720, 208, 56}
                                 : Rect{408, 400, 368, 56};
}

Rect full_button_rect(int width, int height) {
  return portrait(width, height) ? Rect{24, 720, 432, 56}
                                 : Rect{24, 400, 752, 56};
}

bool counter_target_contains(int width, int height, int x, int y) {
  const bool is_portrait = portrait(width, height);
  const int center_x = 240;
  const int center_y = is_portrait ? 332 : 234;
  const int radius = is_portrait ? 128 : 112;
  const int dx = x - center_x;
  const int dy = y - center_y;
  return dx * dx + dy * dy <= radius * radius;
}

void draw_centered(Canvas &canvas, int center_x, int y, const char *text,
                   uint8_t pixels, FontWeight weight = FontWeight::Regular) {
  const int width = font_text_width_pixels(text, pixels, weight);
  canvas.draw_text_pixels(center_x - width / 2, y, text, pixels,
                          GrayLevel::Black, weight);
}

void render_button(Canvas &canvas, const Rect &rect, const char *label,
                   bool filled) {
  prototype_ui::draw_button(canvas, rect, label, filled, 2U);
}

const char *period_label(KickPeriod period) {
  if (period == KickPeriod::Morning)
    return ui_text("MORNING");
  if (period == KickPeriod::Afternoon)
    return ui_text("AFTERNOON");
  return ui_text("EVENING");
}

void draw_period_values(Canvas &canvas, int x, int y, int column_width,
                        const KickDaySummary &summary, bool compact) {
  const uint16_t values[] = {summary.morning, summary.afternoon,
                             summary.evening};
  const KickPeriod periods[] = {KickPeriod::Morning, KickPeriod::Afternoon,
                                KickPeriod::Evening};
  for (size_t index = 0U; index < 3U; ++index) {
    const int left = x + static_cast<int>(index) * column_width;
    canvas.draw_text_pixels(left, y, period_label(periods[index]), 14,
                            GrayLevel::Black, FontWeight::Regular);
    char value[16] = {};
    std::snprintf(value, sizeof(value), "%u",
                  static_cast<unsigned>(values[index]));
    canvas.draw_text_pixels(left, y + 28, value, compact ? 24U : 28U,
                            GrayLevel::Black, FontWeight::Medium);
  }
}

void draw_safety_note(Canvas &canvas, const Rect &rect, bool compact) {
  canvas.draw_rect(rect.x, rect.y, rect.width, rect.height, GrayLevel::Black);
  canvas.draw_circle(rect.x + 24, rect.y + 25, 9, GrayLevel::Black);
  canvas.draw_line(rect.x + 24, rect.y + 19, rect.x + 24, rect.y + 26,
                   GrayLevel::Black);
  canvas.draw_line(rect.x + 24, rect.y + 29, rect.x + 24, rect.y + 30,
                   GrayLevel::Black);
  canvas.draw_text_pixels(rect.x + 44, rect.y + 13, ui_text("IMPORTANT"), 18,
                          GrayLevel::Black, FontWeight::Medium);
  ui_draw_wrapped_text_pixels(
      canvas, rect.x + 16, rect.y + 52, rect.width - 32,
      ui_text("IF MOVEMENTS ARE LESS THAN USUAL, STOP, OR CHANGE, CONTACT MATERNITY CARE NOW"),
      compact ? 14U : 16U, GrayLevel::Black, 3U, 5,
      FontWeight::Regular);
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
                    rect.y + available_height - bar_height - 20, count, 14,
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
    draw_centered(canvas, center, rect.y + rect.height - 22, day, 14,
                  index + 1U == kKickTrendDays ? FontWeight::Medium
                                              : FontWeight::Regular);
  }
}

void draw_insight_stat(Canvas &canvas, int x, int y, const char *label,
                       uint16_t value) {
  canvas.draw_text_pixels(x, y, ui_text(label), 14, GrayLevel::Black,
                          FontWeight::Regular);
  char text[16] = {};
  std::snprintf(text, sizeof(text), "%u", static_cast<unsigned>(value));
  canvas.draw_text_pixels(x, y + 26, text, 24, GrayLevel::Black,
                          FontWeight::Medium);
}

} // namespace

void kick_page_render_today(Canvas &canvas, const KickDaySummary &today,
                            bool save_failed) {
  prototype_ui::begin_page(canvas);
  prototype_ui::draw_header(canvas, "KICK COUNTER",
                            "NOTICE YOUR BABY'S USUAL PATTERN", "BACK");
  const bool is_portrait = portrait(canvas.width(), canvas.height());
  if (is_portrait) {
    canvas.draw_text_pixels(24, 108, ui_text("TODAY'S MOVEMENTS"), 16,
                            GrayLevel::Black, FontWeight::Medium);
    char total[16] = {};
    std::snprintf(total, sizeof(total), "%u",
                  static_cast<unsigned>(today.total));
    canvas.draw_text_pixels(24, 136, total, 56, GrayLevel::Black,
                            FontWeight::Medium);
    canvas.draw_text_pixels(24 + font_text_width_pixels(total, 56,
                                                        FontWeight::Medium) +
                                12,
                            170, ui_text("MOVEMENTS"), 16, GrayLevel::Black,
                            FontWeight::Regular);
    draw_insight_stat(canvas, 320, 128, "SESSIONS", today.session_count);
    canvas.draw_line(24, 220, 456, 220, GrayLevel::Black);
    draw_period_values(canvas, 24, 244, 148, today, false);
    canvas.draw_line(24, 320, 456, 320, GrayLevel::Black);
    canvas.draw_text_pixels(24, 344, ui_text("LATEST SESSION"), 16,
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
      canvas.draw_text_pixels(24, 376, latest, 18, GrayLevel::Black,
                              FontWeight::Regular);
    } else {
      canvas.draw_text_pixels(24, 376, ui_text("NO SESSIONS YET"), 18,
                              GrayLevel::Black, FontWeight::Regular);
    }
    draw_safety_note(canvas, {24, 432, 432, 136}, false);
    canvas.draw_text_pixels(
        24, 592,
        ui_text("RECORDS HELP REVIEW PATTERNS; THEY DO NOT REPLACE CARE"), 14,
        GrayLevel::Black, FontWeight::Regular);
  } else {
    canvas.draw_text_pixels(24, 104, ui_text("TODAY'S MOVEMENTS"), 16,
                            GrayLevel::Black, FontWeight::Medium);
    char total[16] = {};
    std::snprintf(total, sizeof(total), "%u",
                  static_cast<unsigned>(today.total));
    canvas.draw_text_pixels(24, 132, total, 56, GrayLevel::Black,
                            FontWeight::Medium);
    canvas.draw_text_pixels(24 + font_text_width_pixels(total, 56,
                                                        FontWeight::Medium) +
                                12,
                            166, ui_text("MOVEMENTS"), 16, GrayLevel::Black,
                            FontWeight::Regular);
    draw_insight_stat(canvas, 248, 132, "SESSIONS", today.session_count);
    canvas.draw_text_pixels(24, 226, ui_text("LATEST SESSION"), 14,
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
    canvas.draw_text_pixels(24, 254, latest, 18, GrayLevel::Black,
                            FontWeight::Regular);
    canvas.draw_line(384, 100, 384, 336, GrayLevel::Black);
    draw_period_values(canvas, 416, 108, 120, today, true);
    draw_safety_note(canvas, {408, 196, 368, 140}, true);
  }
  if (save_failed) {
    canvas.draw_text_pixels(is_portrait ? 24 : 24, is_portrait ? 632 : 352,
                            ui_text("SAVE FAILED · TRY AGAIN"), 14,
                            GrayLevel::Black, FontWeight::Medium);
  }
  const Rect start = primary_button_rect(canvas.width(), canvas.height());
  const Rect insights = secondary_button_rect(canvas.width(), canvas.height());
  render_button(canvas, start, "START COUNTING", true);
  render_button(canvas, insights, "VIEW 7-DAY TREND", false);
}

void kick_page_render_insights(Canvas &canvas, const KickDaySummary &today,
                               const KickTrendSummary &trend) {
  prototype_ui::begin_page(canvas);
  prototype_ui::draw_header(canvas, "KICK TREND",
                            "7-DAY RECORDS · NOT A DIAGNOSIS", "BACK");
  const bool is_portrait = portrait(canvas.width(), canvas.height());
  if (is_portrait) {
    canvas.draw_text_pixels(24, 100, ui_text("LAST 7 DAYS"), 16,
                            GrayLevel::Black, FontWeight::Medium);
    if (trend.has_data) {
      draw_bar_chart(canvas, {24, 126, 432, 220}, trend);
    } else {
      canvas.draw_rect(24, 126, 432, 220, GrayLevel::Black);
      draw_centered(canvas, 240, 214, ui_text("NO RECORDS IN 7 DAYS"), 18,
                    FontWeight::Regular);
    }
    draw_insight_stat(canvas, 24, 376, "RECORDED DAYS", trend.recorded_days);
    draw_insight_stat(canvas, 174, 376, "SESSIONS", trend.session_count);
    draw_insight_stat(canvas, 324, 376, "DAILY AVERAGE",
                      trend.average_per_recorded_day);
    canvas.draw_line(24, 448, 456, 448, GrayLevel::Black);
    canvas.draw_text_pixels(24, 472, ui_text("TODAY BY PERIOD"), 16,
                            GrayLevel::Black, FontWeight::Medium);
    draw_period_values(canvas, 24, 504, 148, today, true);
    draw_safety_note(canvas, {24, 576, 432, 112}, true);
  } else {
    canvas.draw_text_pixels(24, 94, ui_text("LAST 7 DAYS"), 16,
                            GrayLevel::Black, FontWeight::Medium);
    if (trend.has_data) {
      draw_bar_chart(canvas, {24, 118, 440, 226}, trend);
    } else {
      canvas.draw_rect(24, 118, 440, 226, GrayLevel::Black);
      draw_centered(canvas, 244, 212, ui_text("NO RECORDS IN 7 DAYS"), 18,
                    FontWeight::Regular);
    }
    draw_insight_stat(canvas, 496, 104, "RECORDED DAYS", trend.recorded_days);
    draw_insight_stat(canvas, 640, 104, "SESSIONS", trend.session_count);
    draw_insight_stat(canvas, 496, 176, "DAILY AVERAGE",
                      trend.average_per_recorded_day);
    canvas.draw_text_pixels(640, 176, ui_text("MOST RECORDED"), 14,
                            GrayLevel::Black, FontWeight::Regular);
    canvas.draw_text_pixels(640, 202,
                            trend.has_data ? period_label(trend.busiest_period)
                                           : "--",
                            20, GrayLevel::Black, FontWeight::Medium);
    canvas.draw_line(496, 244, 776, 244, GrayLevel::Black);
    canvas.draw_text_pixels(496, 264, ui_text("TODAY BY PERIOD"), 14,
                            GrayLevel::Black, FontWeight::Medium);
    draw_period_values(canvas, 496, 288, 92, today, true);
    canvas.draw_text_pixels(
        24, 360,
        ui_text("IF MOVEMENTS ARE LESS THAN USUAL, STOP, OR CHANGE, CONTACT MATERNITY CARE NOW"),
        14, GrayLevel::Black, FontWeight::Medium);
  }
  render_button(canvas, full_button_rect(canvas.width(), canvas.height()),
                "START COUNTING", true);
}

void kick_page_render_counter(Canvas &canvas, KickPeriod period,
                              uint16_t count, uint16_t elapsed_minutes) {
  prototype_ui::begin_page(canvas);
  char title[48] = {};
  std::snprintf(title, sizeof(title),
                ui_language_is_chinese() ? "胎动 · %s" : "KICKS · %s",
                period_label(period));
  prototype_ui::draw_header(canvas, title,
                            "TAP ONCE FOR EACH DISTINCT MOVEMENT", "CANCEL");
  const bool is_portrait = portrait(canvas.width(), canvas.height());
  const int center_x = 240;
  const int center_y = is_portrait ? 332 : 234;
  const int radius = is_portrait ? 128 : 112;
  canvas.draw_circle(center_x, center_y, radius, GrayLevel::Black);
  canvas.draw_circle(center_x, center_y, radius - 1, GrayLevel::Black);
  char count_text[16] = {};
  std::snprintf(count_text, sizeof(count_text), "%u",
                static_cast<unsigned>(count));
  draw_centered(canvas, center_x, center_y - 56, count_text, 70,
                FontWeight::Medium);
  draw_centered(canvas, center_x, center_y + 34, ui_text("TAP TO RECORD"), 18,
                FontWeight::Medium);
  char elapsed[48] = {};
  std::snprintf(elapsed, sizeof(elapsed),
                ui_language_is_chinese() ? "已计时 %u 分钟" : "%u MIN ELAPSED",
                static_cast<unsigned>(elapsed_minutes));
  draw_centered(canvas, center_x, center_y + 70, elapsed, 16,
                FontWeight::Regular);

  if (is_portrait) {
    ui_draw_wrapped_text_pixels(
        canvas, 48, 526, 384,
        ui_text("DO NOT WAIT TO FINISH COUNTING IF MOVEMENTS FEEL REDUCED OR CHANGED"),
        16, GrayLevel::Black, 3, 6, FontWeight::Medium);
  } else {
    canvas.draw_text_pixels(432, 126, ui_text("COUNT DISTINCT MOVEMENTS"), 18,
                            GrayLevel::Black, FontWeight::Medium);
    ui_draw_wrapped_text_pixels(
        canvas, 432, 166, 344,
        ui_text("A CONTINUOUS SERIES OF MOTIONS CAN BE RECORDED AS ONE MOVEMENT"),
        16, GrayLevel::Black, 3, 6, FontWeight::Regular);
    canvas.draw_line(432, 248, 776, 248, GrayLevel::Black);
    ui_draw_wrapped_text_pixels(
        canvas, 432, 272, 344,
        ui_text("DO NOT WAIT TO FINISH COUNTING IF MOVEMENTS FEEL REDUCED OR CHANGED"),
        16, GrayLevel::Black, 3, 6, FontWeight::Medium);
  }
  render_button(canvas, primary_button_rect(canvas.width(), canvas.height()),
                "UNDO ONE", false);
  render_button(canvas, secondary_button_rect(canvas.width(), canvas.height()),
                "FINISH AND SAVE", true);
}

KickAction kick_page_action_at(KickPage page, int width, int height, int x,
                               int y) {
  if (x < 0 || y < 0 || x >= width || y >= height)
    return KickAction::None;
  if (prototype_ui::header_action_rect(width, height).contains(x, y))
    return KickAction::Back;
  if (page == KickPage::Today) {
    if (primary_button_rect(width, height).contains(x, y))
      return KickAction::Start;
    if (secondary_button_rect(width, height).contains(x, y))
      return KickAction::ShowInsights;
  } else if (page == KickPage::Insights) {
    if (full_button_rect(width, height).contains(x, y))
      return KickAction::Start;
  } else {
    if (primary_button_rect(width, height).contains(x, y))
      return KickAction::Undo;
    if (secondary_button_rect(width, height).contains(x, y))
      return KickAction::Finish;
    if (counter_target_contains(width, height, x, y))
      return KickAction::Count;
  }
  return KickAction::None;
}
