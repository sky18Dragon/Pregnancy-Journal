#pragma once

#include <cstdint>
#include "ui_language.h"

namespace ui_tokens {

// Text tiers. Nothing below 16 px is allowed on the panel.
//   kSecondaryBody (16): captions, subtitles, footers, chart tick labels.
//   kBody (18): body copy and anything the user must read to act (buttons,
//   list rows, values, warnings).
constexpr uint8_t kSecondaryBody = 16U;
constexpr uint8_t kBody = 18U;
constexpr uint8_t kEmphasis = 20U;
constexpr uint8_t kCardTitle = 22U;
constexpr uint8_t kSectionHeading = 24U;
constexpr uint8_t kPageHeading = 28U;
constexpr uint8_t kDisplay = 52U;
constexpr uint8_t kDisplayLarge = 56U;
constexpr uint8_t kCounterDisplay = 70U;

// Chinese reading copy needs a larger native strike on the 1-bit panel.
// Use the same resolved size for measuring, wrapping and drawing. Keep the
// fixed caption token for compact chart ticks and the font diagnostic screen.
inline uint8_t reading_size() {
  return ui_language_is_chinese() ? kBody : kSecondaryBody;
}
inline uint8_t button_size() {
  return ui_language_is_chinese() ? kEmphasis : kBody;
}
inline uint8_t portrait_heading_size() {
  return ui_language_is_chinese() ? kPageHeading : kSectionHeading;
}

constexpr int kContentMargin = 24;
constexpr int kSafeArea = 24;
constexpr int kMinimumTouchHeight = 48;
constexpr int kPreferredTouchHeight = 56;
constexpr int kCompactButtonHeight = 48;
constexpr int kButtonHeight = 56;
constexpr int kBodyLineGap = 6;
// Strokes. 1 px is only for secondary dividers; every border is 2 px, and
// 3 px marks emphasis (pressed large targets).
constexpr int kRuleStroke = 1;
constexpr int kBorderStroke = 2;
constexpr int kIconStroke = 2;
constexpr int kEmphasisStroke = 3;
constexpr int kIconSmall = 16;
constexpr int kIcon = 20;
constexpr int kIconLarge = 24;

} // namespace ui_tokens
