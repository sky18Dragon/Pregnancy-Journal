#pragma once

#include <cstddef>
#include <cstdint>

#include "canvas.h"
#include "chinese_text_editor.h"
#include "prototype_ui.h"

namespace sticky_ui {

enum class ChineseInputAction : uint8_t {
    None = 0,
    Cancel,
    Save,
    DeletePinyin,
    PreviousCandidates,
    NextCandidates,
    Candidate0,
    Candidate1,
    Candidate2,
    Candidate3,
    Candidate4,
    Candidate5,
    Candidate6,
    Candidate7,
    LetterA,
    LetterB,
    LetterC,
    LetterD,
    LetterE,
    LetterF,
    LetterG,
    LetterH,
    LetterI,
    LetterJ,
    LetterK,
    LetterL,
    LetterM,
    LetterN,
    LetterO,
    LetterP,
    LetterQ,
    LetterR,
    LetterS,
    LetterT,
    LetterU,
    LetterV,
    LetterW,
    LetterX,
    LetterY,
    LetterZ,
    DeleteText,
};

// Renders the isolated S0 Chinese input prototype. It only mutates the shared
// Canvas and never refreshes the display or reads hardware.
void chinese_input_page_render(Canvas &canvas, const ChineseTextEditor &editor,
                               bool limit_reached = false,
                               bool saved = false);

// Visual and touch geometry share this exact layout function. Empty rectangles
// identify actions that are not represented on the current page.
prototype_ui::Rect chinese_input_page_action_rect(ChineseInputAction action,
                                                   int width, int height);
ChineseInputAction chinese_input_page_action_at(int width, int height, int x,
                                                 int y);

bool chinese_input_action_letter(ChineseInputAction action, char &letter);
bool chinese_input_action_candidate(ChineseInputAction action, size_t &index);
ChineseEditorResult chinese_input_page_apply_action(ChineseTextEditor &editor,
                                                    ChineseInputAction action);

// Dedicated raw UTF-8 renderer backed by the GB2312 Level-1 input font. It does
// not pass strings through the product translation table.
int chinese_input_text_width(const char *utf8, uint8_t pixel_size);
void chinese_input_draw_text(Canvas &canvas, int x, int y, const char *utf8,
                             uint8_t pixel_size,
                             GrayLevel color = GrayLevel::Black);

}  // namespace sticky_ui
