#include "weight_pages.h"

#include <cstdio>
#include <cstdlib>
#include "canvas.h"
#include "ui_language.h"
#include "text_layout.h"
#include "ui_tokens.h"

namespace {
using prototype_ui::Rect;
bool portrait(int width, int height) { return height > width; }
Rect footer(size_t index, size_t count, int width, int height) {
  const int gap = 12;
  const int item = (width - 2 * ui_tokens::kContentMargin - gap * static_cast<int>(count - 1U)) / static_cast<int>(count);
  return {24 + static_cast<int>(index) * (item + gap), height - 80, item, ui_tokens::kButtonHeight};
}
// The KG/LB toggle sits left of the header action in landscape and under it in
// portrait; the trend toggle is right-aligned to the content margin. Positions
// follow the screen width instead of fixed x values.
Rect unit(int width, int height) {
  constexpr int kHeaderGap = 16;
  if (portrait(width, height))
    return {width - ui_tokens::kContentMargin - 120, 88, 120, ui_tokens::kMinimumTouchHeight};
  return {prototype_ui::header_action_rect(width, height).x - kHeaderGap - 92, 20, 92,
          ui_tokens::kMinimumTouchHeight};
}
Rect trend_toggle(int width, int height) {
  if (portrait(width, height))
    return {width - ui_tokens::kContentMargin - 176, 292, 176, ui_tokens::kMinimumTouchHeight};
  return {width - ui_tokens::kContentMargin - 216, 86, 216, ui_tokens::kMinimumTouchHeight};
}
Rect adjustment(size_t index, int width, int height) {
  if (portrait(width, height)) return {24 + static_cast<int>(index % 2U) * 224,
                                     330 + static_cast<int>(index / 2U) * 72, 208, ui_tokens::kButtonHeight};
  return {56 + static_cast<int>(index) * 176, 260, 160, ui_tokens::kMinimumTouchHeight};
}
Rect history_row(size_t row, int width, int height) {
  return {24, 116 + static_cast<int>(row) * (portrait(width, height) ? 96 : 50),
          width - 2 * ui_tokens::kContentMargin, portrait(width, height) ? 88 : 48};
}
// Profile adjusters: two rows of four (height, then baseline), each row
// big-down, small-down, small-up, big-up, like the weight editor.
Rect profile_adjust(size_t index, int width, int height) {
  const bool vertical = portrait(width, height);
  constexpr int kGap = 12;
  const int item = (width - 2 * ui_tokens::kContentMargin - 3 * kGap) / 4;
  const int y = (vertical ? 230 : 160) + static_cast<int>(index / 4U) * (vertical ? 188 : 120);
  return {ui_tokens::kContentMargin + static_cast<int>(index % 4U) * (item + kGap), y, item,
          ui_tokens::kButtonHeight};
}
// Two-state toggle: both options are visible and the active one is inverted.
// The whole rectangle stays one hit target that flips the state.
void segmented_toggle(Canvas &canvas, Rect rect, const char *first, const char *second, bool first_active) {
  const int half = rect.width / 2;
  prototype_ui::draw_button(canvas, {rect.x, rect.y, half, rect.height}, first, first_active, 2);
  prototype_ui::draw_button(canvas, {rect.x + half, rect.y, rect.width - half, rect.height}, second, !first_active, 2);
}
const char *unit_name(bool pounds) { return pounds ? "LB" : "KG"; }
void date_text(const PregnancyDate &date, char *output, size_t capacity) {
  std::snprintf(output, capacity, "%04u-%02u-%02u", date.year, date.month, date.day);
}
// Geometry of the dashboard's text and chart for one logical size. Horizontal
// positions follow from the screen width and the content margin; the vertical
// stack differs per orientation and is named here instead of scattered through
// the renderer.
struct DashboardLayout {
  int value_y;            // the big weight number (x = content margin)
  int unit_x, unit_y;     // KG / LB beside the number
  int baseline_label_y;   // "from baseline" caption
  int baseline_value_y;   // the change since the baseline
  int trend_title_x, trend_title_y;
  Rect chart;             // trend plot area
  int footer_text_y;      // height / record count line
};

DashboardLayout dashboard_layout(int width, int height) {
  constexpr int kMargin = ui_tokens::kContentMargin;
  constexpr int kLeftColumnWidth = 285; // landscape: number and baseline column
  constexpr int kUnitX = 230;
  if (portrait(width, height)) {
    return {150, kUnitX, 192, 246, 264, kMargin, 304,
            {kMargin, 356, width - 2 * kMargin, 228}, 622};
  }
  // Landscape: the chart takes the right column, from the left column's edge
  // plus a margin to the right margin.
  const int right_x = kMargin + kLeftColumnWidth + kMargin;
  return {110, kUnitX, 152, 218, 238, right_x, 100,
          {right_x, 142, width - kMargin - right_x, 190}, 372};
}

constexpr int kPlotPadding = 12;     // inside the chart area, around the points
constexpr int kPointRadius = 4;
constexpr int kEmptyTextInset = 16;  // "no records" message inside the chart
constexpr int kEmptyTextTop = 56;

void draw_trend(Canvas &canvas, const WeightService &service, const WeightPageState &state) {
  const Rect chart = dashboard_layout(canvas.width(), canvas.height()).chart;
  const int x = chart.x;
  const int y = chart.y;
  const int width = chart.width;
  const int height = chart.height;
  canvas.draw_line(x, y + height, x + width, y + height, GrayLevel::Black);
  WeightTrend trend = {};
  if (!service.trend(state.today, state.trend_days, trend) || trend.count == 0U) {
    ui_draw_wrapped_text_pixels(canvas, x + kEmptyTextInset, y + kEmptyTextTop, width - 2 * kEmptyTextInset,
        ui_text("NO WEIGHT RECORDS"), ui_tokens::kBody, GrayLevel::Black, 2, 4, FontWeight::Regular);
    return;
  }
  uint16_t minimum = 2000U, maximum = 300U;
  for (size_t i = 0U; i < trend.days; ++i) {
    if (!trend.present[i]) continue;
    if (trend.values[i] < minimum) minimum = trend.values[i];
    if (trend.values[i] > maximum) maximum = trend.values[i];
  }
  if (maximum - minimum < 20U) { minimum -= 10U; maximum += 10U; }
  int previous_x = 0, previous_y = 0;
  bool previous_present = false;
  for (size_t i = 0U; i < trend.days; ++i) {
    if (!trend.present[i]) { previous_present = false; continue; }
    const int px = x + kPlotPadding + static_cast<int>(i) * (width - 2 * kPlotPadding) / (trend.days - 1);
    const int py = y + height - kPlotPadding - static_cast<int>(trend.values[i] - minimum) * (height - 2 * kPlotPadding) / (maximum - minimum);
    canvas.fill_circle(px, py, kPointRadius, GrayLevel::Black);
    if (previous_present) canvas.draw_line(previous_x, previous_y, px, py, GrayLevel::Black);
    previous_x = px; previous_y = py; previous_present = true;
  }
}
}  // namespace

uint16_t weight_display_value(uint16_t kg, bool pounds) {
  return pounds ? static_cast<uint16_t>((static_cast<uint32_t>(kg) * 22046U + 5000U) / 10000U) : kg;
}
uint16_t weight_stored_value(uint16_t value, bool pounds) {
  return pounds ? static_cast<uint16_t>((static_cast<uint32_t>(value) * 453592U + 500000U) / 1000000U) : value;
}

// The hit targets of a page, built from the same rectangle functions the
// renderer draws with. weight_page_action_at() is the first target containing
// the point, so drawing and hit-testing cannot drift apart.
bool weight_page_back_target(WeightPage page, bool editing_history,
                             WeightPage &target) {
  switch (page) {
  case WeightPage::Dashboard:
    return false;
  case WeightPage::ReplaceConfirm:
  case WeightPage::DeleteConfirm:
    target = WeightPage::Editor;
    return true;
  case WeightPage::Editor:
    target = editing_history ? WeightPage::History : WeightPage::Dashboard;
    return true;
  case WeightPage::History:
  case WeightPage::Profile:
    target = WeightPage::Dashboard;
    return true;
  }
  return false;
}

size_t weight_page_targets(WeightPage page, int width, int height,
                           WeightTarget *out, size_t capacity) {
  size_t count = 0U;
  const auto add = [&](const Rect &rect, WeightAction action, size_t row = 0U) {
    if (out != nullptr && count < capacity) out[count] = {rect, action, row};
    ++count;
  };
  add(prototype_ui::header_action_rect(width, height), WeightAction::Back);
  if (page == WeightPage::Dashboard) {
    add(unit(width, height), WeightAction::ToggleUnit);
    add(trend_toggle(width, height), WeightAction::ToggleTrend);
    const WeightAction actions[] = {WeightAction::AddToday, WeightAction::History, WeightAction::Profile};
    for (size_t i = 0U; i < 3U; ++i) add(footer(i, 3U, width, height), actions[i]);
  } else if (page == WeightPage::History) {
    for (size_t i = 0U; i < kWeightHistoryRows; ++i)
      add(history_row(i, width, height), WeightAction::SelectRecord, i);
    add(footer(0U, 2U, width, height), WeightAction::Previous);
    add(footer(1U, 2U, width, height), WeightAction::Next);
  } else if (page == WeightPage::Editor) {
    const WeightAction actions[] = {WeightAction::MinusOne, WeightAction::MinusTenth, WeightAction::PlusTenth, WeightAction::PlusOne};
    for (size_t i = 0U; i < 4U; ++i) add(adjustment(i, width, height), actions[i]);
    add(footer(0U, 2U, width, height), WeightAction::Delete);
    add(footer(1U, 2U, width, height), WeightAction::Save);
  } else if (page == WeightPage::Profile) {
    const WeightAction actions[] = {WeightAction::HeightDownBig, WeightAction::HeightDown, WeightAction::HeightUp, WeightAction::HeightUpBig,
                                    WeightAction::BaselineDownBig, WeightAction::BaselineDown, WeightAction::BaselineUp, WeightAction::BaselineUpBig};
    for (size_t i = 0U; i < 8U; ++i) add(profile_adjust(i, width, height), actions[i]);
    add(footer(0U, 1U, width, height), WeightAction::Save);
  } else {
    add(footer(0U, 2U, width, height), WeightAction::Cancel);
    add(footer(1U, 2U, width, height), WeightAction::Confirm);
  }
  return count < capacity ? count : capacity;
}

WeightHit weight_page_action_at(WeightPage page, int width, int height, int x, int y) {
  WeightTarget targets[16];
  const size_t count = weight_page_targets(page, width, height, targets, 16U);
  for (size_t i = 0U; i < count; ++i)
    if (targets[i].rect.contains(x, y)) return {targets[i].action, targets[i].row};
  return {};
}

void weight_page_render(Canvas &canvas, const WeightService &service,
                        const WeightProfileSettings &settings, const WeightPageState &state) {
  prototype_ui::begin_page(canvas);
  const int width = canvas.width(), height = canvas.height();
  const bool vertical = portrait(width, height);
  const bool pounds = settings.use_pounds;
  char text[80] = {};
  if (state.page == WeightPage::Dashboard) {
    prototype_ui::draw_header(canvas, "WEIGHT TREND", "FOCUS ON THE LONG-TERM TREND", "APPS");
    segmented_toggle(canvas, unit(width, height), "KG", "LB", !pounds);
    const auto *latest = service.latest();
    if (latest) {
      const auto shown = weight_display_value(latest->weight_tenths_kg, pounds);
      std::snprintf(text, sizeof(text), "%u.%u", shown / 10U, shown % 10U);
    } else std::snprintf(text, sizeof(text), "--.-");
    const DashboardLayout layout = dashboard_layout(width, height);
    canvas.draw_text(ui_tokens::kContentMargin, layout.value_y, text, 8, GrayLevel::Black);
    canvas.draw_text(layout.unit_x, layout.unit_y, unit_name(pounds), 3, GrayLevel::Black);
    WeightSummary summary = {};
    if (service.summary(settings.height_cm, settings.baseline_tenths_kg, summary)) {
      const int change = pounds ? static_cast<int>(summary.change_tenths_kg) * 22046 / 10000 : summary.change_tenths_kg;
      std::snprintf(text, sizeof(text), "%s%d.%d %s", change < 0 ? "-" : "+", std::abs(change) / 10, std::abs(change) % 10, unit_name(pounds));
      canvas.draw_text(ui_tokens::kContentMargin, layout.baseline_label_y, ui_text("FROM BASELINE"), 1, GrayLevel::Black);
      canvas.draw_text(ui_tokens::kContentMargin, layout.baseline_value_y, text, 3, GrayLevel::Black);
    }
    canvas.draw_text(layout.trend_title_x, layout.trend_title_y, ui_text("WEIGHT TREND"), 2, GrayLevel::Black);
    segmented_toggle(canvas, trend_toggle(width, height), "7 DAYS", "30 DAYS", state.trend_days == 7U);
    draw_trend(canvas, service, state);
    std::snprintf(text, sizeof(text), ui_language_is_chinese() ? "身高 %u CM   记录 %u / 366" : "HEIGHT %u CM   RECORDS %u / 366", settings.height_cm, static_cast<unsigned>(service.count()));
    canvas.draw_text(ui_tokens::kContentMargin, layout.footer_text_y, text, 1, GrayLevel::Black);
    const char *labels[] = {"ADD TODAY", "HISTORY", "BASELINE"};
    for (size_t i = 0U; i < 3U; ++i) prototype_ui::draw_button(canvas, footer(i, 3U, width, height), labels[i], i == 0U);
  } else if (state.page == WeightPage::History) {
    prototype_ui::draw_header(canvas, "WEIGHT HISTORY", "SELECT A RECORD TO EDIT", "BACK");
    for (size_t row = 0U; row < kWeightHistoryRows; ++row) {
      const auto *record = service.history_at(state.history_offset + row);
      if (!record) continue;
      char date[16] = {};
      date_text(record->date, date, sizeof(date));
      const auto value = weight_display_value(record->weight_tenths_kg, pounds);
      std::snprintf(text, sizeof(text), "%s     %u.%u %s", date, value / 10U, value % 10U, unit_name(pounds));
      prototype_ui::draw_button(canvas, history_row(row, width, height), text);
    }
    if (service.count() == 0U) canvas.draw_text(24, 154, ui_text("NO WEIGHT RECORDS"), 2, GrayLevel::Black);
    prototype_ui::draw_button(canvas, footer(0U, 2U, width, height), "PREVIOUS PAGE");
    prototype_ui::draw_button(canvas, footer(1U, 2U, width, height), "NEXT PAGE");
  } else if (state.page == WeightPage::Editor) {
    date_text(state.edit_date, text, sizeof(text));
    prototype_ui::draw_header(canvas, "WEIGHT", text, "CANCEL");
    std::snprintf(text, sizeof(text), "%u.%u %s", state.edit_display_weight / 10U, state.edit_display_weight % 10U, unit_name(pounds));
    canvas.draw_text((width - ui_text_width(text, 5U)) / 2, vertical ? 200 : 170, text, 5, GrayLevel::Black);
    const char *labels[] = {"- 1.0", "- 0.1", "+ 0.1", "+ 1.0"};
    for (size_t i = 0U; i < 4U; ++i) prototype_ui::draw_button(canvas, adjustment(i, width, height), labels[i]);
    if (state.edit_id != 0U) prototype_ui::draw_button(canvas, footer(0U, 2U, width, height), "DELETE");
    prototype_ui::draw_button(canvas, footer(1U, 2U, width, height), "SAVE", true);
  } else if (state.page == WeightPage::Profile) {
    prototype_ui::draw_header(canvas, "BASELINE", "PERSONAL RECORD", "CANCEL");
    std::snprintf(text, sizeof(text), "%u CM", state.profile.height_cm);
    canvas.draw_text(24, vertical ? 170 : 114, text, 3, GrayLevel::Black);
    const auto baseline = weight_display_value(state.profile.baseline_tenths_kg, pounds);
    std::snprintf(text, sizeof(text), "%u.%u %s", baseline / 10U, baseline % 10U, unit_name(pounds));
    canvas.draw_text(24, vertical ? 358 : 234, text, 3, GrayLevel::Black);
    const char *labels[] = {"- 5", "- 1", "+ 1", "+ 5", "- 1.0", "- 0.1", "+ 0.1", "+ 1.0"};
    for (size_t i = 0U; i < 8U; ++i) prototype_ui::draw_button(canvas, profile_adjust(i, width, height), labels[i]);
    prototype_ui::draw_button(canvas, footer(0U, 1U, width, height), "SAVE", true);
  } else {
    const bool deleting = state.page == WeightPage::DeleteConfirm;
    prototype_ui::draw_header(canvas, deleting ? "DELETE WEIGHT RECORD" : "REPLACE SAME-DAY RECORD", "CONFIRM CHANGE", "BACK");
    date_text(state.edit_date, text, sizeof(text));
    canvas.draw_text(24, vertical ? 184 : 154, text, 3, GrayLevel::Black);
    ui_draw_wrapped_text_pixels(canvas, ui_tokens::kContentMargin, vertical ? 244 : 210, width - 2 * ui_tokens::kContentMargin,
        ui_text(deleting ? "THIS RECORD WILL BE DELETED." : "THE EXISTING RECORD FOR THIS DATE WILL BE REPLACED."),
        ui_tokens::kBody, GrayLevel::Black, 3, 5, FontWeight::Regular);
    prototype_ui::draw_button(canvas, footer(0U, 2U, width, height), "CANCEL");
    prototype_ui::draw_button(canvas, footer(1U, 2U, width, height), "CONFIRM", true);
  }
}
