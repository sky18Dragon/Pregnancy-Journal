#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace sticky_storage {

inline constexpr char kObjectPowerFixtureFile[] = "OBJPWR01.BIN";
inline constexpr char kObjectPartialFixtureFile[] = "OBJPWR02.BIN";
constexpr size_t kObjectPowerFixtureBytes = 1024U;
constexpr size_t kObjectPowerPartialFixtureBytes = 512U;

void sd_object_power_fixture_bytes(
    std::array<uint8_t, kObjectPowerFixtureBytes> &bytes);
uint32_t sd_object_power_fixture_crc32();
uint32_t sd_object_power_partial_fixture_crc32();

}  // namespace sticky_storage
