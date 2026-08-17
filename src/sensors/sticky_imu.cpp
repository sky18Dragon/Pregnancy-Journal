#include "sticky_imu.h"

#include <cmath>
#include <cstddef>
#include <cstdint>

#include "app_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pin_config.h"

namespace {

constexpr char kTag[] = "sticky_imu";
constexpr uint32_t kI2cClockHz = 400000;
constexpr int kI2cTimeoutMs = 100;
constexpr uint8_t kWhoAmIRegister = 0x0F;
constexpr uint8_t kExpectedWhoAmI = 0x6A;
constexpr uint8_t kAccelerometerControlRegister = 0x10;
constexpr uint8_t kControlRegister3 = 0x12;
constexpr uint8_t kAccelerometerOutputRegister = 0x28;
constexpr uint8_t kAccelerometer104Hz2g = 0x40;
constexpr uint8_t kRegisterAutoIncrement = 0x04;
constexpr float kAccelerationScaleG = 0.000061F;
constexpr float kOrientationThresholdG = 0.70F;

// Placement thresholds operate on the measured gravity vector in g.
// 放稳状态阈值使用以g为单位的重力向量。
constexpr float kMotionStartDeltaG = 0.10F;
constexpr float kQuietVectorDeltaG = 0.05F;
constexpr float kQuietMagnitudeMinG = 0.75F;
constexpr float kQuietMagnitudeMaxG = 1.25F;
constexpr int kSettleSampleCount = 10;
constexpr TickType_t kPollInterval = pdMS_TO_TICKS(100);
constexpr uint32_t kTaskStackSize = 3072;
constexpr UBaseType_t kTaskPriority = 4;

i2c_master_dev_handle_t s_device = nullptr;
TaskHandle_t s_monitor_task = nullptr;
portMUX_TYPE s_state_lock = portMUX_INITIALIZER_UNLOCKED;
StickyImuState s_latest_state = {};

// Holds one complete movement session from the last settled pose to the next.
// 保存从上一次放稳姿态到下一次放稳姿态的一整段移动过程。
struct PlacementTracker {
    StickyImuOrientation settled_orientation = StickyImuOrientation::Unknown;
    StickyImuOrientation quiet_candidate = StickyImuOrientation::Unknown;
    StickyImuState settled_reference = {};
    StickyImuState quiet_anchor = {};
    StickyImuState previous_sample = {};
    int quiet_sample_count = 0;
    bool moving = false;
    bool has_settled_reference = false;
    bool has_previous_sample = false;
    TickType_t motion_started_at = 0;
};

esp_err_t write_register(uint8_t reg, uint8_t value)
{
    const uint8_t data[] = {reg, value};
    return i2c_master_transmit(
        s_device, data, sizeof(data), kI2cTimeoutMs);
}

esp_err_t read_registers(uint8_t reg, uint8_t *data, size_t length)
{
    return i2c_master_transmit_receive(
        s_device, &reg, 1, data, length, kI2cTimeoutMs);
}

int16_t read_int16_le(const uint8_t *data)
{
    return static_cast<int16_t>(
        (static_cast<uint16_t>(data[1]) << 8) | data[0]);
}

StickyImuOrientation classify_orientation(float x, float y, float z)
{
    const float abs_x = std::fabs(x);
    const float abs_y = std::fabs(y);
    const float abs_z = std::fabs(z);

    // Selects the dominant gravity axis after it crosses the reference threshold.
    // 当某个重力轴超过参考阈值后，选择数值最大的轴作为当前姿态。
    if (abs_z >= kOrientationThresholdG && abs_z > abs_x && abs_z > abs_y) {
        return z >= 0.0F ? StickyImuOrientation::FaceUp
                         : StickyImuOrientation::FaceDown;
    }
    if (abs_x >= kOrientationThresholdG && abs_x > abs_y) {
        // Hardware calibration: +X is portrait 0; -X is portrait 180.
        // 真机校准结果：+X为竖置0度，-X为竖置180度。
        return x < 0.0F ? StickyImuOrientation::Portrait180
                        : StickyImuOrientation::Portrait0;
    }
    if (abs_y >= kOrientationThresholdG) {
        // Hardware calibration: -Y is landscape 0; +Y is landscape 180.
        // 真机校准结果：-Y为横置0度，+Y为横置180度。
        return y < 0.0F ? StickyImuOrientation::Landscape0
                        : StickyImuOrientation::Landscape180;
    }
    return StickyImuOrientation::Unknown;
}

const char *orientation_name(StickyImuOrientation orientation)
{
    switch (orientation) {
    case StickyImuOrientation::Landscape0:
        return "landscape_0";
    case StickyImuOrientation::Landscape180:
        return "landscape_180";
    case StickyImuOrientation::Portrait0:
        return "portrait_0";
    case StickyImuOrientation::Portrait180:
        return "portrait_180";
    case StickyImuOrientation::FaceUp:
        return "face_up";
    case StickyImuOrientation::FaceDown:
        return "face_down";
    case StickyImuOrientation::Unknown:
    default:
        return "unknown";
    }
}

float acceleration_magnitude(const StickyImuState &state)
{
    return std::sqrt(state.acceleration_x_g * state.acceleration_x_g +
                     state.acceleration_y_g * state.acceleration_y_g +
                     state.acceleration_z_g * state.acceleration_z_g);
}

float acceleration_delta(const StickyImuState &first,
                         const StickyImuState &second)
{
    const float delta_x = first.acceleration_x_g - second.acceleration_x_g;
    const float delta_y = first.acceleration_y_g - second.acceleration_y_g;
    const float delta_z = first.acceleration_z_g - second.acceleration_z_g;
    return std::sqrt(delta_x * delta_x + delta_y * delta_y +
                     delta_z * delta_z);
}

bool is_quiet_sample(const StickyImuState &sample)
{
    const float magnitude = acceleration_magnitude(sample);
    return sample.orientation != StickyImuOrientation::Unknown &&
           magnitude >= kQuietMagnitudeMinG &&
           magnitude <= kQuietMagnitudeMaxG;
}

void reset_quiet_candidate(PlacementTracker &tracker,
                           const StickyImuState &sample)
{
    if (is_quiet_sample(sample)) {
        tracker.quiet_candidate = sample.orientation;
        tracker.quiet_anchor = sample;
        tracker.quiet_sample_count = 1;
    } else {
        tracker.quiet_candidate = StickyImuOrientation::Unknown;
        tracker.quiet_anchor = sample;
        tracker.quiet_sample_count = 0;
    }
}

void begin_motion(PlacementTracker &tracker,
                  const StickyImuState &sample,
                  float delta_g)
{
    tracker.moving = true;
    tracker.motion_started_at = xTaskGetTickCount();
    reset_quiet_candidate(tracker, sample);
    STICKY_LOGI(kTag,
                "imu=motion state=moving from=%s delta_g=%.3f",
                orientation_name(tracker.settled_orientation),
                static_cast<double>(delta_g));
}

void commit_settled_placement(PlacementTracker &tracker,
                              const StickyImuState &sample)
{
    const StickyImuOrientation previous = tracker.settled_orientation;
    const uint32_t motion_ms = tracker.moving
                                   ? static_cast<uint32_t>(pdTICKS_TO_MS(
                                         xTaskGetTickCount() -
                                         tracker.motion_started_at))
                                   : 0;

    tracker.settled_orientation = tracker.quiet_candidate;
    tracker.settled_reference = sample;
    tracker.has_settled_reference = true;
    tracker.moving = false;
    STICKY_LOGI(kTag,
                "imu=placement state=settled from=%s to=%s motion_ms=%lu quiet_samples=%d x_g=%.3f y_g=%.3f z_g=%.3f",
                orientation_name(previous),
                orientation_name(tracker.settled_orientation),
                static_cast<unsigned long>(motion_ms),
                tracker.quiet_sample_count,
                static_cast<double>(sample.acceleration_x_g),
                static_cast<double>(sample.acceleration_y_g),
                static_cast<double>(sample.acceleration_z_g));
}

void update_placement(PlacementTracker &tracker, StickyImuState &sample)
{
    // Compares each sample with the last committed pose and current quiet window.
    // 将每次采样与上次已提交姿态及当前安静窗口进行比较。
    const float reference_delta = tracker.has_settled_reference
                                      ? acceleration_delta(
                                            sample, tracker.settled_reference)
                                      : 0.0F;

    if (!tracker.has_settled_reference) {
        if (tracker.quiet_sample_count == 0 ||
            sample.orientation != tracker.quiet_candidate ||
            acceleration_delta(sample, tracker.quiet_anchor) >
                kQuietVectorDeltaG) {
            reset_quiet_candidate(tracker, sample);
        } else if (is_quiet_sample(sample)) {
            ++tracker.quiet_sample_count;
        }
    } else if (!tracker.moving) {
        const bool left_settled_position =
            reference_delta >= kMotionStartDeltaG ||
            sample.orientation != tracker.settled_orientation ||
            !is_quiet_sample(sample);
        if (left_settled_position) {
            begin_motion(tracker, sample, reference_delta);
        }
    } else if (!is_quiet_sample(sample)) {
        reset_quiet_candidate(tracker, sample);
    } else if (sample.orientation != tracker.quiet_candidate ||
               acceleration_delta(sample, tracker.quiet_anchor) >
                   kQuietVectorDeltaG) {
        reset_quiet_candidate(tracker, sample);
    } else {
        ++tracker.quiet_sample_count;
    }

    // Commits only after one orientation and its gravity vector stay quiet.
    // 只有方向及其重力向量持续安静后，才提交最终放稳姿态。
    if ((!tracker.has_settled_reference || tracker.moving) &&
        tracker.quiet_sample_count >= kSettleSampleCount) {
        commit_settled_placement(tracker, sample);
    }

    const float step_delta = tracker.has_previous_sample
                                 ? acceleration_delta(
                                       sample, tracker.previous_sample)
                                 : 0.0F;
#if STICKY_LOG_MOTION_SAMPLES_ENABLED
    STICKY_LOGD(kTag,
                "imu=sample observed=%s settled=%s motion=%s magnitude_g=%.3f step_delta_g=%.3f quiet_samples=%d x_g=%.3f y_g=%.3f z_g=%.3f",
                orientation_name(sample.orientation),
                orientation_name(tracker.settled_orientation),
                tracker.moving ? "moving" : "still",
                static_cast<double>(acceleration_magnitude(sample)),
                static_cast<double>(step_delta),
                tracker.quiet_sample_count,
                static_cast<double>(sample.acceleration_x_g),
                static_cast<double>(sample.acceleration_y_g),
                static_cast<double>(sample.acceleration_z_g));
#else
    (void)step_delta;
#endif

    tracker.previous_sample = sample;
    tracker.has_previous_sample = true;
    sample.orientation = tracker.settled_orientation;
    sample.moving = tracker.moving;
}

esp_err_t read_acceleration(StickyImuState &state)
{
    uint8_t data[6] = {};
    const esp_err_t result =
        read_registers(kAccelerometerOutputRegister, data, sizeof(data));
    if (result != ESP_OK) {
        return result;
    }

    // Converts three little-endian raw axis values into acceleration in g.
    // 将三个小端格式的轴原始值换算成以g为单位的加速度。
    state.acceleration_x_g = read_int16_le(data) * kAccelerationScaleG;
    state.acceleration_y_g = read_int16_le(data + 2) * kAccelerationScaleG;
    state.acceleration_z_g = read_int16_le(data + 4) * kAccelerationScaleG;
    state.orientation = classify_orientation(state.acceleration_x_g,
                                             state.acceleration_y_g,
                                             state.acceleration_z_g);
    state.valid = true;
    return ESP_OK;
}

void store_state(const StickyImuState &state)
{
    taskENTER_CRITICAL(&s_state_lock);
    s_latest_state = state;
    taskEXIT_CRITICAL(&s_state_lock);
}

void monitor_task(void *)
{
    PlacementTracker tracker = {};
    TickType_t next_read = xTaskGetTickCount();

    while (true) {
        StickyImuState sample = {};
        const esp_err_t result = read_acceleration(sample);
        if (result == ESP_OK) {
            update_placement(tracker, sample);
            store_state(sample);
        } else {
            STICKY_LOGE(kTag,
                        "imu=read result=%s",
                        esp_err_to_name(result));
        }

        vTaskDelayUntil(&next_read, kPollInterval);
    }
}

}  // namespace

esp_err_t sticky_imu_init(i2c_master_bus_handle_t bus)
{
    app_log_register_tag(kTag);
    if (bus == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_device != nullptr) {
        return ESP_OK;
    }

    i2c_device_config_t config = {};
    config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    config.device_address = LSM6DS3_I2C_ADDR;
    config.scl_speed_hz = kI2cClockHz;

    esp_err_t result = i2c_master_bus_add_device(bus, &config, &s_device);
    if (result != ESP_OK) {
        return result;
    }

    uint8_t who_am_i = 0;
    result = read_registers(kWhoAmIRegister, &who_am_i, 1);
    if (result != ESP_OK || who_am_i != kExpectedWhoAmI) {
        STICKY_LOGE(kTag,
                    "imu=who_am_i value=0x%02X result=%s",
                    who_am_i,
                    result == ESP_OK ? "unexpected" : esp_err_to_name(result));
        i2c_master_bus_rm_device(s_device);
        s_device = nullptr;
        return result != ESP_OK ? result : ESP_ERR_NOT_FOUND;
    }

    // Configures 104 Hz, +/-2 g sampling and sequential register reads.
    // 配置104Hz、正负2g采样，并启用连续寄存器读取。
    result = write_register(
        kAccelerometerControlRegister, kAccelerometer104Hz2g);
    if (result == ESP_OK) {
        result = write_register(kControlRegister3, kRegisterAutoIncrement);
    }
    if (result != ESP_OK) {
        i2c_master_bus_rm_device(s_device);
        s_device = nullptr;
        return result;
    }

    vTaskDelay(pdMS_TO_TICKS(100));
    STICKY_LOGI(kTag,
                "imu=ready address=0x%02X who_am_i=0x%02X accelerometer_hz=104 range_g=2 result=ok",
                LSM6DS3_I2C_ADDR,
                who_am_i);
    return ESP_OK;
}

esp_err_t sticky_imu_start_monitoring()
{
    if (s_device == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_monitor_task != nullptr) {
        return ESP_OK;
    }

    const BaseType_t result = xTaskCreate(monitor_task,
                                          "imu_monitor",
                                          kTaskStackSize,
                                          nullptr,
                                          kTaskPriority,
                                          &s_monitor_task);
    if (result != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    STICKY_LOGI(kTag,
                "imu=monitoring interval_ms=100 settle_samples=%d motion_delta_g=%.2f quiet_delta_g=%.2f raw_samples=%d result=ok",
                kSettleSampleCount,
                static_cast<double>(kMotionStartDeltaG),
                static_cast<double>(kQuietVectorDeltaG),
                STICKY_LOG_MOTION_SAMPLES_ENABLED);
    return ESP_OK;
}

esp_err_t sticky_imu_get_state(StickyImuState &state)
{
    taskENTER_CRITICAL(&s_state_lock);
    state = s_latest_state;
    taskEXIT_CRITICAL(&s_state_lock);
    return state.valid ? ESP_OK : ESP_ERR_INVALID_STATE;
}
