#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>

#include "chinese_text_editor.h"
#include "storage_contract.h"

using sticky_ui::ChineseEditorResult;
using sticky_ui::ChineseTextEditor;

int main()
{
    static_assert(sticky_ui::kChineseEditorMaximumUtf8Bytes ==
                  sticky_storage::kMaxUserTextBytes);
    assert(sticky_ui::chinese_text_editor_state_bytes() <= 1024U);
    ChineseTextEditor editor;
    assert(editor.empty());
    assert(editor.type_pinyin('z') == ChineseEditorResult::Ok);
    assert(editor.type_pinyin('h') == ChineseEditorResult::Ok);
    assert(editor.type_pinyin('o') == ChineseEditorResult::Ok);
    assert(editor.type_pinyin('n') == ChineseEditorResult::Ok);
    assert(editor.type_pinyin('g') == ChineseEditorResult::Ok);
    assert(editor.candidate_count() > 0U);
    size_t middle_index = 0U;
    while (middle_index < sticky_ui::kChineseEditorCandidatesPerPage &&
           editor.candidate_at(middle_index) != 0x4E2DU) {
        ++middle_index;
    }
    assert(middle_index < sticky_ui::kChineseEditorCandidatesPerPage);
    assert(editor.choose_candidate(middle_index) == ChineseEditorResult::Ok);
    assert(std::strcmp(editor.text(), "中") == 0);
    assert(editor.codepoint_count() == 1U && editor.pinyin_bytes() == 0U);

    for (char letter : std::string("shi")) {
        assert(editor.type_pinyin(letter) == ChineseEditorResult::Ok);
    }
    assert(editor.candidate_page_count() > 1U);
    assert(editor.next_candidate_page());
    assert(editor.candidate_page() == 1U);
    assert(editor.previous_candidate_page());
    assert(!editor.previous_candidate_page());
    editor.clear_pinyin();
    assert(editor.choose_candidate(0U) ==
           ChineseEditorResult::CandidateUnavailable);

    assert(editor.type_pinyin('A') == ChineseEditorResult::InvalidInput);
    for (char letter : std::string("abcdef")) {
        assert(editor.type_pinyin(letter) == ChineseEditorResult::Ok);
    }
    assert(editor.type_pinyin('g') == ChineseEditorResult::Full);
    assert(editor.erase_pinyin_letter());
    editor.clear_pinyin();

    editor.clear_text();
    for (size_t index = 0U;
         index < sticky_ui::kChineseEditorMaximumCodepoints; ++index) {
        assert(editor.append_codepoint(0x1F600U) == ChineseEditorResult::Ok);
    }
    assert(editor.text_bytes() == 800U);
    assert(editor.append_codepoint('a') == ChineseEditorResult::Full);
    assert(editor.erase_last_codepoint());
    assert(editor.text_bytes() == 796U && editor.codepoint_count() == 199U);
    assert(editor.append_codepoint(0xD800U) ==
           ChineseEditorResult::InvalidInput);

    const char invalid_utf8[] = {static_cast<char>(0xC0U),
                                 static_cast<char>(0x80U), '\0'};
    assert(editor.set_text(invalid_utf8) == ChineseEditorResult::InvalidInput);
    const std::string too_many(201U, 'x');
    assert(editor.set_text(too_many.c_str()) == ChineseEditorResult::Full);
    assert(editor.set_text("孕期备忘") == ChineseEditorResult::Ok);
    assert(editor.codepoint_count() == 4U);
    assert(editor.erase_last_codepoint());
    assert(std::strcmp(editor.text(), "孕期备") == 0);
}
