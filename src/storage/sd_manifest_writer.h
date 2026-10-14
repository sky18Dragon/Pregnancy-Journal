#pragma once

#include <cstddef>
#include <cstdint>

#include "sd_manifest_reader.h"

namespace sticky_storage {

struct SdManifestPaths {
    const char *application_root = nullptr;
    const char *manifest_directory = nullptr;
    const char *slot_a = nullptr;
    const char *slot_b = nullptr;
};

enum class SdManifestWriteIoResult : uint8_t {
    Ok = 0,
    OpenError,
    ShortWrite,
    SyncError,
    CloseError,
};

enum class SdManifestCommitStatus : uint8_t {
    Ok = 0,
    InvalidArgument,
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

struct SdManifestCommitResult {
    SdManifestCommitStatus status = SdManifestCommitStatus::InvalidArgument;
    bool changed = false;
    SdManifest manifest = {};
    SdManifestSlot slot = SdManifestSlot::A;
};

struct SdManifestWriterOps {
    void *context = nullptr;
    bool (*ensure_directory)(void *context, const char *path) = nullptr;
    SdManifestWriteIoResult (*write_record)(void *context, const char *path,
                                            const uint8_t *bytes,
                                            size_t size) = nullptr;
    SdManifestSlotRead (*read_slot)(void *context, const char *path) = nullptr;
};

// Explicit, idempotent initialization entry point. It never formats media and
// refuses to overwrite a corrupt or differently identified dataset.
SdManifestCommitResult sd_manifest_initialize(
    const SdManifestPaths &paths, const char *dataset_id);
SdManifestCommitResult sd_manifest_initialize_with_ops(
    const SdManifestPaths &paths, const char *dataset_id,
    const SdManifestWriterOps &ops);

// Commits exactly the next generation to the inactive A/B slot. The previous
// valid copy remains untouched until the new copy is synced, closed and read
// back successfully. Repeating an already committed request is idempotent.
SdManifestCommitResult sd_manifest_commit_next(
    const SdManifestPaths &paths, const SdManifest &next);
SdManifestCommitResult sd_manifest_commit_next_with_ops(
    const SdManifestPaths &paths, const SdManifest &next,
    const SdManifestWriterOps &ops);

}  // namespace sticky_storage
