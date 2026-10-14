#pragma once

#include <cstddef>
#include <cstdint>

namespace sticky_ui {

constexpr size_t kChineseEditorMaximumCodepoints = 200U;
constexpr size_t kChineseEditorMaximumUtf8Bytes = 800U;
constexpr size_t kChineseEditorMaximumPinyinBytes = 6U;
constexpr size_t kChineseEditorCandidatesPerPage = 8U;

enum class ChineseEditorResult : uint8_t {
    Ok = 0,
    InvalidInput,
    Full,
    CandidateUnavailable,
};

class ChineseTextEditor {
  public:
    ChineseTextEditor();

    const char *text() const;
    size_t text_bytes() const;
    size_t codepoint_count() const;
    bool empty() const;

    ChineseEditorResult set_text(const char *utf8);
    ChineseEditorResult append_codepoint(uint32_t codepoint);
    bool erase_last_codepoint();
    void clear_text();

    const char *pinyin() const;
    size_t pinyin_bytes() const;
    ChineseEditorResult type_pinyin(char letter);
    bool erase_pinyin_letter();
    void clear_pinyin();

    size_t candidate_count() const;
    size_t candidate_page() const;
    size_t candidate_page_count() const;
    uint16_t candidate_at(size_t index_on_page) const;
    bool next_candidate_page();
    bool previous_candidate_page();
    ChineseEditorResult choose_candidate(size_t index_on_page);

  private:
    char text_[kChineseEditorMaximumUtf8Bytes + 1U];
    char pinyin_[kChineseEditorMaximumPinyinBytes + 1U];
    uint16_t text_bytes_;
    uint16_t codepoint_count_;
    uint8_t pinyin_bytes_;
    uint8_t candidate_page_;
};

size_t chinese_text_editor_state_bytes();

}  // namespace sticky_ui
