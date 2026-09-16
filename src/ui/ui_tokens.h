#pragma once

#include <cstdint>

namespace ui_tokens {

constexpr uint8_t kMicro = 12U;
constexpr uint8_t kCompactLabel = 14U;
constexpr uint8_t kSecondaryBody = 16U;
constexpr uint8_t kBody = 18U;
constexpr uint8_t kEmphasis = 20U;
constexpr uint8_t kSectionHeading = 24U;
constexpr uint8_t kPageHeading = 28U;
constexpr uint8_t kDisplay = 52U;
constexpr uint8_t kDisplayLarge = 56U;

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
