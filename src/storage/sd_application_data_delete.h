#pragma once

#include <cstdint>

namespace sticky_storage {

enum class SdApplicationDataDeleteStatus : uint8_t {
    Ok = 0,
    Missing,
    InvalidArgument,
    PathTooLong,
    DepthExceeded,
    IoError,
};

struct SdApplicationDataDeleteResult {
    SdApplicationDataDeleteStatus status =
        SdApplicationDataDeleteStatus::InvalidArgument;
    uint32_t files_removed = 0U;
    uint32_t directories_removed = 0U;
};

struct SdApplicationDataDeleteOps {
    int (*remove_file)(const char *) = nullptr;
    int (*remove_directory)(const char *) = nullptr;
};

SdApplicationDataDeleteResult sd_application_data_delete_with_ops(
    const char *mount_root, const char *application_directory,
    const SdApplicationDataDeleteOps &ops);

// Deletes exactly <mount_root>/PREGJNL. Directory entries beneath that root
// are removed on the FAT volume. Siblings of PREGJNL are never inspected or
// modified. Missing application data is an idempotent success.
SdApplicationDataDeleteResult sd_application_data_delete(
    const char *mount_root, const char *application_directory);

}  // namespace sticky_storage
