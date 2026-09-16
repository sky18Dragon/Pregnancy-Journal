#include "sticky_app_display_orientation.h"

namespace {

bool is_landscape(CanvasRotation rotation) {
  return rotation == CanvasRotation::Deg0 ||
         rotation == CanvasRotation::Deg180;
}

bool is_portrait(CanvasRotation rotation) {
  return rotation == CanvasRotation::Deg90Clockwise ||
         rotation == CanvasRotation::Deg90CounterClockwise;
}

bool follow_device(StickyImuOrientation orientation,
                   CanvasRotation &rotation) {
  switch (orientation) {
  case StickyImuOrientation::Landscape0:
    rotation = CanvasRotation::Deg90CounterClockwise;
    return true;
  case StickyImuOrientation::Landscape180:
    rotation = CanvasRotation::Deg90Clockwise;
    return true;
  case StickyImuOrientation::Portrait0:
    rotation = CanvasRotation::Deg180;
    return true;
  case StickyImuOrientation::Portrait180:
    rotation = CanvasRotation::Deg0;
    return true;
  case StickyImuOrientation::Unknown:
  case StickyImuOrientation::FaceUp:
  case StickyImuOrientation::FaceDown:
    return false;
  }
  return false;
}

} // namespace

bool sticky_app_display_rotation(StickyAppRotationPolicy policy,
                                 StickyImuOrientation orientation,
                                 CanvasRotation current,
                                 CanvasRotation &rotation) {
  rotation = current;
  if (policy == StickyAppRotationPolicy::FollowDevice)
    return follow_device(orientation, rotation);

  if (policy == StickyAppRotationPolicy::FixedLandscape) {
    if (orientation == StickyImuOrientation::Portrait0)
      rotation = CanvasRotation::Deg180;
    else if (orientation == StickyImuOrientation::Portrait180)
      rotation = CanvasRotation::Deg0;
    else if (!is_landscape(rotation))
      rotation = CanvasRotation::Deg0;
    return rotation != current;
  }

  if (orientation == StickyImuOrientation::Landscape0)
    rotation = CanvasRotation::Deg90CounterClockwise;
  else if (orientation == StickyImuOrientation::Landscape180)
    rotation = CanvasRotation::Deg90Clockwise;
  else if (!is_portrait(rotation))
    rotation = CanvasRotation::Deg90Clockwise;
  return rotation != current;
}

bool sticky_app_launcher_rotation(StickyImuOrientation orientation,
                                  CanvasRotation current,
                                  CanvasRotation &rotation) {
  rotation = current;
  return follow_device(orientation, rotation);
}

const char *sticky_app_display_rotation_name(CanvasRotation rotation) {
  switch (rotation) {
  case CanvasRotation::Deg0:
    return "0";
  case CanvasRotation::Deg90Clockwise:
    return "90_clockwise";
  case CanvasRotation::Deg180:
    return "180";
  case CanvasRotation::Deg90CounterClockwise:
    return "90_counter_clockwise";
  }
  return "unknown";
}
