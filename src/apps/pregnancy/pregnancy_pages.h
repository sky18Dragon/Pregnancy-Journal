#pragma once

#include <cstdint>

#include "pregnancy_state.h"

class Canvas;

enum class PregnancyPage : uint8_t {
    ClockSetup,
    DueDateSetup,
    Dashboard,
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
};

void pregnancy_page_render_clock_setup(Canvas &canvas,
                                       const char *digits,
                                       bool input_error,
                                       bool can_cancel);
void pregnancy_page_render_due_date_setup(Canvas &canvas,
                                          const char *digits,
                                          bool input_error,
                                          bool can_cancel);
void pregnancy_page_render_dashboard(Canvas &canvas,
                                     const PregnancyDate &due_date,
                                     const PregnancyProgress &progress);

PregnancyAction pregnancy_page_action_at(PregnancyPage page,
                                         bool can_cancel,
                                         int x,
                                         int y);
bool pregnancy_action_digit(PregnancyAction action, char &digit);

const char *pregnancy_page_name(PregnancyPage page);
const char *pregnancy_action_name(PregnancyAction action);
