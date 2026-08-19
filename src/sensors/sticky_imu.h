#pragma once

#include <cstdint>

#include "driver/i2c_master.h"
#include "esp_err.h"

enum class StickyImuOrientation {
    Unknown,
    Landscape0,
    Landscape180,
    Portrait0,
    Portrait180,
    FaceUp,
    FaceDown,
};

struct StickyImuState {
    float acceleration_x_g = 0.0F;
    float acceleration_y_g = 0.0F;
    float acceleration_z_g = 0.0F;
    StickyImuOrientation orientation = StickyImuOrientation::Unknown;
    bool moving = false;
    bool valid = false;
};

// Initializes the LSM6DS3TR-C accelerometer on the shared sensor I2C bus.
// 在共享传感器I2C总线上初始化LSM6DS3TR-C加速度计。
esp_err_t sticky_imu_init(i2c_master_bus_handle_t bus);

// Starts the stable-orientation monitoring task.
// 启动稳定姿态监测任务。
esp_err_t sticky_imu_start_monitoring();

// Copies the latest acceleration, motion flag, and settled orientation.
// 复制最近一次加速度、移动标记和最终放稳姿态。
esp_err_t sticky_imu_get_state(StickyImuState &state);

// Returns and clears one pending shake-session start event.
// 读取并清除一次待处理的摇晃会话开始事件。
bool sticky_imu_take_shake_started_event();

// Returns and clears one pending shake-session stop event.
// 读取并清除一次待处理的摇晃会话停止事件。
bool sticky_imu_take_shake_stopped_event();

// Reports whether effective shake peaks are still arriving continuously.
// 返回当前是否仍在持续收到有效摇晃峰值。
bool sticky_imu_is_shaking();

// Returns the time span covered by effective peaks in the current session.
// 返回当前摇晃会话中有效峰值实际覆盖的时间跨度。
uint32_t sticky_imu_shake_duration_ms();

// Returns the stable log and UI name for one orientation value.
// 返回姿态对应的稳定日志与界面名称。
const char *sticky_imu_orientation_name(StickyImuOrientation orientation);
