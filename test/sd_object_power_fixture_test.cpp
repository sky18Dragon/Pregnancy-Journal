#include <array>
#include <cassert>

#include "sd_object_power_fixture.h"
#include "storage_crc32.h"

int main()
{
    using namespace sticky_storage;
    static_assert(kObjectPowerPartialFixtureBytes <
                  kObjectPowerFixtureBytes);
    std::array<uint8_t, kObjectPowerFixtureBytes> bytes = {};
    sd_object_power_fixture_bytes(bytes);
    assert(sd_object_power_fixture_crc32() ==
           crc32(bytes.data(), bytes.size()));
    assert(sd_object_power_partial_fixture_crc32() ==
           crc32(bytes.data(), kObjectPowerPartialFixtureBytes));
    assert(sd_object_power_partial_fixture_crc32() !=
           sd_object_power_fixture_crc32());
    return 0;
}
