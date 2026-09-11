#pragma once

#include <cstdint>

enum class UiLanguage : uint8_t {
    English = 0,
    ChineseSimplified = 1,
};

UiLanguage ui_language_get();
void ui_language_set(UiLanguage language);
bool ui_language_is_chinese();

// Returns the active translation for one user-visible English source string.
// 返回一条界面英文源文案在当前语言下的译文。
const char *ui_text(const char *english);

// Measures the translated UTF-8 text with the same metrics used by Canvas.
// 使用与Canvas一致的度量计算翻译后UTF-8文字宽度。
int ui_text_width(const char *english, uint8_t scale);

