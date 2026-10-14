#pragma once

#include <cstddef>
#include <cstdint>

namespace sticky_storage {

enum class Error : uint8_t {
    Ok = 0,
    CardMissing,
    CardReadOnly,
    NoSpace,
    AudioQuotaReached,
    Busy,
    Io,
    CorruptRecord,
    UnsupportedFormat,
    InvalidRecord,
};

// S1 contracts. User-visible labels remain UTF-8 record fields; physical FAT
// paths use generated ASCII 8.3 names, so long-filename support is not needed.
constexpr size_t kMaxOpenFiles = 4U;
constexpr size_t kStorageQueueDepth = 8U;
constexpr size_t kMaxPathBytes = 64U;
constexpr size_t kMaxUserTextBytes = 800U;  // 200 UTF-8 code points, worst case.
constexpr uint32_t kAudioItemLimit = 100U;
constexpr uint64_t kAudioByteLimit = 128ULL * 1024ULL * 1024ULL;
constexpr uint64_t kBusinessReserveBytes = 32ULL * 1024ULL * 1024ULL;
constexpr uint64_t kWorstCaseRecordingBytes = 30ULL * 16000ULL * 2ULL + 44ULL;

constexpr bool can_start_audio_recording(uint32_t audio_items,
                                         uint64_t audio_bytes,
                                         uint64_t free_bytes)
{
    return audio_items < kAudioItemLimit &&
           audio_bytes <= kAudioByteLimit &&
           kWorstCaseRecordingBytes <= kAudioByteLimit - audio_bytes &&
           free_bytes >= kBusinessReserveBytes + kWorstCaseRecordingBytes;
}

}  // namespace sticky_storage
