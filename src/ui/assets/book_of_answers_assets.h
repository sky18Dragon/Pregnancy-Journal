#pragma once

#include "pixel_asset.h"

enum class BookOfAnswersAssetId {
    Home,
    HomeAlt,
    ShakeLeft,
    ShakeRight,
    Thinking,
    ThinkingAlt,
    Revealing,
    RevealingAlt,
    MessageResult,
    MessageResultAlt,
    CrystalResultPrimary,
    CrystalResultSecondary,
};

// Returns one firmware-ready monochrome illustration for the Book of Answers.
// 返回一张可直接绘制的答案书黑白固件插画。
const PixelAsset &book_of_answers_asset(BookOfAnswersAssetId id);
