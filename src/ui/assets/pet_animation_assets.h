#pragma once

#include "pixel_asset.h"

enum class PetAnimationPose {
    Wave,
    Crouch,
    Jump,
    Celebrate,
    WalkLeft,
};

// Returns one reusable 96x96 pet animation pose from flash memory.
// 返回一张存放在固件闪存中的96x96宠物动画动作。
const PixelAsset &pet_animation_asset(PetAnimationPose pose);
