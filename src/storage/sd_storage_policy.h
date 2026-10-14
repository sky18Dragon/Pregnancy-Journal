#pragma once

#include <cstddef>
#include <cstdint>

#include "sd_manifest.h"

namespace sticky_storage {

enum class SdStorageWriteClass : uint8_t {
    Business = 0,
    Audio,
};

enum class SdStoragePolicyStatus : uint8_t {
    Ok = 0,
    InvalidArgument,
    DatasetMismatch,
    StaleGeneration,
    PathOutsideApplicationRoot,
    InvalidPhysicalPath,
    CardUnavailable,
    CardReadOnly,
    CapacityUnavailable,
    UsageUnavailable,
    NoSpace,
    AudioQuotaReached,
};

struct SdDatasetBinding {
    uint32_t mount_generation = 0U;
    char dataset_id[kDatasetIdCharacters + 1U] = {};
};

struct SdStorageUsage {
    uint64_t free_bytes = 0U;
    uint32_t audio_items = 0U;
    uint64_t audio_bytes = 0U;
};

bool sd_dataset_binding_make(uint32_t mount_generation,
                             const char *dataset_id,
                             SdDatasetBinding &binding);
SdStoragePolicyStatus sd_dataset_binding_validate(
    const SdDatasetBinding &binding, uint32_t current_mount_generation,
    const char *current_dataset_id);

// Accepts only the fixed application root itself or uppercase ASCII 8.3
// descendants below it. Prefix lookalikes, traversal and empty components are
// rejected before any filesystem operation.
SdStoragePolicyStatus sd_storage_validate_owned_path(const char *path);

// Business writes may consume the reserved business area but still need their
// entire requested byte count. Audio writes additionally preserve the business
// reserve and respect both the item and byte quotas.
SdStoragePolicyStatus sd_storage_check_write(
    SdStorageWriteClass write_class, uint64_t requested_bytes,
    const SdStorageUsage &usage);

}  // namespace sticky_storage
