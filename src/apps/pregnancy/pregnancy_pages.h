#pragma once

#include <cstddef>
#include <cstdint>

#include "content_service.h"
#include "digit_fields.h"
#include "pregnancy_state.h"
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
void pregnancy_page_render_dashboard(
    Canvas &canvas, const PregnancyDate &due_date,
    const PregnancyProgress &progress);
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
