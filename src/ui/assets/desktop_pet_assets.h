#pragma once

#include "pixel_asset.h"

enum class DesktopPetAssetId : uint8_t {
    Room,
    Idle,
    IdleMask,
    Feed,
    FeedMask,
    Pet,
    PetMask,
    Play,
    PlayMask,
    FeedIcon,
    PetIcon,
    PlayIcon,
    LoveIcon,
};

// Returns one generated desktop-pet bitmap.
// 返回一张已生成的桌宠位图。
const PixelAsset &desktop_pet_asset(DesktopPetAssetId id);
