#include "sd_journal_file.h"

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>

#include <sys/stat.h>
#include <unistd.h>

#include "sd_manifest.h"

namespace sticky_storage {
namespace {

bool valid_path(const char *path)
{
    return path != nullptr && path[0] != '\0';
}

bool valid_paths(const SdJournalFilePaths &paths)
{
    return valid_path(paths.application_root) &&
           valid_path(paths.journal_directory) && valid_path(paths.segment);
}

bool valid_ops(const SdJournalFileOps &ops)
{
    return ops.ensure_directory != nullptr && ops.append_record != nullptr &&
           ops.probe_segment != nullptr;
}

SdJournalFileProbeStatus map_decode_status(SdJournalDecodeStatus status)
{
    switch (status) {
    case SdJournalDecodeStatus::Ok:
        return SdJournalFileProbeStatus::Ready;
    case SdJournalDecodeStatus::Incomplete:
        return SdJournalFileProbeStatus::TailTruncated;
    case SdJournalDecodeStatus::Corrupt:
        return SdJournalFileProbeStatus::Corrupt;
    case SdJournalDecodeStatus::UnsupportedSchema:
        return SdJournalFileProbeStatus::UnsupportedSchema;
    case SdJournalDecodeStatus::UnsupportedRecordType:
        return SdJournalFileProbeStatus::UnsupportedRecordType;
    case SdJournalDecodeStatus::DatasetMismatch:
        return SdJournalFileProbeStatus::DatasetMismatch;
    case SdJournalDecodeStatus::InvalidArgument:
        return SdJournalFileProbeStatus::InvalidArgument;
    }
    return SdJournalFileProbeStatus::Corrupt;
}

SdJournalAppendStatus map_probe_failure(SdJournalFileProbeStatus status)
{
    switch (status) {
    case SdJournalFileProbeStatus::TailTruncated:
        return SdJournalAppendStatus::TailTruncated;
    case SdJournalFileProbeStatus::Corrupt:
        return SdJournalAppendStatus::Corrupt;
    case SdJournalFileProbeStatus::UnsupportedSchema:
        return SdJournalAppendStatus::UnsupportedSchema;
    case SdJournalFileProbeStatus::UnsupportedRecordType:
        return SdJournalAppendStatus::UnsupportedRecordType;
    case SdJournalFileProbeStatus::DatasetMismatch:
        return SdJournalAppendStatus::DatasetMismatch;
    case SdJournalFileProbeStatus::NonMonotonicTransaction:
        return SdJournalAppendStatus::NonMonotonicTransaction;
    case SdJournalFileProbeStatus::ConsumerRejected:
        return SdJournalAppendStatus::Corrupt;
    case SdJournalFileProbeStatus::IoError:
        return SdJournalAppendStatus::ProbeIoError;
    case SdJournalFileProbeStatus::InvalidArgument:
        return SdJournalAppendStatus::InvalidArgument;
    case SdJournalFileProbeStatus::Missing:
    case SdJournalFileProbeStatus::Empty:
    case SdJournalFileProbeStatus::Ready:
        break;
    }
    return SdJournalAppendStatus::Corrupt;
}

SdJournalAppendStatus map_append_result(SdJournalAppendIoResult result)
{
    switch (result) {
    case SdJournalAppendIoResult::Ok:
        return SdJournalAppendStatus::Ok;
    case SdJournalAppendIoResult::OpenError:
        return SdJournalAppendStatus::OpenError;
    case SdJournalAppendIoResult::ShortWrite:
        return SdJournalAppendStatus::ShortWrite;
    case SdJournalAppendIoResult::SyncError:
        return SdJournalAppendStatus::SyncError;
    case SdJournalAppendIoResult::CloseError:
        return SdJournalAppendStatus::CloseError;
    }
    return SdJournalAppendStatus::VerifyError;
}

SdJournalRecordSummary summarize(const SdJournalRecordView &record)
{
    SdJournalRecordSummary summary = {};
    summary.type = record.type;
    summary.transaction_id = record.transaction_id;
    summary.encoded_size = record.encoded_size;
    summary.payload_size = record.payload_size;
    summary.payload_crc32 = record.payload_crc32;
    summary.frame_crc32 = record.frame_crc32;
    return summary;
}

bool same_record(const SdJournalRecordSummary &left,
                 const SdJournalRecordSummary &right)
{
    return left.type == right.type &&
           left.transaction_id == right.transaction_id &&
           left.encoded_size == right.encoded_size &&
           left.payload_size == right.payload_size &&
           left.payload_crc32 == right.payload_crc32 &&
           left.frame_crc32 == right.frame_crc32;
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

SdJournalAppendIoResult append_record_default(void *, const char *path,
                                              const uint8_t *bytes,
                                              size_t size)
{
    std::FILE *file = std::fopen(path, "ab");
    if (file == nullptr) {
        return SdJournalAppendIoResult::OpenError;
    }

    SdJournalAppendIoResult result = SdJournalAppendIoResult::Ok;
    if (std::fwrite(bytes, 1U, size, file) != size) {
        result = SdJournalAppendIoResult::ShortWrite;
    } else if (std::fflush(file) != 0 || ::fsync(fileno(file)) != 0) {
        result = SdJournalAppendIoResult::SyncError;
    }
    if (std::fclose(file) != 0 && result == SdJournalAppendIoResult::Ok) {
        result = SdJournalAppendIoResult::CloseError;
    }
    return result;
}

SdJournalFileProbe probe_segment_default(void *, const char *path,
                                         const char *expected_dataset_id)
{
    return sd_journal_probe_file(path, expected_dataset_id);
}

const SdJournalFileOps kDefaultOps = {
    nullptr,
    ensure_directory_default,
    append_record_default,
    probe_segment_default,
};

}  // namespace

SdJournalFileProbe sd_journal_replay_file(
    const char *path, const char *expected_dataset_id,
    SdJournalRecordVisitor visitor, void *visitor_context)
{
    SdJournalFileProbe result = {};
    if (!valid_path(path) || !sd_dataset_id_valid(expected_dataset_id)) {
        return result;
    }

    errno = 0;
    std::FILE *file = std::fopen(path, "rb");
    if (file == nullptr) {
        result.status = errno == ENOENT ? SdJournalFileProbeStatus::Missing
                                        : SdJournalFileProbeStatus::IoError;
        return result;
    }

    struct stat info = {};
    if (::fstat(fileno(file), &info) == 0 && info.st_size >= 0) {
        result.file_bytes = static_cast<uint64_t>(info.st_size);
    }

    std::array<uint8_t, kSdJournalMaxRecordBytes> frame = {};
    uint64_t offset = 0U;
    while (true) {
        const size_t header_read =
            std::fread(frame.data(), 1U, kSdJournalHeaderBytes, file);
        if (header_read == 0U) {
            if (std::ferror(file) != 0) {
                result.status = SdJournalFileProbeStatus::IoError;
            } else {
                result.status = result.record_count == 0U
                                    ? SdJournalFileProbeStatus::Empty
                                    : SdJournalFileProbeStatus::Ready;
                result.file_bytes = offset;
                result.valid_bytes = offset;
            }
            break;
        }

        SdJournalRecordView record = {};
        SdJournalDecodeStatus decoded = sd_journal_decode_record(
            expected_dataset_id, frame.data(), header_read, record);
        if (header_read != kSdJournalHeaderBytes) {
            result.status = std::ferror(file) != 0
                                ? SdJournalFileProbeStatus::IoError
                                : map_decode_status(decoded);
            result.valid_bytes = offset;
            break;
        }

        if (decoded == SdJournalDecodeStatus::Incomplete &&
            record.encoded_size >= kSdJournalHeaderBytes &&
            record.encoded_size <= frame.size()) {
            const size_t remaining =
                record.encoded_size - kSdJournalHeaderBytes;
            const size_t payload_read = std::fread(
                frame.data() + kSdJournalHeaderBytes, 1U, remaining, file);
            if (payload_read != remaining) {
                result.status = std::ferror(file) != 0
                                    ? SdJournalFileProbeStatus::IoError
                                    : SdJournalFileProbeStatus::TailTruncated;
                result.valid_bytes = offset;
                break;
            }
            decoded = sd_journal_decode_record(
                expected_dataset_id, frame.data(), record.encoded_size,
                record);
        }
        if (decoded != SdJournalDecodeStatus::Ok) {
            result.status = map_decode_status(decoded);
            result.valid_bytes = offset;
            break;
        }
        if (record.transaction_id <= result.last.transaction_id) {
            result.status =
                SdJournalFileProbeStatus::NonMonotonicTransaction;
            result.valid_bytes = offset;
            break;
        }
        if (result.record_count == UINT32_MAX) {
            result.status = SdJournalFileProbeStatus::Corrupt;
            result.valid_bytes = offset;
            break;
        }
        if (visitor != nullptr && !visitor(visitor_context, record)) {
            result.status = SdJournalFileProbeStatus::ConsumerRejected;
            result.valid_bytes = offset;
            break;
        }

        offset += record.encoded_size;
        result.valid_bytes = offset;
        if (result.record_count == 0U) {
            result.first_transaction_id = record.transaction_id;
        }
        result.last = summarize(record);
        ++result.record_count;
    }

    if (std::fclose(file) != 0) {
        result.status = SdJournalFileProbeStatus::IoError;
    }
    return result;
}

SdJournalFileProbe sd_journal_probe_file(const char *path,
                                         const char *expected_dataset_id)
{
    return sd_journal_replay_file(path, expected_dataset_id, nullptr,
                                  nullptr);
}

SdJournalAppendResult sd_journal_append_file_with_ops(
    const SdJournalFilePaths &paths, const char *dataset_id,
    SdJournalRecordType type, uint64_t transaction_id,
    const uint8_t *payload, size_t payload_size,
    const SdJournalFileOps &ops)
{
    SdJournalAppendResult result = {};
    if (!valid_paths(paths) || !valid_ops(ops) ||
        !sd_dataset_id_valid(dataset_id)) {
        return result;
    }

    std::array<uint8_t, kSdJournalMaxRecordBytes> encoded = {};
    const size_t encoded_size = sd_journal_encode_record(
        type, dataset_id, transaction_id, payload, payload_size,
        encoded.data(), encoded.size());
    if (encoded_size == 0U) {
        return result;
    }
    SdJournalRecordView requested_view = {};
    if (sd_journal_decode_record(dataset_id, encoded.data(), encoded_size,
                                 requested_view) !=
        SdJournalDecodeStatus::Ok) {
        return result;
    }
    const SdJournalRecordSummary requested = summarize(requested_view);

    const SdJournalFileProbe before =
        ops.probe_segment(ops.context, paths.segment, dataset_id);
    result.probe = before;
    if (before.status == SdJournalFileProbeStatus::Ready) {
        if (transaction_id == before.last.transaction_id) {
            result.status = same_record(before.last, requested)
                                ? SdJournalAppendStatus::Ok
                                : SdJournalAppendStatus::TransactionConflict;
            return result;
        }
        if (transaction_id < before.last.transaction_id) {
            result.status =
                SdJournalAppendStatus::NonMonotonicTransaction;
            return result;
        }
    } else if (before.status != SdJournalFileProbeStatus::Missing &&
               before.status != SdJournalFileProbeStatus::Empty) {
        result.status = map_probe_failure(before.status);
        return result;
    }

    if (!ops.ensure_directory(ops.context, paths.application_root) ||
        !ops.ensure_directory(ops.context, paths.journal_directory)) {
        result.status = SdJournalAppendStatus::DirectoryError;
        return result;
    }

    result.status = map_append_result(ops.append_record(
        ops.context, paths.segment, encoded.data(), encoded_size));
    if (result.status != SdJournalAppendStatus::Ok) {
        return result;
    }

    const SdJournalFileProbe after =
        ops.probe_segment(ops.context, paths.segment, dataset_id);
    result.probe = after;
    const uint64_t expected_bytes = before.file_bytes + encoded_size;
    const uint32_t expected_count = before.record_count + 1U;
    if (after.status != SdJournalFileProbeStatus::Ready ||
        after.file_bytes != expected_bytes ||
        after.valid_bytes != expected_bytes ||
        after.record_count != expected_count ||
        !same_record(after.last, requested)) {
        result.status = after.status == SdJournalFileProbeStatus::IoError
                            ? SdJournalAppendStatus::ProbeIoError
                            : SdJournalAppendStatus::VerifyError;
        return result;
    }

    result.status = SdJournalAppendStatus::Ok;
    result.changed = true;
    return result;
}

SdJournalAppendResult sd_journal_append_file(
    const SdJournalFilePaths &paths, const char *dataset_id,
    SdJournalRecordType type, uint64_t transaction_id,
    const uint8_t *payload, size_t payload_size)
{
    return sd_journal_append_file_with_ops(
        paths, dataset_id, type, transaction_id, payload, payload_size,
        kDefaultOps);
}

}  // namespace sticky_storage
