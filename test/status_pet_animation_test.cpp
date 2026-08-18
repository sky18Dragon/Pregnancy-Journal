#include <cassert>

#include "status_pet_animation.h"

int main()
{
    assert(status_pet_frame_count() == 9U);

    const StatusPetFrame &left_action = status_pet_frame(0U);
    assert(left_action.pose == PetAnimationPose::Wave);
    assert(left_action.center_x < 100);

    int previous_jump_x = 0;
    for (size_t index = 2U; index <= 4U; ++index) {
        const StatusPetFrame &frame = status_pet_frame(index);
        assert(frame.pose == PetAnimationPose::Jump);
        assert(frame.center_x > previous_jump_x);
        previous_jump_x = frame.center_x;
    }

    const StatusPetFrame &right_action = status_pet_frame(5U);
    assert(right_action.pose == PetAnimationPose::Celebrate);
    assert(right_action.center_x > 700);

    int previous_walk_x = 801;
    for (size_t index = 6U; index <= 8U; ++index) {
        const StatusPetFrame &frame = status_pet_frame(index);
        assert(frame.pose == PetAnimationPose::WalkLeft);
        assert(frame.center_x < previous_walk_x);
        previous_walk_x = frame.center_x;
    }

    for (size_t index = 0U; index < status_pet_frame_count(); ++index) {
        const StatusPetFrame &frame = status_pet_frame(index);
        assert(frame.center_x >= 48);
        assert(frame.center_x <= 752);
        assert(frame.hold_ms >= 100U);
        assert(frame.hold_ms <= 300U);
    }

    assert(&status_pet_frame(status_pet_frame_count()) ==
           &status_pet_frame(0U));
    return 0;
}
