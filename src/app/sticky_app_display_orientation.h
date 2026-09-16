#pragma once

#include "app_descriptor.h"
#include "canvas.h"
#include "sticky_orientation.h"

// Resolves an app rotation policy against the device's settled IMU pose.
// Unknown and face-up/down poses retain the supplied rotation.
bool sticky_app_display_rotation(StickyAppRotationPolicy policy,
                                 StickyImuOrientation orientation,
                                 CanvasRotation current,
                                 CanvasRotation &rotation);

bool sticky_app_launcher_rotation(StickyImuOrientation orientation,
                                  CanvasRotation current,
                                  CanvasRotation &rotation);

const char *sticky_app_display_rotation_name(CanvasRotation rotation);
