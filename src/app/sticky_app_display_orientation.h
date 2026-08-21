#pragma once

#include "canvas.h"
#include "sticky_app_id.h"
#include "sticky_orientation.h"

// Maps the device's final settled orientation to the selected app's canvas.
// 将设备最终稳定方向映射为目标APP的画布方向。
bool sticky_app_display_rotation(StickyAppId app,
                                 StickyImuOrientation orientation,
                                 CanvasRotation &rotation);

const char *sticky_app_display_rotation_name(CanvasRotation rotation);
