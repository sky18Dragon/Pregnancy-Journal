#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "storage_repository.h"

namespace sticky_storage {

constexpr uint16_t kSdIndexSnapshotSchemaVersion = 1U;
constexpr size_t kSdIndexSnapshotHeaderBytes = 80U;
constexpr size_t kSdIndexSnapshotEntryBytes = 64U;
constexpr size_t kSdIndexSnapshotEntryCapacity = 1024U;
constexpr size_t kSdIndexSnapshotMaxBytes =
    kSdIndexSnapshotHeaderBytes +
    kSdIndexSnapshotEntryCapacity * kSdIndexSnapshotEntryBytes;
constexpr size_t kSdIndexObjectNameBytes = 12U;

struct SdIndexSnapshotEntry {
    RepositoryKey key = {};
    uint32_t revision = 0U;
    bool tombstone = false;
    uint32_t object_bytes = 0U;
    uint32_t object_crc32 = 0U;
    char object_file[kSdIndexObjectNameBytes + 1U] = {};
};

struct SdIndexSnapshot {
    char dataset_id[kDatasetIdCharacters + 1U] = {};
    uint64_t last_transaction_id = 0U;
    std::array<SdIndexSnapshotEntry, kSdIndexSnapshotEntryCapacity> entries = {};
    size_t entry_count = 0U;

    // Reset in place without a maximum-size temporary on the task stack.
    // 原地重置，避免在任务栈上创建完整快照大小的临时对象。
    void reset();
};

enum class SdIndexSnapshotStatus : uint8_t {
    Ok = 0,
    InvalidArgument,
    Incomplete,
    Corrupt,
    UnsupportedSchema,
};

using SdIndexSnapshotEntryProvider = bool (*)(
    void *context, size_t index, SdIndexSnapshotEntry &entry);

// Encodes entries one at a time so firmware callers do not need to place the
// maximum-size SdIndexSnapshot aggregate on a constrained task stack.
size_t sd_index_snapshot_encode_entries(
    const char *dataset_id, uint64_t last_transaction_id,
    size_t entry_count, SdIndexSnapshotEntryProvider provider,
    void *provider_context, uint8_t *output, size_t output_capacity);
size_t sd_index_snapshot_encode(const SdIndexSnapshot &snapshot,
                                uint8_t *output, size_t output_capacity);
// Validates directly from the encoded buffer with constant stack usage.
SdIndexSnapshotStatus sd_index_snapshot_validate(const uint8_t *bytes,
                                                 size_t size);
SdIndexSnapshotStatus sd_index_snapshot_decode(const uint8_t *bytes,
                                               size_t size,
                                               SdIndexSnapshot &snapshot);

struct SdIndexFilePaths {
    const char *application_root = nullptr;
    const char *index_directory = nullptr;
    const char *index_file = nullptr;
};

enum class SdIndexFileProbeStatus : uint8_t {
    Missing = 0,
    Match,
    Mismatch,
    IoError,
    InvalidArgument,
};

struct SdIndexFileProbe {
    SdIndexFileProbeStatus status = SdIndexFileProbeStatus::InvalidArgument;
    uint64_t file_bytes = 0U;
    uint32_t file_crc32 = 0U;
};

enum class SdIndexWriteIoResult : uint8_t {
    Ok = 0,
    OpenError,
    ShortWrite,
    SyncError,
    CloseError,
};

enum class SdIndexWriteStatus : uint8_t {
    Ok = 0,
    InvalidArgument,
    DirectoryError,
    ProbeIoError,
    ExistingConflict,
    OpenError,
    ShortWrite,
    SyncError,
    CloseError,
    VerifyError,
};

struct SdIndexWriteResult {
    SdIndexWriteStatus status = SdIndexWriteStatus::InvalidArgument;
    bool changed = false;
    SdIndexFileProbe probe = {};
};

struct SdIndexFileOps {
    void *context = nullptr;
    bool (*ensure_directory)(void *context, const char *path) = nullptr;
    SdIndexWriteIoResult (*write_file)(void *context, const char *path,
                                       const uint8_t *bytes,
                                       size_t size) = nullptr;
    SdIndexFileProbe (*probe_file)(void *context, const char *path,
                                   const uint8_t *expected,
                                   size_t expected_size) = nullptr;
};

// Index files are immutable checkpoint candidates. A complete exact retry is
// idempotent; any existing partial or conflicting file is preserved and
// blocks reuse of that name.
SdIndexFileProbe sd_index_file_probe(const char *path,
                                     const uint8_t *expected,
                                     size_t expected_size);
SdIndexWriteResult sd_index_file_write_with_ops(
    const SdIndexFilePaths &paths, const uint8_t *bytes, size_t size,
    const SdIndexFileOps &ops);
SdIndexWriteResult sd_index_file_write(const SdIndexFilePaths &paths,
                                       const uint8_t *bytes, size_t size);

}  // namespace sticky_storage
