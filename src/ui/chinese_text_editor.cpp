#include "chinese_text_editor.h"

#include <cstring>

#include "pinyin_candidate_index.h"

namespace sticky_ui {
namespace {

bool decode_utf8(const char *text, size_t length, size_t &offset,
                 uint32_t &codepoint)
{
    if (offset >= length) {
        return false;
    }
    const auto first = static_cast<uint8_t>(text[offset++]);
    if (first <= 0x7FU) {
        codepoint = first;
        return first != 0U;
    }
    size_t continuation_count = 0U;
    uint32_t minimum = 0U;
    if (first >= 0xC2U && first <= 0xDFU) {
        codepoint = first & 0x1FU;
        continuation_count = 1U;
        minimum = 0x80U;
    } else if (first >= 0xE0U && first <= 0xEFU) {
        codepoint = first & 0x0FU;
        continuation_count = 2U;
        minimum = 0x800U;
    } else if (first >= 0xF0U && first <= 0xF4U) {
        codepoint = first & 0x07U;
        continuation_count = 3U;
        minimum = 0x10000U;
    } else {
        return false;
    }
    if (offset + continuation_count > length) {
        return false;
    }
    for (size_t index = 0U; index < continuation_count; ++index) {
        const auto next = static_cast<uint8_t>(text[offset++]);
        if ((next & 0xC0U) != 0x80U) {
            return false;
        }
        codepoint = (codepoint << 6U) | (next & 0x3FU);
    }
    return codepoint >= minimum && codepoint <= 0x10FFFFU &&
           !(codepoint >= 0xD800U && codepoint <= 0xDFFFU);
}

size_t encode_utf8(uint32_t codepoint, char output[4])
{
    if (codepoint == 0U || codepoint > 0x10FFFFU ||
        (codepoint >= 0xD800U && codepoint <= 0xDFFFU)) {
        return 0U;
    }
    if (codepoint <= 0x7FU) {
        output[0] = static_cast<char>(codepoint);
        return 1U;
    }
    if (codepoint <= 0x7FFU) {
        output[0] = static_cast<char>(0xC0U | (codepoint >> 6U));
        output[1] = static_cast<char>(0x80U | (codepoint & 0x3FU));
        return 2U;
    }
    if (codepoint <= 0xFFFFU) {
        output[0] = static_cast<char>(0xE0U | (codepoint >> 12U));
        output[1] = static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU));
        output[2] = static_cast<char>(0x80U | (codepoint & 0x3FU));
        return 3U;
    }
    output[0] = static_cast<char>(0xF0U | (codepoint >> 18U));
    output[1] = static_cast<char>(0x80U | ((codepoint >> 12U) & 0x3FU));
    output[2] = static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU));
    output[3] = static_cast<char>(0x80U | (codepoint & 0x3FU));
    return 4U;
}

}  // namespace

ChineseTextEditor::ChineseTextEditor()
    : text_{0}, pinyin_{0}, text_bytes_(0U), codepoint_count_(0U),
      pinyin_bytes_(0U), candidate_page_(0U)
{
}

const char *ChineseTextEditor::text() const { return text_; }
size_t ChineseTextEditor::text_bytes() const { return text_bytes_; }
size_t ChineseTextEditor::codepoint_count() const { return codepoint_count_; }
bool ChineseTextEditor::empty() const { return codepoint_count_ == 0U; }

ChineseEditorResult ChineseTextEditor::set_text(const char *utf8)
{
    if (utf8 == nullptr) {
        return ChineseEditorResult::InvalidInput;
    }
    const size_t length = std::strlen(utf8);
    if (length > kChineseEditorMaximumUtf8Bytes) {
        return ChineseEditorResult::Full;
    }
    size_t offset = 0U;
    size_t count = 0U;
    while (offset < length) {
        uint32_t codepoint = 0U;
        if (!decode_utf8(utf8, length, offset, codepoint)) {
            return ChineseEditorResult::InvalidInput;
        }
        ++count;
        if (count > kChineseEditorMaximumCodepoints) {
            return ChineseEditorResult::Full;
        }
    }
    std::memcpy(text_, utf8, length + 1U);
    text_bytes_ = length;
    codepoint_count_ = count;
    return ChineseEditorResult::Ok;
}

ChineseEditorResult ChineseTextEditor::append_codepoint(uint32_t codepoint)
{
    char encoded[4] = {};
    const size_t encoded_bytes = encode_utf8(codepoint, encoded);
    if (encoded_bytes == 0U) {
        return ChineseEditorResult::InvalidInput;
    }
    if (codepoint_count_ >= kChineseEditorMaximumCodepoints ||
        text_bytes_ + encoded_bytes > kChineseEditorMaximumUtf8Bytes) {
        return ChineseEditorResult::Full;
    }
    std::memcpy(text_ + text_bytes_, encoded, encoded_bytes);
    text_bytes_ += encoded_bytes;
    text_[text_bytes_] = '\0';
    ++codepoint_count_;
    return ChineseEditorResult::Ok;
}

bool ChineseTextEditor::erase_last_codepoint()
{
    if (text_bytes_ == 0U) {
        return false;
    }
    size_t offset = text_bytes_ - 1U;
    while (offset > 0U &&
           (static_cast<uint8_t>(text_[offset]) & 0xC0U) == 0x80U) {
        --offset;
    }
    text_bytes_ = offset;
    text_[text_bytes_] = '\0';
    --codepoint_count_;
    return true;
}

void ChineseTextEditor::clear_text()
{
    text_[0] = '\0';
    text_bytes_ = 0U;
    codepoint_count_ = 0U;
}

const char *ChineseTextEditor::pinyin() const { return pinyin_; }
size_t ChineseTextEditor::pinyin_bytes() const { return pinyin_bytes_; }

ChineseEditorResult ChineseTextEditor::type_pinyin(char letter)
{
    if (letter < 'a' || letter > 'z') {
        return ChineseEditorResult::InvalidInput;
    }
    if (pinyin_bytes_ >= kChineseEditorMaximumPinyinBytes) {
        return ChineseEditorResult::Full;
    }
    pinyin_[pinyin_bytes_++] = letter;
    pinyin_[pinyin_bytes_] = '\0';
    candidate_page_ = 0U;
    return ChineseEditorResult::Ok;
}

bool ChineseTextEditor::erase_pinyin_letter()
{
    if (pinyin_bytes_ == 0U) {
        return false;
    }
    pinyin_[--pinyin_bytes_] = '\0';
    candidate_page_ = 0U;
    return true;
}

void ChineseTextEditor::clear_pinyin()
{
    pinyin_[0] = '\0';
    pinyin_bytes_ = 0U;
    candidate_page_ = 0U;
}

size_t ChineseTextEditor::candidate_count() const
{
    return pinyin_candidate_lookup(pinyin_).count;
}

size_t ChineseTextEditor::candidate_page() const { return candidate_page_; }

size_t ChineseTextEditor::candidate_page_count() const
{
    const size_t count = candidate_count();
    return count == 0U ? 0U
                       : (count + kChineseEditorCandidatesPerPage - 1U) /
                             kChineseEditorCandidatesPerPage;
}

uint16_t ChineseTextEditor::candidate_at(size_t index_on_page) const
{
    if (index_on_page >= kChineseEditorCandidatesPerPage) {
        return 0U;
    }
    const PinyinCandidateSpan span = pinyin_candidate_lookup(pinyin_);
    const size_t index = candidate_page_ * kChineseEditorCandidatesPerPage +
                         index_on_page;
    return index < span.count ? span.codepoints[index] : 0U;
}

bool ChineseTextEditor::next_candidate_page()
{
    if (candidate_page_ + 1U >= candidate_page_count()) {
        return false;
    }
    ++candidate_page_;
    return true;
}

bool ChineseTextEditor::previous_candidate_page()
{
    if (candidate_page_ == 0U) {
        return false;
    }
    --candidate_page_;
    return true;
}

ChineseEditorResult ChineseTextEditor::choose_candidate(size_t index_on_page)
{
    const uint16_t codepoint = candidate_at(index_on_page);
    if (codepoint == 0U) {
        return ChineseEditorResult::CandidateUnavailable;
    }
    const ChineseEditorResult result = append_codepoint(codepoint);
    if (result == ChineseEditorResult::Ok) {
        clear_pinyin();
    }
    return result;
}

size_t chinese_text_editor_state_bytes()
{
    return sizeof(ChineseTextEditor);
}

}  // namespace sticky_ui
