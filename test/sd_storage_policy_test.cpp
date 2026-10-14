#include <cassert>
#include <cstdint>
#include <cstring>

#include "sd_storage_layout.h"
#include "sd_storage_policy.h"
#include "storage_contract.h"

int main()
{
    using namespace sticky_storage;
    constexpr char dataset_a[] = "0123456789abcdef0123456789abcdef";
    constexpr char dataset_b[] = "fedcba9876543210fedcba9876543210";

    SdDatasetBinding binding = {};
    assert(sd_dataset_binding_make(4U, dataset_a, binding));
    assert(binding.mount_generation == 4U);
    assert(std::strcmp(binding.dataset_id, dataset_a) == 0);
    assert(sd_dataset_binding_validate(binding, 4U, dataset_a) ==
           SdStoragePolicyStatus::Ok);
    assert(sd_dataset_binding_validate(binding, 5U, dataset_a) ==
           SdStoragePolicyStatus::StaleGeneration);
    assert(sd_dataset_binding_validate(binding, 4U, dataset_b) ==
           SdStoragePolicyStatus::DatasetMismatch);
    assert(!sd_dataset_binding_make(0U, dataset_a, binding));
    assert(!sd_dataset_binding_make(1U, "invalid", binding));

    assert(sd_storage_validate_owned_path(kApplicationRootPath) ==
           SdStoragePolicyStatus::Ok);
    assert(sd_storage_validate_owned_path(kManifestSlotAPath) ==
           SdStoragePolicyStatus::Ok);
    assert(sd_storage_validate_owned_path(kJournalFirstSegmentPath) ==
           SdStoragePolicyStatus::Ok);
    assert(sd_storage_validate_owned_path(kRecordsDirectoryPath) ==
           SdStoragePolicyStatus::Ok);
    assert(sd_storage_validate_owned_path(
               "/sdcard/PREGJNL/RECORDS/REC00001.BIN") ==
           SdStoragePolicyStatus::Ok);
    assert(sd_storage_validate_owned_path("/sdcard/OTHER/FILE.BIN") ==
           SdStoragePolicyStatus::PathOutsideApplicationRoot);
    assert(sd_storage_validate_owned_path("/sdcard/PREGJNLX/FILE.BIN") ==
           SdStoragePolicyStatus::PathOutsideApplicationRoot);
    assert(sd_storage_validate_owned_path("/sdcard/PREGJNL/../OTHER") ==
           SdStoragePolicyStatus::InvalidPhysicalPath);
    assert(sd_storage_validate_owned_path("/sdcard/PREGJNL//FILE.BIN") ==
           SdStoragePolicyStatus::InvalidPhysicalPath);
    assert(sd_storage_validate_owned_path("/sdcard/PREGJNL/lower.bin") ==
           SdStoragePolicyStatus::InvalidPhysicalPath);
    assert(sd_storage_validate_owned_path(
               "/sdcard/PREGJNL/TOOLONG99.BIN") ==
           SdStoragePolicyStatus::InvalidPhysicalPath);
    assert(sd_storage_validate_owned_path(nullptr) ==
           SdStoragePolicyStatus::InvalidArgument);

    SdStorageUsage usage = {};
    usage.free_bytes = kBusinessReserveBytes + kWorstCaseRecordingBytes;
    assert(sd_storage_check_write(SdStorageWriteClass::Audio,
                                  kWorstCaseRecordingBytes, usage) ==
           SdStoragePolicyStatus::Ok);
    usage.free_bytes -= 1U;
    assert(sd_storage_check_write(SdStorageWriteClass::Audio,
                                  kWorstCaseRecordingBytes, usage) ==
           SdStoragePolicyStatus::NoSpace);
    usage.free_bytes = UINT64_MAX;
    usage.audio_items = kAudioItemLimit;
    assert(sd_storage_check_write(SdStorageWriteClass::Audio, 1U, usage) ==
           SdStoragePolicyStatus::AudioQuotaReached);
    usage.audio_items = 0U;
    usage.audio_bytes = kAudioByteLimit;
    assert(sd_storage_check_write(SdStorageWriteClass::Audio, 1U, usage) ==
           SdStoragePolicyStatus::AudioQuotaReached);

    usage = {};
    usage.free_bytes = 100U;
    assert(sd_storage_check_write(SdStorageWriteClass::Business, 100U,
                                  usage) == SdStoragePolicyStatus::Ok);
    assert(sd_storage_check_write(SdStorageWriteClass::Business, 101U,
                                  usage) ==
           SdStoragePolicyStatus::NoSpace);
    assert(sd_storage_check_write(SdStorageWriteClass::Business, 0U,
                                  usage) ==
           SdStoragePolicyStatus::InvalidArgument);
    return 0;
}
