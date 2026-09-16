#include <cassert>

#include "sticky_app_display_orientation.h"

int main() {
  CanvasRotation rotation = CanvasRotation::Deg0;
  assert(sticky_app_display_rotation(
      StickyAppRotationPolicy::FollowDevice,
      StickyImuOrientation::Landscape0, CanvasRotation::Deg0, rotation));
  assert(rotation == CanvasRotation::Deg90CounterClockwise);

  assert(sticky_app_display_rotation(
      StickyAppRotationPolicy::FollowDevice,
      StickyImuOrientation::Landscape180, rotation, rotation));
  assert(rotation == CanvasRotation::Deg90Clockwise);

  assert(sticky_app_display_rotation(
      StickyAppRotationPolicy::FollowDevice,
      StickyImuOrientation::Portrait0, rotation, rotation));
  assert(rotation == CanvasRotation::Deg180);

  assert(sticky_app_display_rotation(
      StickyAppRotationPolicy::FollowDevice,
      StickyImuOrientation::Portrait180, rotation, rotation));
  assert(rotation == CanvasRotation::Deg0);

  assert(!sticky_app_display_rotation(
      StickyAppRotationPolicy::FollowDevice, StickyImuOrientation::FaceUp,
      rotation, rotation));
  assert(rotation == CanvasRotation::Deg0);

  rotation = CanvasRotation::Deg90Clockwise;
  assert(sticky_app_display_rotation(
      StickyAppRotationPolicy::FixedLandscape,
      StickyImuOrientation::Landscape0, rotation, rotation));
  assert(rotation == CanvasRotation::Deg0);

  rotation = CanvasRotation::Deg0;
  assert(sticky_app_display_rotation(
      StickyAppRotationPolicy::FixedPortrait,
      StickyImuOrientation::Portrait0, rotation, rotation));
  assert(rotation == CanvasRotation::Deg90Clockwise);
  return 0;
}
