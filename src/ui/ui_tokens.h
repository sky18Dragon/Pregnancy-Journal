#pragma once

#include <cstdint>

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

constexpr int kContentMargin = 24;
constexpr int kSafeArea = 24;
constexpr int kMinimumTouchHeight = 48;
constexpr int kPreferredTouchHeight = 56;
constexpr int kCompactButtonHeight = 48;
constexpr int kButtonHeight = 56;
constexpr int kBodyLineGap = 6;
constexpr int kRuleStroke = 1;
constexpr int kBorderStroke = 1;
constexpr int kIconStroke = 2;
constexpr int kEmphasisStroke = 3;
constexpr int kIconSmall = 16;
constexpr int kIcon = 20;
constexpr int kIconLarge = 24;

} // namespace ui_tokens
