#pragma once

#include <cstdint>

namespace StickyBatteryProtocol {

constexpr uint8_t kControlRegister = 0x00U;
constexpr uint8_t kMacDataRegister = 0x40U;
constexpr uint8_t kStateOfChargeRegister = 0x2CU;
constexpr uint16_t kDeviceTypeCommand = 0x0001U;
constexpr uint16_t kExpectedDeviceId = 0x0220U;

// Decodes the little-endian words returned by BQ27220 standard commands.
// 解码BQ27220标准命令返回的低字节在前数据。
constexpr uint16_t decode_word(uint8_t low, uint8_t high)
{
    return static_cast<uint16_t>(low) |
           static_cast<uint16_t>(static_cast<uint16_t>(high) << 8U);
}

}  // namespace StickyBatteryProtocol
