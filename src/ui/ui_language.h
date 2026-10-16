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

#ifdef UI_TEXT_OBSERVER
// Host preview only: called when Chinese is active and `english` has no
// translation, so the preview tool can list every untranslated source string.
// 仅宿主预览：中文模式下遇到没有译文的英文源文案时回调。
using UiMissingTranslationObserver = void (*)(const char *english);
void ui_set_missing_translation_observer(UiMissingTranslationObserver observer);
#endif

// Measures the translated UTF-8 text with the same metrics used by Canvas.
// 使用与Canvas一致的度量计算翻译后UTF-8文字宽度。
int ui_text_width(const char *english, uint8_t scale);

