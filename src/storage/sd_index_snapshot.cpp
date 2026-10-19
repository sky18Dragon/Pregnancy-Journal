#include "sd_index_snapshot.h"

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

constexpr std::array<uint8_t, 4U> kMagic = {'P', 'J', 'I', 'X'};

void write_u16(uint8_t *bytes, uint16_t value)
{
    bytes[0] = static_cast<uint8_t>(value & 0xFFU);
    bytes[1] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
}

void write_u32(uint8_t *bytes, uint32_t value)
{
    for (size_t index = 0U; index < 4U; ++index) {
        bytes[index] = static_cast<uint8_t>((value >> (index * 8U)) & 0xFFU);
    }
}

void write_u64(uint8_t *bytes, uint64_t value)
{
    for (size_t index = 0U; index < 8U; ++index) {
        bytes[index] = static_cast<uint8_t>((value >> (index * 8U)) & 0xFFU);
    }
}

uint16_t read_u16(const uint8_t *bytes)
{
    return static_cast<uint16_t>(bytes[0]) |
           static_cast<uint16_t>(static_cast<uint16_t>(bytes[1]) << 8U);
}

uint32_t read_u32(const uint8_t *bytes)
{
    uint32_t value = 0U;
    for (size_t index = 0U; index < 4U; ++index) {
        value |= static_cast<uint32_t>(bytes[index]) << (index * 8U);
    }
    return value;
}

uint64_t read_u64(const uint8_t *bytes)
{
    uint64_t value = 0U;
    for (size_t index = 0U; index < 8U; ++index) {
        value |= static_cast<uint64_t>(bytes[index]) << (index * 8U);
    }
    return value;
}

bool zero_bytes(const uint8_t *bytes, size_t size)
{
    for (size_t index = 0U; index < size; ++index) {
        if (bytes[index] != 0U) {
            return false;
        }
    }
    return true;
}

bool name_valid(const char *name)
{
    return name != nullptr && std::strlen(name) <= kSdIndexObjectNameBytes &&
           fat83_name_valid(name);
}

bool key_less(const RepositoryKey &left, const RepositoryKey &right)
{
    if (left.repository != right.repository) {
        return static_cast<uint8_t>(left.repository) <
               static_cast<uint8_t>(right.repository);
    }
    return std::strcmp(left.record_id, right.record_id) < 0;
}

bool entry_valid(const SdIndexSnapshotEntry &entry)
{
    if (!repository_key_valid(entry.key) || entry.revision == 0U) {
        return false;
    }
    if (entry.tombstone) {
        return entry.object_bytes == 0U && entry.object_crc32 == 0U &&
               entry.object_file[0] == '\0';
    }
    return entry.object_bytes > 0U &&
           entry.object_bytes <= kRepositoryPayloadBytes &&
           name_valid(entry.object_file);
}

void encode_name(const char *name, uint8_t *output)
{
    std::memset(output, 0, kSdIndexObjectNameBytes);
    std::memcpy(output, name, std::strlen(name));
}

bool decode_name(const uint8_t *bytes, char *output)
{
    size_t length = 0U;
    while (length < kSdIndexObjectNameBytes && bytes[length] != 0U) {
        ++length;
    }
    if (length < kSdIndexObjectNameBytes &&
        !zero_bytes(bytes + length, kSdIndexObjectNameBytes - length)) {
        return false;
    }
    std::memcpy(output, bytes, length);
    output[length] = '\0';
    return true;
}

uint32_t snapshot_crc(const uint8_t *bytes, size_t size)
{
    uint32_t crc = crc32_update(kCrc32Initial, bytes, 68U);
    crc = crc32_update(crc, bytes + kSdIndexSnapshotHeaderBytes,
                       size - kSdIndexSnapshotHeaderBytes);
    return crc ^ kCrc32FinalXor;
}

bool valid_paths(const SdIndexFilePaths &paths)
{
    if (paths.application_root == nullptr ||
        paths.index_directory == nullptr || paths.index_file == nullptr ||
        paths.application_root[0] == '\0' ||
        paths.index_directory[0] == '\0' || paths.index_file[0] == '\0') {
        return false;
    }
    char expected[160U] = {};
    const int written = std::snprintf(expected, sizeof(expected), "%s/%s",
                                      paths.application_root,
                                      kIndexDirectory);
    if (written <= 0 || static_cast<size_t>(written) >= sizeof(expected) ||
        std::strcmp(expected, paths.index_directory) != 0) {
        return false;
    }
    const size_t prefix = std::strlen(paths.index_directory);
    const size_t file_length = std::strlen(paths.index_file);
    if (file_length <= prefix ||
        std::strncmp(paths.index_file, paths.index_directory, prefix) != 0 ||
        paths.index_file[prefix] != '/') {
        return false;
    }
    const char *name = paths.index_file + prefix + 1U;
    return fat83_name_valid(name) && std::strchr(name, '/') == nullptr;
}

bool valid_file_bytes(const uint8_t *bytes, size_t size)
{
    return bytes != nullptr && size >= kSdIndexSnapshotHeaderBytes &&
           size <= kSdIndexSnapshotMaxBytes;
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

SdIndexWriteIoResult write_file_default(void *, const char *path,
                                        const uint8_t *bytes, size_t size)
{
    std::FILE *file = std::fopen(path, "wb");
    if (file == nullptr) {
        return SdIndexWriteIoResult::OpenError;
    }
    SdIndexWriteIoResult result = SdIndexWriteIoResult::Ok;
    if (std::fwrite(bytes, 1U, size, file) != size) {
        result = SdIndexWriteIoResult::ShortWrite;
    } else if (std::fflush(file) != 0 || ::fsync(fileno(file)) != 0) {
        result = SdIndexWriteIoResult::SyncError;
    }
    if (std::fclose(file) != 0 && result == SdIndexWriteIoResult::Ok) {
        result = SdIndexWriteIoResult::CloseError;
    }
    return result;
}

SdIndexFileProbe probe_file_default(void *, const char *path,
                                    const uint8_t *expected,
                                    size_t expected_size)
{
    return sd_index_file_probe(path, expected, expected_size);
}

const SdIndexFileOps kDefaultOps = {
    nullptr, ensure_directory_default, write_file_default, probe_file_default};

SdIndexWriteStatus map_write_status(SdIndexWriteIoResult status)
{
    switch (status) {
    case SdIndexWriteIoResult::Ok:
        return SdIndexWriteStatus::Ok;
    case SdIndexWriteIoResult::OpenError:
        return SdIndexWriteStatus::OpenError;
    case SdIndexWriteIoResult::ShortWrite:
        return SdIndexWriteStatus::ShortWrite;
    case SdIndexWriteIoResult::SyncError:
        return SdIndexWriteStatus::SyncError;
    case SdIndexWriteIoResult::CloseError:
        return SdIndexWriteStatus::CloseError;
    }
    return SdIndexWriteStatus::VerifyError;
}

bool snapshot_entry_provider(void *context, size_t index,
                             SdIndexSnapshotEntry &entry)
{
    const auto *snapshot = static_cast<const SdIndexSnapshot *>(context);
    if (snapshot == nullptr || index >= snapshot->entry_count) {
        return false;
    }
    entry = snapshot->entries[index];
    return true;
}

SdIndexSnapshotStatus decode_snapshot(const uint8_t *bytes, size_t size,
                                      SdIndexSnapshot *snapshot)
{
    if (snapshot != nullptr) {
        snapshot->reset();
    }
    if (bytes == nullptr) {
        return SdIndexSnapshotStatus::InvalidArgument;
    }
    if (size < kSdIndexSnapshotHeaderBytes) {
        return SdIndexSnapshotStatus::Incomplete;
    }
    if (std::memcmp(bytes, kMagic.data(), kMagic.size()) != 0 ||
        read_u16(bytes + 6U) != kSdIndexSnapshotHeaderBytes ||
        !zero_bytes(bytes + 20U, 4U) || !zero_bytes(bytes + 72U, 8U)) {
        return SdIndexSnapshotStatus::Corrupt;
    }
    if (read_u16(bytes + 4U) != kSdIndexSnapshotSchemaVersion) {
        return SdIndexSnapshotStatus::UnsupportedSchema;
    }
    const uint32_t total = read_u32(bytes + 8U);
    const uint32_t count = read_u32(bytes + 12U);
    const uint32_t live_count = read_u32(bytes + 16U);
    if (count > kSdIndexSnapshotEntryCapacity ||
        total != kSdIndexSnapshotHeaderBytes +
                     count * kSdIndexSnapshotEntryBytes) {
        return SdIndexSnapshotStatus::Corrupt;
    }
    if (size < total) {
        return SdIndexSnapshotStatus::Incomplete;
    }
    if (size != total ||
        read_u32(bytes + 64U) !=
            crc32(bytes + kSdIndexSnapshotHeaderBytes,
                  size - kSdIndexSnapshotHeaderBytes) ||
        read_u32(bytes + 68U) != snapshot_crc(bytes, size)) {
        return SdIndexSnapshotStatus::Corrupt;
    }

    char dataset_id[kDatasetIdCharacters + 1U] = {};
    std::memcpy(dataset_id, bytes + 32U, kDatasetIdCharacters);
    const uint64_t last_transaction_id = read_u64(bytes + 24U);
    if (!sd_dataset_id_valid(dataset_id) || last_transaction_id == 0U) {
        return SdIndexSnapshotStatus::Corrupt;
    }
    if (snapshot != nullptr) {
        std::memcpy(snapshot->dataset_id, dataset_id, sizeof(dataset_id));
        snapshot->last_transaction_id = last_transaction_id;
        snapshot->entry_count = count;
    }

    size_t decoded_live = 0U;
    RepositoryKey previous_key = {};
    for (size_t index = 0U; index < count; ++index) {
        const uint8_t *encoded = bytes + kSdIndexSnapshotHeaderBytes +
                                 index * kSdIndexSnapshotEntryBytes;
        if (!zero_bytes(encoded + 2U, 2U) ||
            !zero_bytes(encoded + 60U, 4U) || encoded[1] > 1U) {
            return SdIndexSnapshotStatus::Corrupt;
        }
        SdIndexSnapshotEntry entry = {};
        entry.key.repository = static_cast<RepositoryKind>(encoded[0]);
        entry.tombstone = encoded[1] != 0U;
        entry.revision = read_u32(encoded + 4U);
        entry.object_bytes = read_u32(encoded + 8U);
        entry.object_crc32 = read_u32(encoded + 12U);
        std::memcpy(entry.key.record_id, encoded + 16U,
                    kRepositoryRecordIdCharacters);
        entry.key.record_id[kRepositoryRecordIdCharacters] = '\0';
        if (!decode_name(encoded + 48U, entry.object_file) ||
            !entry_valid(entry) ||
            (index != 0U && !key_less(previous_key, entry.key))) {
            return SdIndexSnapshotStatus::Corrupt;
        }
        previous_key = entry.key;
        if (snapshot != nullptr) {
            snapshot->entries[index] = entry;
        }
        decoded_live += entry.tombstone ? 0U : 1U;
    }
    return decoded_live == live_count ? SdIndexSnapshotStatus::Ok
                                      : SdIndexSnapshotStatus::Corrupt;
}

}  // namespace

size_t sd_index_snapshot_encode_entries(
    const char *dataset_id, uint64_t last_transaction_id,
    size_t entry_count, SdIndexSnapshotEntryProvider provider,
    void *provider_context, uint8_t *output, size_t output_capacity)
{
    if (output == nullptr || provider == nullptr ||
        !sd_dataset_id_valid(dataset_id) || last_transaction_id == 0U ||
        entry_count > kSdIndexSnapshotEntryCapacity) {
        return 0U;
    }
    const size_t total = kSdIndexSnapshotHeaderBytes +
                         entry_count * kSdIndexSnapshotEntryBytes;
    if (output_capacity < total || total > UINT32_MAX) {
        return 0U;
    }

    std::memset(output, 0, total);
    size_t live_count = 0U;
    RepositoryKey previous_key = {};
    for (size_t index = 0U; index < entry_count; ++index) {
        SdIndexSnapshotEntry entry = {};
        if (!provider(provider_context, index, entry) ||
            !entry_valid(entry) ||
            (index != 0U && !key_less(previous_key, entry.key))) {
            return 0U;
        }
        previous_key = entry.key;
        live_count += entry.tombstone ? 0U : 1U;

        uint8_t *encoded = output + kSdIndexSnapshotHeaderBytes +
                           index * kSdIndexSnapshotEntryBytes;
        encoded[0] = static_cast<uint8_t>(entry.key.repository);
        encoded[1] = entry.tombstone ? 1U : 0U;
        write_u32(encoded + 4U, entry.revision);
        write_u32(encoded + 8U, entry.object_bytes);
        write_u32(encoded + 12U, entry.object_crc32);
        std::memcpy(encoded + 16U, entry.key.record_id,
                    kRepositoryRecordIdCharacters);
        if (!entry.tombstone) {
            encode_name(entry.object_file, encoded + 48U);
        }
    }

    std::memcpy(output, kMagic.data(), kMagic.size());
    write_u16(output + 4U, kSdIndexSnapshotSchemaVersion);
    write_u16(output + 6U,
              static_cast<uint16_t>(kSdIndexSnapshotHeaderBytes));
    write_u32(output + 8U, static_cast<uint32_t>(total));
    write_u32(output + 12U, static_cast<uint32_t>(entry_count));
    write_u32(output + 16U, static_cast<uint32_t>(live_count));
    write_u64(output + 24U, last_transaction_id);
    std::memcpy(output + 32U, dataset_id, kDatasetIdCharacters);
    write_u32(output + 64U,
              crc32(output + kSdIndexSnapshotHeaderBytes,
                    total - kSdIndexSnapshotHeaderBytes));
    write_u32(output + 68U, snapshot_crc(output, total));
    return total;
}

size_t sd_index_snapshot_encode(const SdIndexSnapshot &snapshot,
                                uint8_t *output, size_t output_capacity)
{
    if (snapshot.entry_count > snapshot.entries.size()) {
        return 0U;
    }
    return sd_index_snapshot_encode_entries(
        snapshot.dataset_id, snapshot.last_transaction_id,
        snapshot.entry_count, snapshot_entry_provider,
        const_cast<SdIndexSnapshot *>(&snapshot), output, output_capacity);
}

SdIndexSnapshotStatus sd_index_snapshot_validate(const uint8_t *bytes,
                                                 size_t size)
{
    return decode_snapshot(bytes, size, nullptr);
}

void SdIndexSnapshot::reset()
{
    std::memset(dataset_id, 0, sizeof(dataset_id));
    last_transaction_id = 0U;
    entries.fill(SdIndexSnapshotEntry{});
    entry_count = 0U;
}

SdIndexSnapshotStatus sd_index_snapshot_decode(const uint8_t *bytes,
                                               size_t size,
                                               SdIndexSnapshot &snapshot)
{
    return decode_snapshot(bytes, size, &snapshot);
}

SdIndexFileProbe sd_index_file_probe(const char *path,
                                     const uint8_t *expected,
                                     size_t expected_size)
{
    SdIndexFileProbe result = {};
    if (path == nullptr || path[0] == '\0' ||
        !valid_file_bytes(expected, expected_size)) {
        return result;
    }
    errno = 0;
    std::FILE *file = std::fopen(path, "rb");
    if (file == nullptr) {
        result.status = errno == ENOENT ? SdIndexFileProbeStatus::Missing
                                        : SdIndexFileProbeStatus::IoError;
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
                result.status = SdIndexFileProbeStatus::IoError;
            }
            break;
        }
    }
    const bool close_ok = std::fclose(file) == 0;
    result.file_bytes = compared;
    result.file_crc32 = crc ^ kCrc32FinalXor;
    if (!close_ok || result.status == SdIndexFileProbeStatus::IoError) {
        result.status = SdIndexFileProbeStatus::IoError;
        return result;
    }
    result.status = matches && compared == expected_size
                        ? SdIndexFileProbeStatus::Match
                        : SdIndexFileProbeStatus::Mismatch;
    return result;
}

SdIndexWriteResult sd_index_file_write_with_ops(
    const SdIndexFilePaths &paths, const uint8_t *bytes, size_t size,
    const SdIndexFileOps &ops)
{
    SdIndexWriteResult result = {};
    if (!valid_paths(paths) || !valid_file_bytes(bytes, size) ||
        ops.ensure_directory == nullptr || ops.write_file == nullptr ||
        ops.probe_file == nullptr) {
        return result;
    }
    if (sd_index_snapshot_validate(bytes, size) !=
        SdIndexSnapshotStatus::Ok) {
        return result;
    }
    result.probe = ops.probe_file(ops.context, paths.index_file, bytes, size);
    if (result.probe.status == SdIndexFileProbeStatus::Match) {
        result.status = SdIndexWriteStatus::Ok;
        return result;
    }
    if (result.probe.status == SdIndexFileProbeStatus::Mismatch) {
        result.status = SdIndexWriteStatus::ExistingConflict;
        return result;
    }
    if (result.probe.status != SdIndexFileProbeStatus::Missing) {
        result.status = SdIndexWriteStatus::ProbeIoError;
        return result;
    }
    if (!ops.ensure_directory(ops.context, paths.application_root) ||
        !ops.ensure_directory(ops.context, paths.index_directory)) {
        result.status = SdIndexWriteStatus::DirectoryError;
        return result;
    }
    result.status = map_write_status(
        ops.write_file(ops.context, paths.index_file, bytes, size));
    if (result.status != SdIndexWriteStatus::Ok) {
        return result;
    }
    result.probe = ops.probe_file(ops.context, paths.index_file, bytes, size);
    if (result.probe.status != SdIndexFileProbeStatus::Match) {
        result.status = result.probe.status == SdIndexFileProbeStatus::IoError
                            ? SdIndexWriteStatus::ProbeIoError
                            : SdIndexWriteStatus::VerifyError;
        return result;
    }
    result.changed = true;
    return result;
}

SdIndexWriteResult sd_index_file_write(const SdIndexFilePaths &paths,
                                       const uint8_t *bytes, size_t size)
{
    return sd_index_file_write_with_ops(paths, bytes, size, kDefaultOps);
}

}  // namespace sticky_storage
