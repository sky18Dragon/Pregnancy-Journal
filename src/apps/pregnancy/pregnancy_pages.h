#pragma once

#include <cstddef>
#include <cstdint>

#include "content_service.h"
#include "digit_fields.h"
#include "pregnancy_state.h"
#include "key_policy.h"
#include "prototype_ui.h"

class Canvas;

enum class PregnancyPage : uint8_t {
  ClockSetup,
  SourceSetup,
  DateSetup,
  DueDateSetup = DateSetup,
  Dashboard,
  Baby,
  Mom,
};

// How the hardware keys act on `page`. Dashboard is the home page; the setup
// pages are a required flow with no home to go back to.
// 各页对硬件按键的反应；Dashboard 是首页；设置向导页没有首页可回，忽略所有按键。
inline KeyPolicy pregnancy_page_key_policy(PregnancyPage page) {
  switch (page) {
  case PregnancyPage::Dashboard:
    return KeyPolicy::Home;
  case PregnancyPage::Baby:
  case PregnancyPage::Mom:
    return KeyPolicy::Free;
  case PregnancyPage::ClockSetup:
  case PregnancyPage::SourceSetup:
  case PregnancyPage::DateSetup:
    return KeyPolicy::Wizard;
  }
  return KeyPolicy::Wizard;
}

// Page changes that are trialled with the partial waveform instead of the full
// one (no black flash, ghosting is cleaned up later by an idle full refresh).
// Only the Dashboard <-> Baby pair; every other change keeps the full refresh.
// 试验：首页与宝宝页互相切换用局刷（不闪黑），残影由空闲时的一次全刷清理；其余换页仍全刷。
inline bool pregnancy_page_change_uses_partial(PregnancyPage from,
                                               PregnancyPage to) {
  return (from == PregnancyPage::Dashboard && to == PregnancyPage::Baby) ||
         (from == PregnancyPage::Baby && to == PregnancyPage::Dashboard);
}

enum class PregnancyAction : uint8_t {
  None,
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
  Delete,
  Continue,
  Back,
  SelectDueDate,
  SelectLmp,
  ShowOverview,
  ShowBaby,
  ShowMom,
  OpenLauncher,
  Field0, // setup pages: select the year / month / day / hour / minute
  Field1, // field to edit (see pregnancy_action_field)
  Field2,
  Field3,
  Field4,
};

void pregnancy_page_render_clock_setup(Canvas &canvas,
                                       const DigitFields &fields,
                                       bool input_error, bool can_cancel);
void pregnancy_page_render_due_date_setup(Canvas &canvas,
                                          const DigitFields &fields,
                                          bool input_error, bool can_cancel);
void pregnancy_page_render_source_setup(Canvas &canvas, bool can_cancel);
void pregnancy_page_render_profile_date_setup(Canvas &canvas,
                                              const DigitFields &fields,
                                              bool input_error,
                                              bool due_date_primary);
// The dashboard caption under the picture, for `week` (6-40; others clamp): a sentence that names
// the plate's object and changes every 8 weeks. Chinese is one line; English is two lines separated
// by '\n' (a fixed break after "of"). Needs kCaptionBufferSize bytes.
constexpr size_t kCaptionBufferSize = 96;
void pregnancy_page_caption_text(uint8_t week, bool chinese, char *out, size_t size);
void pregnancy_page_render_dashboard(
    Canvas &canvas, const PregnancyDate &due_date,
    const PregnancyProgress &progress);
// Dashboard status row. The power indicator hugs the top-left corner (a status glyph, so it may sit
// inside the 24 px safe area like the system overlay does) and the menu icon stays in the top-right;
// `x`/`y` are the top-left for battery_status_overlay_draw_compact(). Both orientations use it.
void pregnancy_page_status_battery_origin(bool portrait, int &x, int &y);
// Sleep frame: the moon follows the power indicator, just to its right. The menu icon is left alone.
void pregnancy_page_render_sleep_moon(Canvas &canvas);
void pregnancy_page_render_detail(Canvas &canvas, PregnancyPage page,
                                  const PregnancyProgress &progress,
                                  const WeekContent &content);

// Upper bound on the targets of any page (digit pages: 5 fields + 12 keys + back).
constexpr size_t kMaxPregnancyTargets = 24U;

// One tappable rectangle. The renderers draw these same rectangles.
struct PregnancyTarget {
  prototype_ui::Rect rect;
  PregnancyAction action;
};

// Lists the active hit targets of `page` for the given logical size (at most
// `capacity` are written; the return value is the number written). The hit
// test is defined as the first target containing the point.
size_t pregnancy_page_targets(PregnancyPage page, bool can_cancel, int width,
                              int height, PregnancyTarget *out,
                              size_t capacity);

PregnancyAction pregnancy_page_action_at(PregnancyPage page, bool can_cancel,
                                         int x, int y);
PregnancyAction pregnancy_page_action_at(PregnancyPage page, bool can_cancel,
                                         int width, int height, int x, int y);
bool pregnancy_action_digit(PregnancyAction action, char &digit);
// Maps Field0..Field4 to the field index 0..4.
bool pregnancy_action_field(PregnancyAction action, size_t &field);

const char *pregnancy_page_name(PregnancyPage page);
const char *pregnancy_action_name(PregnancyAction action);
