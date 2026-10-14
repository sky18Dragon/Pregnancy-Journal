#include "sd_object_power_fixture.h"

#include <cstring>

#include "storage_crc32.h"

namespace sticky_storage {

void sd_object_power_fixture_bytes(
    std::array<uint8_t, kObjectPowerFixtureBytes> &bytes)
{
    bytes.fill(0U);
    constexpr uint8_t header[] = {'P', 'J', 'O', 'P', 1U, 0U, 0U, 0U};
    std::memcpy(bytes.data(), header, sizeof(header));
    for (size_t index = sizeof(header); index < bytes.size(); ++index) {
        bytes[index] = static_cast<uint8_t>((index * 73U + 29U) & 0xFFU);
    }
}

uint32_t sd_object_power_fixture_crc32()
{
    std::array<uint8_t, kObjectPowerFixtureBytes> bytes = {};
    sd_object_power_fixture_bytes(bytes);
    return crc32(bytes.data(), bytes.size());
}

uint32_t sd_object_power_partial_fixture_crc32()
{
    std::array<uint8_t, kObjectPowerFixtureBytes> bytes = {};
    sd_object_power_fixture_bytes(bytes);
    return crc32(bytes.data(), kObjectPowerPartialFixtureBytes);
}

}  // namespace sticky_storage
