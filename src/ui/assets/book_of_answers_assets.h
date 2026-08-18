#pragma once

#include "pixel_asset.h"

enum class BookOfAnswersAssetId {
    Home,
    ShakeLeft,
    ShakeRight,
    Thinking,
    Revealing,
    MessageResult,
    CrystalPeek,
};

// Returns one firmware-ready monochrome illustration for the Book of Answers.
// 返回一张可直接绘制的答案书黑白固件插画。
const PixelAsset &book_of_answers_asset(BookOfAnswersAssetId id);
