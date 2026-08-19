#pragma once

#include "pixel_asset.h"

enum class DesktopPetAssetId : uint8_t {
    Room,
    Idle,
    IdleMask,
    IdleBlink,
    IdleBlinkMask,
    IdleEarTwitch,
    IdleEarTwitchMask,
    IdleLookAround,
    IdleLookAroundMask,
    IdleStretch,
    IdleStretchMask,
    IdleHungry,
    IdleHungryMask,
    IdleTired,
    IdleTiredMask,
    Feed,
    FeedMask,
    Pet,
    PetMask,
    Play,
    PlayMask,
    FeedIcon,
    PetIcon,
    TalkIcon,
    PlayIcon,
    LoveIcon,
};

// Returns one generated desktop-pet bitmap.
// 返回一张已生成的桌宠位图。
const PixelAsset &desktop_pet_asset(DesktopPetAssetId id);
