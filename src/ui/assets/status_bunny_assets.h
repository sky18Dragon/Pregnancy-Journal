#pragma once

#include "pixel_asset.h"

enum class StatusBunnyAssetId {
    Focusing,
    InMeeting,
    Welcome,
    OutForLunch,
    OffDuty,
    Custom,
};

// Returns one reusable 80x80 bunny scene from flash memory.
// 返回一张存放在固件闪存中的80x80可复用兔子场景素材。
const PixelAsset &status_bunny_asset(StatusBunnyAssetId id);
