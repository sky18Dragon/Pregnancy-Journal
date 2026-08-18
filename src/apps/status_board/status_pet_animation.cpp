#include "status_pet_animation.h"

namespace {

// Frame holds are the quiet interval after each electronic-paper refresh.
// 每帧停留时间从电子纸刷新完成后开始计算。
constexpr StatusPetFrame kFrames[] = {
    {PetAnimationPose::Wave, 62, 260},
    {PetAnimationPose::Crouch, 62, 120},
    {PetAnimationPose::Jump, 240, 120},
    {PetAnimationPose::Jump, 430, 120},
    {PetAnimationPose::Jump, 620, 120},
    {PetAnimationPose::Celebrate, 738, 280},
    {PetAnimationPose::WalkLeft, 570, 150},
    {PetAnimationPose::WalkLeft, 350, 150},
    {PetAnimationPose::WalkLeft, 150, 150},
};

}  // namespace

size_t status_pet_frame_count()
{
    return sizeof(kFrames) / sizeof(kFrames[0]);
}

const StatusPetFrame &status_pet_frame(size_t index)
{
    return kFrames[index % status_pet_frame_count()];
}

const char *status_pet_pose_name(PetAnimationPose pose)
{
    switch (pose) {
    case PetAnimationPose::Wave:
        return "wave";
    case PetAnimationPose::Crouch:
        return "crouch";
    case PetAnimationPose::Jump:
        return "jump";
    case PetAnimationPose::Celebrate:
        return "celebrate";
    case PetAnimationPose::WalkLeft:
        return "walk_left";
    }
    return "unknown";
}
