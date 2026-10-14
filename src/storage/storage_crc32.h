#pragma once

#include <cstddef>
#include <cstdint>

namespace sticky_storage {

constexpr uint32_t kCrc32Initial = UINT32_MAX;
constexpr uint32_t kCrc32FinalXor = UINT32_MAX;

// Incremental CRC32/IEEE. Keep the accumulator unfinalized between chunks.
inline uint32_t crc32_update(uint32_t crc, const uint8_t *bytes, size_t count)
{
    for (size_t index = 0U; index < count; ++index) {
        crc ^= bytes[index];
        for (uint8_t bit = 0U; bit < 8U; ++bit) {
            crc = (crc >> 1U) ^
                  ((crc & 1U) != 0U ? 0xEDB88320U : 0U);
        }
    }
    return crc;
}

inline uint32_t crc32(const uint8_t *bytes, size_t count)
{
    return crc32_update(kCrc32Initial, bytes, count) ^ kCrc32FinalXor;
}

}  // namespace sticky_storage
