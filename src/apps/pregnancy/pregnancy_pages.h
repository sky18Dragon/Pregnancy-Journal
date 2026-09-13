#pragma once

#include <cstdint>

#include "pregnancy_state.h"
#include "content_service.h"

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
    Edit,
    SelectDueDate,
    SelectLmp,
    ShowOverview,
    ShowBaby,
    ShowMom,
};

void pregnancy_page_render_clock_setup(Canvas &canvas,
                                       const char *digits,
                                       bool input_error,
                                       bool can_cancel);
void pregnancy_page_render_due_date_setup(Canvas &canvas,
                                          const char *digits,
                                          bool input_error,
                                          bool can_cancel);
void pregnancy_page_render_source_setup(Canvas &canvas, bool can_cancel);
void pregnancy_page_render_profile_date_setup(Canvas &canvas,
                                                const char *digits,
                                                bool input_error,
                                                bool due_date_primary);
void pregnancy_page_render_dashboard(Canvas &canvas,
                                     const PregnancyDate &due_date,
                                     const PregnancyProgress &progress);
void pregnancy_page_render_detail(Canvas &canvas,
                                  PregnancyPage page,
                                  const PregnancyProgress &progress,
                                  const WeekContent &content);

PregnancyAction pregnancy_page_action_at(PregnancyPage page,
                                         bool can_cancel,
                                         int x,
                                         int y);
bool pregnancy_action_digit(PregnancyAction action, char &digit);

const char *pregnancy_page_name(PregnancyPage page);
const char *pregnancy_action_name(PregnancyAction action);
