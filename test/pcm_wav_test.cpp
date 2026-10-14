#include "pcm_wav.h"
#include <cassert>
#include <cstring>

int main()
{
    auto h = pcm_wav::header(pcm_wav::kPrototypeBytes);
    auto read = [&h](size_t at) {
        return uint32_t(h[at]) | (uint32_t(h[at + 1]) << 8) |
               (uint32_t(h[at + 2]) << 16) | (uint32_t(h[at + 3]) << 24);
    };
    assert(h.size() == 44 && std::memcmp(h.data(), "RIFF", 4) == 0);
    assert(std::memcmp(h.data() + 8, "WAVEfmt ", 8) == 0);
    assert(std::memcmp(h.data() + 36, "data", 4) == 0);
    assert(read(4) == 960036 && read(40) == 960000);
    assert(read(16) == 16 && read(24) == 16000 && read(28) == 32000);
    assert(h[20] == 1 && h[22] == 1 && h[32] == 2 && h[34] == 16);
    h = pcm_wav::header(0);
    assert(read(4) == 36 && read(40) == 0);
    const auto *sample = reinterpret_cast<const uint8_t *>("123456789");
    const auto crc = pcm_wav::crc32(UINT32_MAX, sample, 9);
    assert((crc ^ UINT32_MAX) == 0xcbf43926U);
    assert(pcm_wav::crc32(pcm_wav::crc32(UINT32_MAX, sample, 4), sample + 4, 5) == crc);
    assert(pcm_wav::crc32(crc, sample, 0) == crc);
}
