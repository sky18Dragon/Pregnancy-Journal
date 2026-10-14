#include "chinese_input_page.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "assets/chinese_input_font_assets.h"
#include "font.h"
#include "ui_tokens.h"

namespace sticky_ui {
namespace {

using prototype_ui::Rect;

constexpr int kGap = 8;
constexpr int kCompactGap = 4;
constexpr int kHeaderTop = 8;
constexpr int kHeaderButtonWidth = 96;
constexpr int kKeyHeight = 48;
constexpr int kCandidateHeight = 52;

constexpr char kLandscapeRows[][11] = {"qwertyuiop", "asdfghjkl", "zxcvbnm"};
constexpr char kPortraitRows[][7] = {"abcdef", "ghijkl", "mnopqr", "stuvwx",
                                     "yz"};

bool is_portrait(int width, int height) { return height > width; }

Rect empty_rect() { return {0, 0, 0, 0}; }

Rect header_rect(ChineseInputAction action, int width)
{
    if (action == ChineseInputAction::Save) {
        return {width - ui_tokens::kContentMargin - kHeaderButtonWidth,
                kHeaderTop, kHeaderButtonWidth,
                ui_tokens::kCompactButtonHeight};
    }
    if (action == ChineseInputAction::Cancel) {
        return {width - ui_tokens::kContentMargin - kHeaderButtonWidth * 2 -
                    kGap,
                kHeaderTop, kHeaderButtonWidth,
                ui_tokens::kCompactButtonHeight};
    }
    return empty_rect();
}

Rect composition_rect(int width, int height)
{
    if (is_portrait(width, height)) {
        return {ui_tokens::kContentMargin, 240, 224, kKeyHeight};
    }
    return {ui_tokens::kContentMargin, 172, width - 152, kKeyHeight};
}

Rect portrait_candidate_rect(size_t index)
{
    constexpr int kWidth = 96;
    return {ui_tokens::kContentMargin +
                static_cast<int>(index % 4U) * (kWidth + kGap),
            296 + static_cast<int>(index / 4U) * (kKeyHeight + kGap),
            kWidth, kKeyHeight};
}

Rect landscape_candidate_rect(size_t index)
{
    constexpr int kWidth = 76;
    return {80 + static_cast<int>(index) * (kWidth + kCompactGap), 228,
            kWidth, kCandidateHeight};
}

Rect candidate_rect(size_t index, int width, int height)
{
    return is_portrait(width, height) ? portrait_candidate_rect(index)
                                      : landscape_candidate_rect(index);
}

Rect landscape_letter_rect(char letter)
{
    for (size_t row = 0U; row < 3U; ++row) {
        const char *found = std::strchr(kLandscapeRows[row], letter);
        if (found == nullptr) {
            continue;
        }
        const int key_width = 56;
        const int count = static_cast<int>(std::strlen(kLandscapeRows[row]));
        const int row_width = count * key_width + (count - 1) * kGap;
        const int start_x = row == 2U ? 128 : (800 - row_width) / 2;
        return {start_x + static_cast<int>(found - kLandscapeRows[row]) *
                              (key_width + kGap),
                292 + static_cast<int>(row) * (kKeyHeight + kGap), key_width,
                kKeyHeight};
    }
    return empty_rect();
}

Rect portrait_letter_rect(char letter)
{
    for (size_t row = 0U; row < 5U; ++row) {
        const char *found = std::strchr(kPortraitRows[row], letter);
        if (found == nullptr) {
            continue;
        }
        return {28 + static_cast<int>(found - kPortraitRows[row]) * 72,
                416 + static_cast<int>(row) * (kKeyHeight + kGap), 64,
                kKeyHeight};
    }
    return empty_rect();
}

Rect letter_rect(char letter, int width, int height)
{
    return is_portrait(width, height) ? portrait_letter_rect(letter)
                                      : landscape_letter_rect(letter);
}

void draw_input_glyph(Canvas &canvas, int x, int y,
                      const ChineseFontStrike &strike,
                      const ChineseFontGlyph &glyph, GrayLevel color)
{
    const size_t stride = (static_cast<size_t>(glyph.width) + 7U) / 8U;
    const int baseline = y + strike.ascent;
    for (uint8_t row = 0U; row < glyph.height; ++row) {
        for (uint8_t column = 0U; column < glyph.width; ++column) {
            const uint8_t packed = strike.bitmap[
                glyph.bitmap_offset + static_cast<size_t>(row) * stride +
                column / 8U];
            if ((packed & (0x80U >> (column & 7U))) != 0U) {
                canvas.draw_pixel(x + glyph.bearing_x + column,
                                  baseline + glyph.bearing_y + row, color);
            }
        }
    }
}

int glyph_advance(const ChineseFontStrike &strike, uint32_t codepoint)
{
    const ChineseFontGlyph *glyph =
        chinese_input_font_resolve_glyph(strike, codepoint);
    return glyph == nullptr ? std::max<int>(1, strike.pixel_size / 2)
                            : glyph->advance;
}

size_t encode_utf8(uint32_t codepoint, char output[5])
{
    if (codepoint <= 0x7FU) {
        output[0] = static_cast<char>(codepoint);
        output[1] = '\0';
        return 1U;
    }
    if (codepoint <= 0x7FFU) {
        output[0] = static_cast<char>(0xC0U | (codepoint >> 6U));
        output[1] = static_cast<char>(0x80U | (codepoint & 0x3FU));
        output[2] = '\0';
        return 2U;
    }
    output[0] = static_cast<char>(0xE0U | (codepoint >> 12U));
    output[1] = static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU));
    output[2] = static_cast<char>(0x80U | (codepoint & 0x3FU));
    output[3] = '\0';
    return 3U;
}

void draw_centered(Canvas &canvas, const Rect &rect, const char *label,
                   uint8_t pixel_size, GrayLevel color)
{
    const int width = chinese_input_text_width(label, pixel_size);
    const int x = rect.x + std::max(0, (rect.width - width) / 2);
    const int y = rect.y + std::max(0, (rect.height - pixel_size) / 2 - 1);
    chinese_input_draw_text(canvas, x, y, label, pixel_size, color);
}

void draw_input_button(Canvas &canvas, const Rect &rect, const char *label,
                       bool filled = false, uint8_t pixel_size = 18U)
{
    if (filled) {
        canvas.fill_rect(rect.x, rect.y, rect.width, rect.height,
                         GrayLevel::Black);
    } else {
        canvas.draw_rect(rect.x, rect.y, rect.width, rect.height,
                         GrayLevel::Black);
    }
    draw_centered(canvas, rect, label, pixel_size,
                  filled ? GrayLevel::White : GrayLevel::Black);
}

void draw_text_body(Canvas &canvas, const Rect &box,
                    const ChineseTextEditor &editor, int max_lines)
{
    const ChineseFontStrike &strike = chinese_input_font_strike(18U);
    const int start_x = box.x + 12;
    int cursor_x = start_x;
    int cursor_y = box.y + 10;
    const int right = box.x + box.width - 12;
    const int line_height = 28;
    int line = 0;
    const char *text = editor.text();
    uint32_t codepoint = 0U;
    while (line < max_lines && font_decode_utf8(text, codepoint)) {
        const int advance = glyph_advance(strike, codepoint) + 1;
        if (cursor_x + advance > right) {
            ++line;
            if (line >= max_lines) {
                break;
            }
            cursor_x = start_x;
            cursor_y += line_height;
        }
        const ChineseFontGlyph *glyph =
            chinese_input_font_resolve_glyph(strike, codepoint);
        if (glyph != nullptr) {
            draw_input_glyph(canvas, cursor_x, cursor_y, strike, *glyph,
                             GrayLevel::Black);
        }
        cursor_x += advance;
    }
    if (editor.empty()) {
        chinese_input_draw_text(canvas, start_x, cursor_y, "在这里输入记录",
                                18U, GrayLevel::Black);
    }
}

void draw_counter(Canvas &canvas, const Rect &box,
                  const ChineseTextEditor &editor, bool limit_reached)
{
    char counter[24] = {};
    std::snprintf(counter, sizeof(counter), "%u/200",
                  static_cast<unsigned>(editor.codepoint_count()));
    const int width = chinese_input_text_width(counter, 18U);
    const int y = box.y + box.height - 27;
    chinese_input_draw_text(canvas, box.x + box.width - width - 12, y, counter,
                            18U, GrayLevel::Black);
    if (limit_reached) {
        chinese_input_draw_text(canvas, box.x + 12, y, "已达上限", 18U,
                                GrayLevel::Black);
    }
}

void draw_composition(Canvas &canvas, const ChineseTextEditor &editor,
                      int width, int height)
{
    const Rect box = composition_rect(width, height);
    canvas.draw_rect(box.x, box.y, box.width, box.height, GrayLevel::Black);
    chinese_input_draw_text(canvas, box.x + 12, box.y + 12, "拼音", 18U,
                            GrayLevel::Black);
    chinese_input_draw_text(canvas, box.x + 68, box.y + 8,
                            editor.pinyin_bytes() == 0U ? "-" : editor.pinyin(),
                            24U, GrayLevel::Black);
    char page[16] = {};
    const size_t page_count = editor.candidate_page_count();
    std::snprintf(page, sizeof(page), "%u/%u",
                  static_cast<unsigned>(page_count == 0U
                                            ? 0U
                                            : editor.candidate_page() + 1U),
                  static_cast<unsigned>(page_count));
    const int page_width = chinese_input_text_width(page, 18U);
    chinese_input_draw_text(canvas, box.x + box.width - page_width - 12,
                            box.y + 12, page, 18U, GrayLevel::Black);
    draw_input_button(
        canvas,
        chinese_input_page_action_rect(ChineseInputAction::DeletePinyin, width,
                                       height),
        "删音");
    if (is_portrait(width, height)) {
        draw_input_button(
            canvas,
            chinese_input_page_action_rect(
                ChineseInputAction::PreviousCandidates, width, height),
            "<", false, 24U);
        draw_input_button(
            canvas,
            chinese_input_page_action_rect(ChineseInputAction::NextCandidates,
                                           width, height),
            ">", false, 24U);
    }
}

void draw_candidates(Canvas &canvas, const ChineseTextEditor &editor, int width,
                     int height)
{
    if (!is_portrait(width, height)) {
        draw_input_button(
            canvas,
            chinese_input_page_action_rect(
                ChineseInputAction::PreviousCandidates, width, height),
            "<", false, 24U);
        draw_input_button(
            canvas,
            chinese_input_page_action_rect(ChineseInputAction::NextCandidates,
                                           width, height),
            ">", false, 24U);
    }
    for (size_t index = 0U; index < kChineseEditorCandidatesPerPage; ++index) {
        const Rect rect = candidate_rect(index, width, height);
        canvas.draw_rect(rect.x, rect.y, rect.width, rect.height,
                         GrayLevel::Black);
        const uint16_t codepoint = editor.candidate_at(index);
        if (codepoint == 0U) {
            continue;
        }
        char utf8[5] = {};
        encode_utf8(codepoint, utf8);
        draw_centered(canvas, rect, utf8, 24U, GrayLevel::Black);
    }
}

void draw_keyboard(Canvas &canvas, int width, int height)
{
    for (char letter = 'a'; letter <= 'z'; ++letter) {
        const Rect rect = letter_rect(letter, width, height);
        char label[2] = {letter, '\0'};
        draw_input_button(canvas, rect, label);
    }
    draw_input_button(
        canvas,
        chinese_input_page_action_rect(ChineseInputAction::DeleteText, width,
                                       height),
        "退格");
}

}  // namespace

int chinese_input_text_width(const char *utf8, uint8_t pixel_size)
{
    if (utf8 == nullptr) {
        return 0;
    }
    const ChineseFontStrike &strike = chinese_input_font_strike(pixel_size);
    int width = 0;
    const char *text = utf8;
    uint32_t codepoint = 0U;
    while (font_decode_utf8(text, codepoint)) {
        width += glyph_advance(strike, codepoint) + 1;
    }
    return width > 0 ? width - 1 : 0;
}

void chinese_input_draw_text(Canvas &canvas, int x, int y, const char *utf8,
                             uint8_t pixel_size, GrayLevel color)
{
    if (utf8 == nullptr) {
        return;
    }
    const ChineseFontStrike &strike = chinese_input_font_strike(pixel_size);
    const char *text = utf8;
    uint32_t codepoint = 0U;
    int cursor_x = x;
    while (font_decode_utf8(text, codepoint)) {
        const ChineseFontGlyph *glyph =
            chinese_input_font_resolve_glyph(strike, codepoint);
        if (glyph != nullptr) {
            draw_input_glyph(canvas, cursor_x, y, strike, *glyph, color);
        }
        cursor_x += glyph_advance(strike, codepoint) + 1;
    }
}

Rect chinese_input_page_action_rect(ChineseInputAction action, int width,
                                    int height)
{
    if (action == ChineseInputAction::Cancel ||
        action == ChineseInputAction::Save) {
        return header_rect(action, width);
    }
    const bool portrait = is_portrait(width, height);
    if (action == ChineseInputAction::DeletePinyin) {
        return portrait ? Rect{368, 240, 88, kKeyHeight}
                        : Rect{width - ui_tokens::kContentMargin - 96, 172, 96,
                               kKeyHeight};
    }
    if (action == ChineseInputAction::PreviousCandidates) {
        return portrait ? Rect{256, 240, 48, kKeyHeight}
                        : Rect{24, 228, 48, kCandidateHeight};
    }
    if (action == ChineseInputAction::NextCandidates) {
        return portrait ? Rect{312, 240, 48, kKeyHeight}
                        : Rect{728, 228, 48, kCandidateHeight};
    }
    if (action >= ChineseInputAction::Candidate0 &&
        action <= ChineseInputAction::Candidate7) {
        const size_t index = static_cast<uint8_t>(action) -
                             static_cast<uint8_t>(ChineseInputAction::Candidate0);
        return candidate_rect(index, width, height);
    }
    char letter = 0;
    if (chinese_input_action_letter(action, letter)) {
        return letter_rect(letter, width, height);
    }
    if (action == ChineseInputAction::DeleteText) {
        return portrait ? Rect{172, 640, 136, kKeyHeight}
                        : Rect{584, 404, 88, kKeyHeight};
    }
    return empty_rect();
}

ChineseInputAction chinese_input_page_action_at(int width, int height, int x,
                                                int y)
{
    const auto first = static_cast<uint8_t>(ChineseInputAction::Cancel);
    const auto last = static_cast<uint8_t>(ChineseInputAction::DeleteText);
    for (uint8_t value = first; value <= last; ++value) {
        const auto action = static_cast<ChineseInputAction>(value);
        const Rect rect = chinese_input_page_action_rect(action, width, height);
        if (rect.width > 0 && rect.height > 0 && rect.contains(x, y)) {
            return action;
        }
    }
    return ChineseInputAction::None;
}

bool chinese_input_action_letter(ChineseInputAction action, char &letter)
{
    if (action < ChineseInputAction::LetterA ||
        action > ChineseInputAction::LetterZ) {
        return false;
    }
    letter = static_cast<char>(
        'a' + static_cast<uint8_t>(action) -
        static_cast<uint8_t>(ChineseInputAction::LetterA));
    return true;
}

bool chinese_input_action_candidate(ChineseInputAction action, size_t &index)
{
    if (action < ChineseInputAction::Candidate0 ||
        action > ChineseInputAction::Candidate7) {
        return false;
    }
    index = static_cast<uint8_t>(action) -
            static_cast<uint8_t>(ChineseInputAction::Candidate0);
    return true;
}

ChineseEditorResult chinese_input_page_apply_action(ChineseTextEditor &editor,
                                                    ChineseInputAction action)
{
    char letter = 0;
    if (chinese_input_action_letter(action, letter)) {
        return editor.type_pinyin(letter);
    }
    size_t index = 0U;
    if (chinese_input_action_candidate(action, index)) {
        return editor.choose_candidate(index);
    }
    switch (action) {
    case ChineseInputAction::DeletePinyin:
        return editor.erase_pinyin_letter() ? ChineseEditorResult::Ok
                                            : ChineseEditorResult::InvalidInput;
    case ChineseInputAction::PreviousCandidates:
        return editor.previous_candidate_page()
                   ? ChineseEditorResult::Ok
                   : ChineseEditorResult::CandidateUnavailable;
    case ChineseInputAction::NextCandidates:
        return editor.next_candidate_page()
                   ? ChineseEditorResult::Ok
                   : ChineseEditorResult::CandidateUnavailable;
    case ChineseInputAction::DeleteText:
        return editor.erase_last_codepoint() ? ChineseEditorResult::Ok
                                             : ChineseEditorResult::InvalidInput;
    default:
        return ChineseEditorResult::InvalidInput;
    }
}

void chinese_input_page_render(Canvas &canvas, const ChineseTextEditor &editor,
                               bool limit_reached, bool saved)
{
    canvas.clear(GrayLevel::White);
    const int width = canvas.width();
    const int height = canvas.height();
    chinese_input_draw_text(canvas, ui_tokens::kContentMargin, 18, "记录备忘",
                            24U, GrayLevel::Black);
    draw_input_button(
        canvas,
        chinese_input_page_action_rect(ChineseInputAction::Cancel, width,
                                       height),
        "取消");
    draw_input_button(
        canvas,
        chinese_input_page_action_rect(ChineseInputAction::Save, width, height),
        saved ? "已保存" : "保存", true);

    const Rect text_box = is_portrait(width, height)
                              ? Rect{24, 72, width - 48, 160}
                              : Rect{24, 72, width - 48, 92};
    canvas.draw_rect(text_box.x, text_box.y, text_box.width, text_box.height,
                     GrayLevel::Black);
    draw_text_body(canvas, text_box, editor,
                   is_portrait(width, height) ? 4 : 2);
    draw_counter(canvas, text_box, editor, limit_reached);
    draw_composition(canvas, editor, width, height);
    draw_candidates(canvas, editor, width, height);
    draw_keyboard(canvas, width, height);
}

}  // namespace sticky_ui
