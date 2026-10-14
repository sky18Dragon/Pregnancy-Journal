#include "sd_storage_policy.h"

#include <cstring>

#include "sd_storage_layout.h"
#include "storage_contract.h"

namespace sticky_storage {

bool sd_dataset_binding_make(uint32_t mount_generation,
                             const char *dataset_id,
                             SdDatasetBinding &binding)
{
    binding = {};
    if (mount_generation == 0U || !sd_dataset_id_valid(dataset_id)) {
        return false;
    }
    binding.mount_generation = mount_generation;
    std::memcpy(binding.dataset_id, dataset_id, sizeof(binding.dataset_id));
    return true;
}

SdStoragePolicyStatus sd_dataset_binding_validate(
    const SdDatasetBinding &binding, uint32_t current_mount_generation,
    const char *current_dataset_id)
{
    if (binding.mount_generation == 0U ||
        !sd_dataset_id_valid(binding.dataset_id) ||
        current_mount_generation == 0U ||
        !sd_dataset_id_valid(current_dataset_id)) {
        return SdStoragePolicyStatus::InvalidArgument;
    }
    if (binding.mount_generation != current_mount_generation) {
        return SdStoragePolicyStatus::StaleGeneration;
    }
    return std::strcmp(binding.dataset_id, current_dataset_id) == 0
               ? SdStoragePolicyStatus::Ok
               : SdStoragePolicyStatus::DatasetMismatch;
}

SdStoragePolicyStatus sd_storage_validate_owned_path(const char *path)
{
    if (path == nullptr || path[0] == '\0') {
        return SdStoragePolicyStatus::InvalidArgument;
    }
    const size_t length = std::strlen(path);
    if (length >= kMaxPathBytes) {
        return SdStoragePolicyStatus::InvalidPhysicalPath;
    }
    const size_t root_length = std::strlen(kApplicationRootPath);
    if (std::strncmp(path, kApplicationRootPath, root_length) != 0 ||
        (path[root_length] != '\0' && path[root_length] != '/')) {
        return SdStoragePolicyStatus::PathOutsideApplicationRoot;
    }
    if (path[root_length] == '\0') {
        return SdStoragePolicyStatus::Ok;
    }

    const char *component = path + root_length + 1U;
    while (*component != '\0') {
        const char *separator = std::strchr(component, '/');
        const size_t component_length =
            separator == nullptr
                ? std::strlen(component)
                : static_cast<size_t>(separator - component);
        if (component_length == 0U || component_length > 12U) {
            return SdStoragePolicyStatus::InvalidPhysicalPath;
        }
        char name[13U] = {};
        std::memcpy(name, component, component_length);
        if (!fat83_name_valid(name)) {
            return SdStoragePolicyStatus::InvalidPhysicalPath;
        }
        if (separator == nullptr) {
            break;
        }
        component = separator + 1U;
    }
    return SdStoragePolicyStatus::Ok;
}

SdStoragePolicyStatus sd_storage_check_write(
    SdStorageWriteClass write_class, uint64_t requested_bytes,
    const SdStorageUsage &usage)
{
    if (requested_bytes == 0U) {
        return SdStoragePolicyStatus::InvalidArgument;
    }
    if (write_class == SdStorageWriteClass::Business) {
        return usage.free_bytes >= requested_bytes
                   ? SdStoragePolicyStatus::Ok
                   : SdStoragePolicyStatus::NoSpace;
    }
    if (write_class != SdStorageWriteClass::Audio) {
        return SdStoragePolicyStatus::InvalidArgument;
    }
    if (usage.audio_items >= kAudioItemLimit ||
        usage.audio_bytes > kAudioByteLimit ||
        requested_bytes > kAudioByteLimit - usage.audio_bytes) {
        return SdStoragePolicyStatus::AudioQuotaReached;
    }
    if (requested_bytes > UINT64_MAX - kBusinessReserveBytes ||
        usage.free_bytes < kBusinessReserveBytes + requested_bytes) {
        return SdStoragePolicyStatus::NoSpace;
    }
    return SdStoragePolicyStatus::Ok;
}

}  // namespace sticky_storage
