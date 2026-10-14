#pragma once

#include <cstddef>
#include <cstdint>

namespace sticky_storage {

constexpr size_t kSdAudioInventoryEntryLimit = 256U;

enum class SdAudioUsageStatus : uint8_t {
    Ready = 0,
    Missing,
    InvalidArgument,
    UnexpectedEntry,
    TooManyEntries,
    Overflow,
    IoError,
};

struct SdAudioUsageProbe {
    SdAudioUsageStatus status = SdAudioUsageStatus::InvalidArgument;
    uint32_t items = 0U;
    uint64_t bytes = 0U;
};

// Read-only bounded inventory for AUDIO. Missing means a valid empty dataset.
// Unknown names, nested directories and scan overflow block new recordings so
// quota enforcement never silently ignores bytes already on the card.
SdAudioUsageProbe sd_audio_usage_probe(const char *directory);

}  // namespace sticky_storage
