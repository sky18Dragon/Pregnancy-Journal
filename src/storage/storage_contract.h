#pragma once

#include <cstddef>
#include <cstdint>

namespace sticky_storage {

enum class Error : uint8_t {
    Ok = 0,
    CardMissing,
    CardReadOnly,
    NoSpace,
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

}  // namespace sticky_storage
