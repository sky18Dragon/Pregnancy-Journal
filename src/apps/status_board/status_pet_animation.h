#pragma once

#include <cstddef>
#include <cstdint>

#include "pet_animation_assets.h"

struct StatusPetFrame {
    PetAnimationPose pose;
    int center_x;
    uint16_t hold_ms;
};

// Returns the number of frames in one complete left-to-right patrol.
// 返回一轮完整左右巡场动画的帧数。
size_t status_pet_frame_count();

// Returns one frame and wraps out-of-range indexes to the first frame.
// 返回指定动画帧，超出范围的索引从首帧循环。
const StatusPetFrame &status_pet_frame(size_t index);

const char *status_pet_pose_name(PetAnimationPose pose);
