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
constexpr int kStableSampleCount = 5;
constexpr TickType_t kPollInterval = pdMS_TO_TICKS(100);
constexpr uint32_t kTaskStackSize = 3072;
constexpr UBaseType_t kTaskPriority = 4;

i2c_master_dev_handle_t s_device = nullptr;
TaskHandle_t s_monitor_task = nullptr;
portMUX_TYPE s_state_lock = portMUX_INITIALIZER_UNLOCKED;
StickyImuState s_latest_state = {};

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
        // Calibration: -X is the normal landscape down direction.
        // 校准结果：横向放置时-X指向物理下方。
        return x < 0.0F ? StickyImuOrientation::ArrowDown
                        : StickyImuOrientation::ArrowUp;
    }
    if (abs_y >= kOrientationThresholdG) {
        // Calibration: portrait -Y points toward the physical ground.
        // 校准结果：竖向放置时-Y指向物理下方。
        return y < 0.0F ? StickyImuOrientation::ArrowRight
                        : StickyImuOrientation::ArrowLeft;
    }
    return StickyImuOrientation::Unknown;
}

const char *orientation_name(StickyImuOrientation orientation)
{
    switch (orientation) {
    case StickyImuOrientation::ArrowUp:
        return "arrow_up";
    case StickyImuOrientation::ArrowDown:
        return "arrow_down";
    case StickyImuOrientation::ArrowLeft:
        return "arrow_left";
    case StickyImuOrientation::ArrowRight:
        return "arrow_right";
    case StickyImuOrientation::FaceUp:
        return "face_up";
    case StickyImuOrientation::FaceDown:
        return "face_down";
    case StickyImuOrientation::Unknown:
    default:
        return "unknown";
    }
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
    StickyImuOrientation candidate = StickyImuOrientation::Unknown;
    StickyImuOrientation stable = StickyImuOrientation::Unknown;
    int candidate_count = 0;
    TickType_t next_read = xTaskGetTickCount();

    while (true) {
        StickyImuState sample = {};
        const esp_err_t result = read_acceleration(sample);
        if (result == ESP_OK) {
            // Counts consecutive samples of the same candidate orientation.
            // 统计连续出现同一候选姿态的采样次数。
            if (sample.orientation == candidate) {
                ++candidate_count;
            } else {
                candidate = sample.orientation;
                candidate_count = 1;
            }

            // Publishes and logs a stable orientation only when it changes.
            // 仅在姿态连续稳定且发生变化时保存状态并输出一次日志。
            if (candidate_count >= kStableSampleCount && candidate != stable) {
                stable = candidate;
                sample.orientation = stable;
                store_state(sample);
                STICKY_LOGI(kTag,
                            "imu=orientation value=%s x_g=%.3f y_g=%.3f z_g=%.3f stable_samples=%d",
                            orientation_name(stable),
                            static_cast<double>(sample.acceleration_x_g),
                            static_cast<double>(sample.acceleration_y_g),
                            static_cast<double>(sample.acceleration_z_g),
                            kStableSampleCount);
            } else {
                sample.orientation = stable;
                store_state(sample);
            }
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
                "imu=monitoring interval_ms=100 stable_samples=%d result=ok",
                kStableSampleCount);
    return ESP_OK;
}

esp_err_t sticky_imu_get_state(StickyImuState &state)
{
    taskENTER_CRITICAL(&s_state_lock);
    state = s_latest_state;
    taskEXIT_CRITICAL(&s_state_lock);
    return state.valid ? ESP_OK : ESP_ERR_INVALID_STATE;
}
