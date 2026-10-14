#include "sd_record_object_file.h"

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>

#include <sys/stat.h>
#include <unistd.h>

#include "sd_storage_layout.h"
#include "storage_crc32.h"

namespace sticky_storage {
namespace {

bool valid_path(const char *path)
{
    return path != nullptr && path[0] != '\0';
}

bool valid_paths(const SdRecordObjectPaths &paths)
{
    if (!valid_path(paths.application_root) ||
        !valid_path(paths.records_directory) || !valid_path(paths.object)) {
        return false;
    }
    char records[160U] = {};
    const int written = std::snprintf(
        records, sizeof(records), "%s/%s", paths.application_root,
        kRecordsDirectory);
    if (written <= 0 || static_cast<size_t>(written) >= sizeof(records) ||
        std::strcmp(records, paths.records_directory) != 0) {
        return false;
    }
    const size_t prefix = std::strlen(paths.records_directory);
    const size_t object_length = std::strlen(paths.object);
    if (object_length <= prefix) {
        return false;
    }
    if (std::strncmp(paths.object, paths.records_directory, prefix) != 0 ||
        paths.object[prefix] != '/') {
        return false;
    }
    const char *name = paths.object + prefix + 1U;
    return fat83_name_valid(name) && std::strchr(name, '/') == nullptr;
}

bool valid_object(const uint8_t *bytes, size_t size)
{
    return bytes != nullptr && size > 0U &&
           size <= kRepositoryPayloadBytes;
}

bool valid_ops(const SdRecordObjectFileOps &ops)
{
    return ops.ensure_directory != nullptr && ops.write_object != nullptr &&
           ops.probe_object != nullptr;
}

bool ensure_directory_default(void *, const char *path)
{
    if (::mkdir(path, 0775) == 0) {
        return true;
    }
    if (errno != EEXIST) {
        return false;
    }
    struct stat info = {};
    return ::stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

SdRecordObjectWriteIoResult write_object_default(
    void *, const char *path, const uint8_t *bytes, size_t size)
{
    std::FILE *file = std::fopen(path, "wb");
    if (file == nullptr) {
        return SdRecordObjectWriteIoResult::OpenError;
    }
    SdRecordObjectWriteIoResult result =
        SdRecordObjectWriteIoResult::Ok;
    if (std::fwrite(bytes, 1U, size, file) != size) {
        result = SdRecordObjectWriteIoResult::ShortWrite;
    } else if (std::fflush(file) != 0 || ::fsync(fileno(file)) != 0) {
        result = SdRecordObjectWriteIoResult::SyncError;
    }
    if (std::fclose(file) != 0 &&
        result == SdRecordObjectWriteIoResult::Ok) {
        result = SdRecordObjectWriteIoResult::CloseError;
    }
    return result;
}

SdRecordObjectProbe probe_object_default(
    void *, const char *path, const uint8_t *expected,
    size_t expected_size)
{
    return sd_record_object_probe(path, expected, expected_size);
}

const SdRecordObjectFileOps kDefaultOps = {
    nullptr,
    ensure_directory_default,
    write_object_default,
    probe_object_default,
};

SdRecordObjectWriteStatus map_write_result(
    SdRecordObjectWriteIoResult result)
{
    switch (result) {
    case SdRecordObjectWriteIoResult::Ok:
        return SdRecordObjectWriteStatus::Ok;
    case SdRecordObjectWriteIoResult::OpenError:
        return SdRecordObjectWriteStatus::OpenError;
    case SdRecordObjectWriteIoResult::ShortWrite:
        return SdRecordObjectWriteStatus::ShortWrite;
    case SdRecordObjectWriteIoResult::SyncError:
        return SdRecordObjectWriteStatus::SyncError;
    case SdRecordObjectWriteIoResult::CloseError:
        return SdRecordObjectWriteStatus::CloseError;
    }
    return SdRecordObjectWriteStatus::VerifyError;
}

}  // namespace

SdRecordObjectProbe sd_record_object_probe(
    const char *path, const uint8_t *expected, size_t expected_size)
{
    SdRecordObjectProbe result = {};
    if (!valid_path(path) || !valid_object(expected, expected_size)) {
        return result;
    }
    errno = 0;
    std::FILE *file = std::fopen(path, "rb");
    if (file == nullptr) {
        result.status = errno == ENOENT ? SdRecordObjectProbeStatus::Missing
                                        : SdRecordObjectProbeStatus::IoError;
        return result;
    }

    std::array<uint8_t, 256U> chunk = {};
    size_t compared = 0U;
    bool matches = true;
    uint32_t crc = kCrc32Initial;
    while (true) {
        const size_t count = std::fread(chunk.data(), 1U, chunk.size(), file);
        if (count != 0U) {
            crc = crc32_update(crc, chunk.data(), count);
            for (size_t index = 0U; index < count; ++index) {
                if (compared + index >= expected_size ||
                    chunk[index] != expected[compared + index]) {
                    matches = false;
                }
            }
            compared += count;
        }
        if (count != chunk.size()) {
            if (std::ferror(file) != 0) {
                result.status = SdRecordObjectProbeStatus::IoError;
            }
            break;
        }
    }
    const bool close_ok = std::fclose(file) == 0;
    result.file_bytes = compared;
    result.file_crc32 = crc ^ kCrc32FinalXor;
    if (!close_ok || result.status == SdRecordObjectProbeStatus::IoError) {
        result.status = SdRecordObjectProbeStatus::IoError;
        return result;
    }
    result.status = matches && compared == expected_size
                        ? SdRecordObjectProbeStatus::Match
                        : SdRecordObjectProbeStatus::Mismatch;
    return result;
}

SdRecordObjectProbe sd_record_object_probe_reference(
    const char *path, uint32_t expected_bytes, uint32_t expected_crc32)
{
    SdRecordObjectProbe result = {};
    if (!valid_path(path) || expected_bytes == 0U ||
        expected_bytes > kRepositoryPayloadBytes) {
        return result;
    }
    errno = 0;
    std::FILE *file = std::fopen(path, "rb");
    if (file == nullptr) {
        result.status = errno == ENOENT ? SdRecordObjectProbeStatus::Missing
                                        : SdRecordObjectProbeStatus::IoError;
        return result;
    }

    std::array<uint8_t, 256U> chunk = {};
    uint64_t bytes = 0U;
    uint32_t crc = kCrc32Initial;
    while (true) {
        const size_t count = std::fread(chunk.data(), 1U, chunk.size(), file);
        if (count != 0U) {
            crc = crc32_update(crc, chunk.data(), count);
            bytes += count;
        }
        if (count != chunk.size()) {
            if (std::ferror(file) != 0) {
                result.status = SdRecordObjectProbeStatus::IoError;
            }
            break;
        }
    }
    const bool close_ok = std::fclose(file) == 0;
    result.file_bytes = bytes;
    result.file_crc32 = crc ^ kCrc32FinalXor;
    if (!close_ok || result.status == SdRecordObjectProbeStatus::IoError) {
        result.status = SdRecordObjectProbeStatus::IoError;
        return result;
    }
    result.status = bytes == expected_bytes &&
                            result.file_crc32 == expected_crc32
                        ? SdRecordObjectProbeStatus::Match
                        : SdRecordObjectProbeStatus::Mismatch;
    return result;
}

SdRecordObjectReadResult sd_record_object_read_reference(
    const char *path, uint32_t expected_bytes, uint32_t expected_crc32,
    uint8_t *output, size_t output_capacity)
{
    SdRecordObjectReadResult result = {};
    if (!valid_path(path) || expected_bytes == 0U ||
        expected_bytes > kRepositoryPayloadBytes || output == nullptr ||
        output_capacity < expected_bytes) {
        return result;
    }
    errno = 0;
    std::FILE *file = std::fopen(path, "rb");
    if (file == nullptr) {
        result.status = errno == ENOENT ? SdRecordObjectReadStatus::Missing
                                        : SdRecordObjectReadStatus::IoError;
        return result;
    }

    const size_t read = std::fread(output, 1U, expected_bytes, file);
    result.bytes = read;
    if (read != expected_bytes) {
        result.status = std::ferror(file) != 0
                            ? SdRecordObjectReadStatus::IoError
                            : SdRecordObjectReadStatus::SizeMismatch;
    } else {
        uint8_t extra = 0U;
        const size_t extra_read = std::fread(&extra, 1U, 1U, file);
        if (extra_read != 0U) {
            result.status = SdRecordObjectReadStatus::SizeMismatch;
        } else if (std::ferror(file) != 0) {
            result.status = SdRecordObjectReadStatus::IoError;
        } else {
            result.crc32 = sticky_storage::crc32(output, read);
            result.status = result.crc32 == expected_crc32
                                ? SdRecordObjectReadStatus::Ok
                                : SdRecordObjectReadStatus::CrcMismatch;
        }
    }
    if (std::fclose(file) != 0) {
        result.status = SdRecordObjectReadStatus::IoError;
    }
    return result;
}

SdRecordObjectWriteResult sd_record_object_write_with_ops(
    const SdRecordObjectPaths &paths, const uint8_t *bytes, size_t size,
    const SdRecordObjectFileOps &ops)
{
    SdRecordObjectWriteResult result = {};
    if (!valid_paths(paths) || !valid_object(bytes, size) ||
        !valid_ops(ops)) {
        return result;
    }
    result.probe = ops.probe_object(
        ops.context, paths.object, bytes, size);
    if (result.probe.status == SdRecordObjectProbeStatus::Match) {
        result.status = SdRecordObjectWriteStatus::Ok;
        return result;
    }
    if (result.probe.status == SdRecordObjectProbeStatus::Mismatch) {
        result.status = SdRecordObjectWriteStatus::ExistingConflict;
        return result;
    }
    if (result.probe.status != SdRecordObjectProbeStatus::Missing) {
        result.status = result.probe.status ==
                                SdRecordObjectProbeStatus::InvalidArgument
                            ? SdRecordObjectWriteStatus::InvalidArgument
                            : SdRecordObjectWriteStatus::ProbeIoError;
        return result;
    }
    if (!ops.ensure_directory(ops.context, paths.application_root) ||
        !ops.ensure_directory(ops.context, paths.records_directory)) {
        result.status = SdRecordObjectWriteStatus::DirectoryError;
        return result;
    }
    result.status = map_write_result(ops.write_object(
        ops.context, paths.object, bytes, size));
    if (result.status != SdRecordObjectWriteStatus::Ok) {
        return result;
    }
    result.probe = ops.probe_object(
        ops.context, paths.object, bytes, size);
    if (result.probe.status != SdRecordObjectProbeStatus::Match) {
        result.status = result.probe.status ==
                                SdRecordObjectProbeStatus::IoError
                            ? SdRecordObjectWriteStatus::ProbeIoError
                            : SdRecordObjectWriteStatus::VerifyError;
        return result;
    }
    result.changed = true;
    return result;
}

SdRecordObjectWriteResult sd_record_object_write(
    const SdRecordObjectPaths &paths, const uint8_t *bytes, size_t size)
{
    return sd_record_object_write_with_ops(paths, bytes, size, kDefaultOps);
}

}  // namespace sticky_storage
