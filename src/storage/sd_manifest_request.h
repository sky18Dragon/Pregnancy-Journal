#pragma once

#include <cstdint>

#include "sd_card_state.h"
#include "sd_manifest_writer.h"

namespace sticky_storage {

enum class SdManifestRequestOperation : uint8_t {
    Initialize = 0,
    CommitNext,
};

enum class SdManifestRequestStatus : uint8_t {
    Ok = 0,
    InvalidArgument,
    ConfirmationRequired,
    CardMissing,
    CardReadOnly,
    NoSpace,
    CardUnavailable,
    StaleGeneration,
    AlreadyInitialized,
    NotInitialized,
    DatasetMismatch,
    GenerationMismatch,
    GenerationOverflow,
    Corrupt,
    IoError,
    DirectoryError,
    OpenError,
    ShortWrite,
    SyncError,
    CloseError,
    VerifyError,
};

struct SdManifestRequest {
    SdManifestRequestOperation operation =
        SdManifestRequestOperation::Initialize;
    uint32_t mount_generation = 0U;
    bool user_confirmed = false;
    SdManifest manifest = {};
};

struct SdManifestRequestContext {
    SdCardState card_state = SdCardState::Absent;
    uint32_t mount_generation = 0U;
};

struct SdManifestRequestResult {
    SdManifestRequestStatus status =
        SdManifestRequestStatus::InvalidArgument;
    bool changed = false;
    SdManifest manifest = {};
    SdManifestSlot slot = SdManifestSlot::A;
};

bool sd_manifest_make_initialize_request(uint32_t mount_generation,
                                         const char *dataset_id,
                                         bool user_confirmed,
                                         SdManifestRequest &request);
bool sd_manifest_make_commit_request(uint32_t mount_generation,
                                     const SdManifest &manifest,
                                     SdManifestRequest &request);

// Executes one already-queued request. The storage worker must be the only
// caller that can touch the mounted filesystem, so the card state cannot
// change concurrently during this call.
SdManifestRequestResult sd_manifest_execute_request(
    const SdManifestRequest &request,
    const SdManifestRequestContext &context,
    const SdManifestPaths &paths);
SdManifestRequestResult sd_manifest_execute_request_with_ops(
    const SdManifestRequest &request,
    const SdManifestRequestContext &context, const SdManifestPaths &paths,
    const SdManifestWriterOps &ops);

}  // namespace sticky_storage
