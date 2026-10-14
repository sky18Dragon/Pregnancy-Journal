#include "sd_journal_store.h"

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>

#include <dirent.h>

#include "sd_manifest.h"
namespace sticky_storage {
namespace {

bool valid_paths(const SdJournalStorePaths &paths)
{
    if (paths.application_root == nullptr ||
        paths.application_root[0] == '\0' ||
        paths.journal_directory == nullptr ||
        paths.journal_directory[0] == '\0' ||
        std::strlen(paths.application_root) >= kMaxPathBytes ||
        std::strlen(paths.journal_directory) >= kMaxPathBytes) {
        return false;
    }
    char expected[kMaxPathBytes] = {};
    const int written = std::snprintf(expected, sizeof(expected), "%s/JOURNAL",
                                      paths.application_root);
    return written > 0 && static_cast<size_t>(written) < sizeof(expected) &&
           std::strcmp(expected, paths.journal_directory) == 0;
}

bool make_segment_path(const SdJournalStorePaths &paths, uint32_t number,
                       char *output, size_t output_capacity)
{
    char name[13U] = {};
    if (!sd_journal_segment_name(number, name, sizeof(name))) {
        return false;
    }
    const int written = std::snprintf(output, output_capacity, "%s/%s",
                                      paths.journal_directory, name);
    return written > 0 && static_cast<size_t>(written) < output_capacity &&
           static_cast<size_t>(written) < kMaxPathBytes;
}

bool parse_segment_name(const char *name, uint32_t &number)
{
    number = 0U;
    if (name == nullptr || std::strlen(name) != 12U ||
        std::memcmp(name, "TXN", 3U) != 0 ||
        std::memcmp(name + 8U, ".LOG", 4U) != 0) {
        return false;
    }
    for (size_t index = 3U; index < 8U; ++index) {
        if (name[index] < '0' || name[index] > '9') {
            return false;
        }
        number = number * 10U + static_cast<uint32_t>(name[index] - '0');
    }
    return number >= kSdJournalFirstSegmentNumber &&
           number <= kSdJournalLastSegmentNumber;
}

SdJournalStoreStatus map_file_status(SdJournalFileProbeStatus status)
{
    switch (status) {
    case SdJournalFileProbeStatus::Ready:
        return SdJournalStoreStatus::Ready;
    case SdJournalFileProbeStatus::TailTruncated:
        return SdJournalStoreStatus::TailTruncated;
    case SdJournalFileProbeStatus::Corrupt:
        return SdJournalStoreStatus::Corrupt;
    case SdJournalFileProbeStatus::UnsupportedSchema:
        return SdJournalStoreStatus::UnsupportedSchema;
    case SdJournalFileProbeStatus::UnsupportedRecordType:
        return SdJournalStoreStatus::UnsupportedRecordType;
    case SdJournalFileProbeStatus::DatasetMismatch:
        return SdJournalStoreStatus::DatasetMismatch;
    case SdJournalFileProbeStatus::NonMonotonicTransaction:
        return SdJournalStoreStatus::NonMonotonicTransaction;
    case SdJournalFileProbeStatus::ConsumerRejected:
        return SdJournalStoreStatus::ConsumerRejected;
    case SdJournalFileProbeStatus::IoError:
        return SdJournalStoreStatus::IoError;
    case SdJournalFileProbeStatus::InvalidArgument:
        return SdJournalStoreStatus::InvalidArgument;
    case SdJournalFileProbeStatus::Missing:
        return SdJournalStoreStatus::SegmentGap;
    case SdJournalFileProbeStatus::Empty:
        return SdJournalStoreStatus::Corrupt;
    }
    return SdJournalStoreStatus::Corrupt;
}

SdJournalStoreStatus map_append_status(SdJournalAppendStatus status)
{
    switch (status) {
    case SdJournalAppendStatus::Ok:
        return SdJournalStoreStatus::Ready;
    case SdJournalAppendStatus::InvalidArgument:
        return SdJournalStoreStatus::InvalidArgument;
    case SdJournalAppendStatus::DirectoryError:
        return SdJournalStoreStatus::DirectoryError;
    case SdJournalAppendStatus::ProbeIoError:
        return SdJournalStoreStatus::IoError;
    case SdJournalAppendStatus::TailTruncated:
        return SdJournalStoreStatus::TailTruncated;
    case SdJournalAppendStatus::Corrupt:
        return SdJournalStoreStatus::Corrupt;
    case SdJournalAppendStatus::UnsupportedSchema:
        return SdJournalStoreStatus::UnsupportedSchema;
    case SdJournalAppendStatus::UnsupportedRecordType:
        return SdJournalStoreStatus::UnsupportedRecordType;
    case SdJournalAppendStatus::DatasetMismatch:
        return SdJournalStoreStatus::DatasetMismatch;
    case SdJournalAppendStatus::NonMonotonicTransaction:
        return SdJournalStoreStatus::NonMonotonicTransaction;
    case SdJournalAppendStatus::TransactionConflict:
        return SdJournalStoreStatus::TransactionConflict;
    case SdJournalAppendStatus::OpenError:
        return SdJournalStoreStatus::OpenError;
    case SdJournalAppendStatus::ShortWrite:
        return SdJournalStoreStatus::ShortWrite;
    case SdJournalAppendStatus::SyncError:
        return SdJournalStoreStatus::SyncError;
    case SdJournalAppendStatus::CloseError:
        return SdJournalStoreStatus::CloseError;
    case SdJournalAppendStatus::VerifyError:
        return SdJournalStoreStatus::VerifyError;
    }
    return SdJournalStoreStatus::IoError;
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

bool requested_summary(SdJournalRecordType type, const char *dataset_id,
                       uint64_t transaction_id, const uint8_t *payload,
                       size_t payload_size, SdJournalRecordSummary &summary,
                       size_t &encoded_size)
{
    std::array<uint8_t, kSdJournalMaxRecordBytes> encoded = {};
    encoded_size = sd_journal_encode_record(
        type, dataset_id, transaction_id, payload, payload_size,
        encoded.data(), encoded.size());
    if (encoded_size == 0U) {
        return false;
    }
    SdJournalRecordView view = {};
    if (sd_journal_decode_record(dataset_id, encoded.data(), encoded_size,
                                 view) != SdJournalDecodeStatus::Ok) {
        return false;
    }
    summary.type = view.type;
    summary.transaction_id = view.transaction_id;
    summary.encoded_size = view.encoded_size;
    summary.payload_size = view.payload_size;
    summary.payload_crc32 = view.payload_crc32;
    summary.frame_crc32 = view.frame_crc32;
    return true;
}

struct ReplayContext {
    SdJournalStoreRecordVisitor visitor = nullptr;
    void *visitor_context = nullptr;
    uint32_t segment_number = 0U;
    uint64_t previous_transaction_id = 0U;
    bool non_monotonic = false;
};

bool replay_record(void *context, const SdJournalRecordView &record)
{
    auto &replay = *static_cast<ReplayContext *>(context);
    if (record.transaction_id <= replay.previous_transaction_id) {
        replay.non_monotonic = true;
        return false;
    }
    replay.previous_transaction_id = record.transaction_id;
    return replay.visitor == nullptr ||
           replay.visitor(replay.visitor_context, replay.segment_number,
                          record);
}

}  // namespace

SdJournalStoreProbe sd_journal_store_replay(
    const SdJournalStorePaths &paths, const char *expected_dataset_id,
    SdJournalStoreRecordVisitor visitor, void *visitor_context)
{
    SdJournalStoreProbe result = {};
    if (!valid_paths(paths) || !sd_dataset_id_valid(expected_dataset_id)) {
        return result;
    }

    errno = 0;
    DIR *directory = ::opendir(paths.journal_directory);
    if (directory == nullptr) {
        result.status = errno == ENOENT ? SdJournalStoreStatus::Missing
                                        : SdJournalStoreStatus::IoError;
        return result;
    }

    uint32_t highest = 0U;
    while (dirent *entry = ::readdir(directory)) {
        if (std::strcmp(entry->d_name, ".") == 0 ||
            std::strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        uint32_t number = 0U;
        if (!parse_segment_name(entry->d_name, number)) {
            result.status = SdJournalStoreStatus::UnexpectedEntry;
            ::closedir(directory);
            return result;
        }
        ++result.segment_count;
        if (number > highest) {
            highest = number;
        }
    }
    if (::closedir(directory) != 0) {
        result.status = SdJournalStoreStatus::IoError;
        return result;
    }
    if (result.segment_count == 0U) {
        result.status = SdJournalStoreStatus::Empty;
        return result;
    }
    if (result.segment_count != highest) {
        result.status = SdJournalStoreStatus::SegmentGap;
        return result;
    }

    ReplayContext replay = {visitor, visitor_context, 0U, 0U, false};
    for (uint32_t number = 1U; number <= highest; ++number) {
        char path[kMaxPathBytes] = {};
        if (!make_segment_path(paths, number, path, sizeof(path))) {
            result.status = SdJournalStoreStatus::InvalidArgument;
            return result;
        }
        replay.segment_number = number;
        const SdJournalFileProbe segment = sd_journal_replay_file(
            path, expected_dataset_id, replay_record, &replay);
        if (segment.status != SdJournalFileProbeStatus::Ready) {
            result.status = replay.non_monotonic
                                ? SdJournalStoreStatus::NonMonotonicTransaction
                                : map_file_status(segment.status);
            // A truncated final frame does not invalidate the complete frames
            // before it. Publish that verified prefix in the aggregate fields
            // so callers do not mistake a recoverable tail for an empty log.
            // Other failures remain fail-closed and expose only the active
            // segment diagnostics.
            if (segment.status == SdJournalFileProbeStatus::TailTruncated &&
                number == highest &&
                segment.record_count <=
                    UINT32_MAX - result.total_record_count) {
                result.total_bytes += segment.file_bytes;
                result.total_record_count += segment.record_count;
            }
            result.active_segment = number;
            result.active = segment;
            return result;
        }
        if (segment.file_bytes > kSdJournalSegmentMaxBytes) {
            result.status = SdJournalStoreStatus::SegmentTooLarge;
            result.active_segment = number;
            result.active = segment;
            return result;
        }
        if (segment.record_count >
            UINT32_MAX - result.total_record_count) {
            result.status = SdJournalStoreStatus::Corrupt;
            return result;
        }
        result.total_bytes += segment.file_bytes;
        result.total_record_count += segment.record_count;
        result.active_segment = number;
        result.active = segment;
    }
    result.status = SdJournalStoreStatus::Ready;
    return result;
}

SdJournalStoreProbe sd_journal_store_probe(
    const SdJournalStorePaths &paths, const char *expected_dataset_id)
{
    return sd_journal_store_replay(paths, expected_dataset_id, nullptr,
                                   nullptr);
}

SdJournalStoreAppendResult sd_journal_store_append(
    const SdJournalStorePaths &paths, const char *dataset_id,
    SdJournalRecordType type, uint64_t transaction_id,
    const uint8_t *payload, size_t payload_size)
{
    SdJournalStoreAppendResult result = {};
    if (!valid_paths(paths) || !sd_dataset_id_valid(dataset_id)) {
        return result;
    }
    SdJournalRecordSummary requested = {};
    size_t encoded_size = 0U;
    if (!requested_summary(type, dataset_id, transaction_id, payload,
                           payload_size, requested, encoded_size)) {
        return result;
    }

    const SdJournalStoreProbe before =
        sd_journal_store_probe(paths, dataset_id);
    result.probe = before;
    uint32_t current_segment = kSdJournalFirstSegmentNumber;
    uint64_t current_bytes = 0U;
    if (before.status == SdJournalStoreStatus::Ready) {
        if (transaction_id == before.active.last.transaction_id) {
            result.status = same_record(before.active.last, requested)
                                ? SdJournalStoreStatus::Ready
                                : SdJournalStoreStatus::TransactionConflict;
            return result;
        }
        if (transaction_id < before.active.last.transaction_id) {
            result.status = SdJournalStoreStatus::NonMonotonicTransaction;
            return result;
        }
        current_segment = before.active_segment;
        current_bytes = before.active.file_bytes;
    } else if (before.status != SdJournalStoreStatus::Missing &&
               before.status != SdJournalStoreStatus::Empty) {
        result.status = before.status;
        return result;
    }

    const SdJournalSegmentPlan plan = sd_journal_plan_append(
        current_segment, current_bytes, encoded_size);
    if (plan.status == SdJournalSegmentPlanStatus::SegmentLimitReached) {
        result.status = SdJournalStoreStatus::SegmentLimitReached;
        return result;
    }
    if (plan.status == SdJournalSegmentPlanStatus::InvalidArgument) {
        return result;
    }

    char target_path[kMaxPathBytes] = {};
    if (!make_segment_path(paths, plan.segment_number, target_path,
                           sizeof(target_path))) {
        return result;
    }
    const SdJournalFilePaths file_paths = {
        paths.application_root,
        paths.journal_directory,
        target_path,
    };
    const SdJournalAppendResult appended = sd_journal_append_file(
        file_paths, dataset_id, type, transaction_id, payload, payload_size);
    if (appended.status != SdJournalAppendStatus::Ok) {
        result.status = map_append_status(appended.status);
        return result;
    }

    const SdJournalStoreProbe after =
        sd_journal_store_probe(paths, dataset_id);
    result.probe = after;
    const uint32_t before_count =
        before.status == SdJournalStoreStatus::Ready
            ? before.total_record_count
            : 0U;
    const uint64_t before_bytes =
        before.status == SdJournalStoreStatus::Ready ? before.total_bytes : 0U;
    if (after.status != SdJournalStoreStatus::Ready ||
        after.total_record_count != before_count + 1U ||
        after.total_bytes != before_bytes + encoded_size ||
        !same_record(after.active.last, requested)) {
        result.status = after.status == SdJournalStoreStatus::IoError
                            ? SdJournalStoreStatus::IoError
                            : SdJournalStoreStatus::VerifyError;
        return result;
    }
    result.status = SdJournalStoreStatus::Ready;
    result.changed = true;
    return result;
}

}  // namespace sticky_storage
