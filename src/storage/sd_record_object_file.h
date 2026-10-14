#pragma once

#include <cstddef>
#include <cstdint>

#include "storage_repository.h"

namespace sticky_storage {

struct SdRecordObjectPaths {
    const char *application_root = nullptr;
    const char *records_directory = nullptr;
    const char *object = nullptr;
};

enum class SdRecordObjectProbeStatus : uint8_t {
    Missing = 0,
    Match,
    Mismatch,
    IoError,
    InvalidArgument,
};

struct SdRecordObjectProbe {
    SdRecordObjectProbeStatus status =
        SdRecordObjectProbeStatus::InvalidArgument;
    uint64_t file_bytes = 0U;
    uint32_t file_crc32 = 0U;
};

enum class SdRecordObjectWriteIoResult : uint8_t {
    Ok = 0,
    OpenError,
    ShortWrite,
    SyncError,
    CloseError,
};

enum class SdRecordObjectWriteStatus : uint8_t {
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

struct SdRecordObjectWriteResult {
    SdRecordObjectWriteStatus status =
        SdRecordObjectWriteStatus::InvalidArgument;
    bool changed = false;
    SdRecordObjectProbe probe = {};
};

enum class SdRecordObjectReadStatus : uint8_t {
    Ok = 0,
    InvalidArgument,
    Missing,
    IoError,
    SizeMismatch,
    CrcMismatch,
};

struct SdRecordObjectReadResult {
    SdRecordObjectReadStatus status =
        SdRecordObjectReadStatus::InvalidArgument;
    size_t bytes = 0U;
    uint32_t crc32 = 0U;
};

struct SdRecordObjectFileOps {
    void *context = nullptr;
    bool (*ensure_directory)(void *context, const char *path) = nullptr;
    SdRecordObjectWriteIoResult (*write_object)(
        void *context, const char *path, const uint8_t *bytes,
        size_t size) = nullptr;
    SdRecordObjectProbe (*probe_object)(
        void *context, const char *path, const uint8_t *expected,
        size_t expected_size) = nullptr;
};

// Reads a bounded immutable object without modifying it. Match requires exact
// length and byte equality; a partial or different existing file is Mismatch.
SdRecordObjectProbe sd_record_object_probe(
    const char *path, const uint8_t *expected, size_t expected_size);
// Verifies an immutable object by its journal/index reference without keeping
// the full object payload in memory.
SdRecordObjectProbe sd_record_object_probe_reference(
    const char *path, uint32_t expected_bytes, uint32_t expected_crc32);
// Reads exactly one journal-referenced object into caller-owned bounded
// storage. Bytes are usable only when status is Ok; size and CRC must both
// match the committed reference.
SdRecordObjectReadResult sd_record_object_read_reference(
    const char *path, uint32_t expected_bytes, uint32_t expected_crc32,
    uint8_t *output, size_t output_capacity);

// Creates one immutable object, flushes/fsyncs/closes it and verifies an exact
// readback. Exact retries are idempotent. A partial or different existing path
// is never overwritten or repaired automatically.
SdRecordObjectWriteResult sd_record_object_write(
    const SdRecordObjectPaths &paths, const uint8_t *bytes, size_t size);
SdRecordObjectWriteResult sd_record_object_write_with_ops(
    const SdRecordObjectPaths &paths, const uint8_t *bytes, size_t size,
    const SdRecordObjectFileOps &ops);

}  // namespace sticky_storage
