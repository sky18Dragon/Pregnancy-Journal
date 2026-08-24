#pragma once

#include "canvas.h"
#include "sticky_app_id.h"
#include "sticky_orientation.h"

// Maps the device's final settled orientation to the selected app's canvas.
// 将设备最终稳定方向映射为目标APP的画布方向。
bool sticky_app_display_rotation(StickyAppId app,
                                 StickyImuOrientation orientation,
                                 CanvasRotation &rotation);

// Maps the current settled device pose to the launcher's canvas direction.
// 将设备当前稳定姿态映射为应用选择器的画布方向。
bool sticky_app_launcher_rotation(StickyImuOrientation orientation,
                                  CanvasRotation &rotation);

const char *sticky_app_display_rotation_name(CanvasRotation rotation);
