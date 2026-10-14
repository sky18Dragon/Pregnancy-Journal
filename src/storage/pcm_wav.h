#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "storage_crc32.h"

namespace pcm_wav {
constexpr uint32_t kSampleRate = 16000;
constexpr uint32_t kBytesPerSecond = kSampleRate * 2;
constexpr uint32_t kPrototypeBytes = kBytesPerSecond * 30;

// Fixed little-endian PCM16 mono header, independent of host struct packing.
inline std::array<uint8_t, 44> header(uint32_t data_bytes)
{
    std::array<uint8_t, 44> h{};
    auto word = [&h](size_t at, uint32_t value, size_t count) {
        for (size_t i = 0; i < count; ++i) h[at + i] = (value >> (i * 8)) & 0xff;
    };
    const char *riff = "RIFF", *wave = "WAVEfmt ", *data = "data";
    for (size_t i = 0; i < 4; ++i) h[i] = riff[i];
    for (size_t i = 0; i < 8; ++i) h[8 + i] = wave[i];
    for (size_t i = 0; i < 4; ++i) h[36 + i] = data[i];
    word(4, data_bytes + 36, 4);
    word(16, 16, 4);
    word(20, 1, 2);
    word(22, 1, 2);
    word(24, kSampleRate, 4);
    word(28, kBytesPerSecond, 4);
    word(32, 2, 2);
    word(34, 16, 2);
    word(40, data_bytes, 4);
    return h;
}

// Incremental CRC32 (IEEE); initialize to UINT32_MAX, xor at completion.
inline uint32_t crc32(uint32_t crc, const uint8_t *bytes, size_t count)
{
    return sticky_storage::crc32_update(crc, bytes, count);
}
}  // namespace pcm_wav
