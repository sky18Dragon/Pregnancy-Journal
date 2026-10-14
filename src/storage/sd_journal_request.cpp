#include "sd_journal_request.h"

#include <array>
#include <cstring>

#include "sd_journal_payload.h"

namespace sticky_storage {
namespace {

SdJournalRequestStatus map_card_state(SdCardState state, bool write)
{
    switch (state) {
    case SdCardState::Absent:
    case SdCardState::Unmounted:
        return SdJournalRequestStatus::CardMissing;
    case SdCardState::ReadyReadOnly:
        return write ? SdJournalRequestStatus::CardReadOnly
                     : SdJournalRequestStatus::Ok;
    case SdCardState::Full:
        return write ? SdJournalRequestStatus::NoSpace
                     : SdJournalRequestStatus::Ok;
    case SdCardState::ReadyReadWrite:
        return SdJournalRequestStatus::Ok;
    case SdCardState::Mounting:
    case SdCardState::Fault:
    case SdCardState::Ejecting:
        return SdJournalRequestStatus::CardUnavailable;
    }
    return SdJournalRequestStatus::CardUnavailable;
}

SdJournalRequestStatus map_append_status(SdJournalAppendStatus status)
{
    switch (status) {
    case SdJournalAppendStatus::Ok:
        return SdJournalRequestStatus::Ok;
    case SdJournalAppendStatus::InvalidArgument:
        return SdJournalRequestStatus::InvalidArgument;
    case SdJournalAppendStatus::DirectoryError:
        return SdJournalRequestStatus::DirectoryError;
    case SdJournalAppendStatus::ProbeIoError:
        return SdJournalRequestStatus::ProbeIoError;
    case SdJournalAppendStatus::TailTruncated:
        return SdJournalRequestStatus::TailTruncated;
    case SdJournalAppendStatus::Corrupt:
        return SdJournalRequestStatus::Corrupt;
    case SdJournalAppendStatus::UnsupportedSchema:
        return SdJournalRequestStatus::UnsupportedSchema;
    case SdJournalAppendStatus::UnsupportedRecordType:
        return SdJournalRequestStatus::UnsupportedRecordType;
    case SdJournalAppendStatus::DatasetMismatch:
        return SdJournalRequestStatus::DatasetMismatch;
    case SdJournalAppendStatus::NonMonotonicTransaction:
        return SdJournalRequestStatus::NonMonotonicTransaction;
    case SdJournalAppendStatus::TransactionConflict:
        return SdJournalRequestStatus::TransactionConflict;
    case SdJournalAppendStatus::OpenError:
        return SdJournalRequestStatus::OpenError;
    case SdJournalAppendStatus::ShortWrite:
        return SdJournalRequestStatus::ShortWrite;
    case SdJournalAppendStatus::SyncError:
        return SdJournalRequestStatus::SyncError;
    case SdJournalAppendStatus::CloseError:
        return SdJournalRequestStatus::CloseError;
    case SdJournalAppendStatus::VerifyError:
        return SdJournalRequestStatus::VerifyError;
    }
    return SdJournalRequestStatus::ProbeIoError;
}

SdJournalRequestStatus map_probe_status(SdJournalFileProbeStatus status)
{
    switch (status) {
    case SdJournalFileProbeStatus::Missing:
    case SdJournalFileProbeStatus::Empty:
    case SdJournalFileProbeStatus::Ready:
        return SdJournalRequestStatus::Ok;
    case SdJournalFileProbeStatus::TailTruncated:
        return SdJournalRequestStatus::TailTruncated;
    case SdJournalFileProbeStatus::Corrupt:
        return SdJournalRequestStatus::Corrupt;
    case SdJournalFileProbeStatus::UnsupportedSchema:
        return SdJournalRequestStatus::UnsupportedSchema;
    case SdJournalFileProbeStatus::UnsupportedRecordType:
        return SdJournalRequestStatus::UnsupportedRecordType;
    case SdJournalFileProbeStatus::DatasetMismatch:
        return SdJournalRequestStatus::DatasetMismatch;
    case SdJournalFileProbeStatus::NonMonotonicTransaction:
        return SdJournalRequestStatus::NonMonotonicTransaction;
    case SdJournalFileProbeStatus::ConsumerRejected:
        return SdJournalRequestStatus::Corrupt;
    case SdJournalFileProbeStatus::IoError:
        return SdJournalRequestStatus::ProbeIoError;
    case SdJournalFileProbeStatus::InvalidArgument:
        return SdJournalRequestStatus::InvalidArgument;
    }
    return SdJournalRequestStatus::ProbeIoError;
}

SdJournalRequestStatus map_store_status(SdJournalStoreStatus status)
{
    switch (status) {
    case SdJournalStoreStatus::Missing:
    case SdJournalStoreStatus::Empty:
    case SdJournalStoreStatus::Ready:
        return SdJournalRequestStatus::Ok;
    case SdJournalStoreStatus::TailTruncated:
        return SdJournalRequestStatus::TailTruncated;
    case SdJournalStoreStatus::Corrupt:
    case SdJournalStoreStatus::SegmentGap:
    case SdJournalStoreStatus::SegmentTooLarge:
    case SdJournalStoreStatus::UnexpectedEntry:
        return SdJournalRequestStatus::Corrupt;
    case SdJournalStoreStatus::UnsupportedSchema:
        return SdJournalRequestStatus::UnsupportedSchema;
    case SdJournalStoreStatus::UnsupportedRecordType:
        return SdJournalRequestStatus::UnsupportedRecordType;
    case SdJournalStoreStatus::DatasetMismatch:
        return SdJournalRequestStatus::DatasetMismatch;
    case SdJournalStoreStatus::NonMonotonicTransaction:
        return SdJournalRequestStatus::NonMonotonicTransaction;
    case SdJournalStoreStatus::ConsumerRejected:
        return SdJournalRequestStatus::Corrupt;
    case SdJournalStoreStatus::SegmentLimitReached:
        return SdJournalRequestStatus::NoSpace;
    case SdJournalStoreStatus::IoError:
        return SdJournalRequestStatus::ProbeIoError;
    case SdJournalStoreStatus::InvalidArgument:
        return SdJournalRequestStatus::InvalidArgument;
    case SdJournalStoreStatus::DirectoryError:
        return SdJournalRequestStatus::DirectoryError;
    case SdJournalStoreStatus::TransactionConflict:
        return SdJournalRequestStatus::TransactionConflict;
    case SdJournalStoreStatus::OpenError:
        return SdJournalRequestStatus::OpenError;
    case SdJournalStoreStatus::ShortWrite:
        return SdJournalRequestStatus::ShortWrite;
    case SdJournalStoreStatus::SyncError:
        return SdJournalRequestStatus::SyncError;
    case SdJournalStoreStatus::CloseError:
        return SdJournalRequestStatus::CloseError;
    case SdJournalStoreStatus::VerifyError:
        return SdJournalRequestStatus::VerifyError;
    }
    return SdJournalRequestStatus::ProbeIoError;
}

SdJournalStoreProbe wrap_file_probe(const SdJournalFileProbe &file)
{
    SdJournalStoreProbe store = {};
    store.active = file;
    store.total_bytes = file.file_bytes;
    store.total_record_count = file.record_count;
    if (file.status != SdJournalFileProbeStatus::Missing &&
        file.status != SdJournalFileProbeStatus::Empty) {
        store.segment_count = 1U;
        store.active_segment = 1U;
    }
    switch (file.status) {
    case SdJournalFileProbeStatus::Missing:
        store.status = SdJournalStoreStatus::Missing;
        break;
    case SdJournalFileProbeStatus::Empty:
        store.status = SdJournalStoreStatus::Empty;
        break;
    case SdJournalFileProbeStatus::Ready:
        store.status = SdJournalStoreStatus::Ready;
        break;
    case SdJournalFileProbeStatus::TailTruncated:
        store.status = SdJournalStoreStatus::TailTruncated;
        break;
    case SdJournalFileProbeStatus::Corrupt:
        store.status = SdJournalStoreStatus::Corrupt;
        break;
    case SdJournalFileProbeStatus::UnsupportedSchema:
        store.status = SdJournalStoreStatus::UnsupportedSchema;
        break;
    case SdJournalFileProbeStatus::UnsupportedRecordType:
        store.status = SdJournalStoreStatus::UnsupportedRecordType;
        break;
    case SdJournalFileProbeStatus::DatasetMismatch:
        store.status = SdJournalStoreStatus::DatasetMismatch;
        break;
    case SdJournalFileProbeStatus::NonMonotonicTransaction:
        store.status = SdJournalStoreStatus::NonMonotonicTransaction;
        break;
    case SdJournalFileProbeStatus::ConsumerRejected:
        store.status = SdJournalStoreStatus::ConsumerRejected;
        break;
    case SdJournalFileProbeStatus::IoError:
        store.status = SdJournalStoreStatus::IoError;
        break;
    case SdJournalFileProbeStatus::InvalidArgument:
        store.status = SdJournalStoreStatus::InvalidArgument;
        break;
    }
    return store;
}

bool valid_request(const SdJournalRequest &request)
{
    if (request.mount_generation == 0U ||
        !sd_dataset_id_valid(request.dataset_id) ||
        request.payload_size > request.payload.size()) {
        return false;
    }
    if (request.operation == SdJournalRequestOperation::Probe) {
        return request.transaction_id == 0U && request.payload_size == 0U;
    }
    if (request.operation != SdJournalRequestOperation::Append) {
        return false;
    }

    if (!sd_journal_payload_valid_for_record(
            request.type, request.transaction_id, request.payload.data(),
            request.payload_size)) {
        return false;
    }

    std::array<uint8_t, kSdJournalMaxRecordBytes> encoded = {};
    return sd_journal_encode_record(
               request.type, request.dataset_id, request.transaction_id,
               request.payload.data(), request.payload_size, encoded.data(),
               encoded.size()) != 0U;
}

SdJournalRequestStatus validate_context(
    const SdJournalRequest &request, const SdJournalRequestContext &context)
{
    if (request.mount_generation != context.mount_generation) {
        return SdJournalRequestStatus::StaleGeneration;
    }
    const bool write = request.operation == SdJournalRequestOperation::Append;
    const SdJournalRequestStatus card = map_card_state(context.card_state,
                                                       write);
    if (card != SdJournalRequestStatus::Ok) {
        return card;
    }
    if (!context.manifest_selected ||
        (context.manifest_state != SdManifestProbeState::Ready &&
         context.manifest_state != SdManifestProbeState::Degraded)) {
        return SdJournalRequestStatus::ManifestUnavailable;
    }
    if (std::strcmp(request.dataset_id, context.manifest.dataset_id) != 0) {
        return SdJournalRequestStatus::DatasetMismatch;
    }
    return SdJournalRequestStatus::Ok;
}

}  // namespace

bool sd_journal_make_probe_request(uint32_t mount_generation,
                                   const char *dataset_id,
                                   SdJournalRequest &request)
{
    request = {};
    if (mount_generation == 0U || !sd_dataset_id_valid(dataset_id)) {
        return false;
    }
    request.operation = SdJournalRequestOperation::Probe;
    request.mount_generation = mount_generation;
    std::memcpy(request.dataset_id, dataset_id, sizeof(request.dataset_id));
    return true;
}

bool sd_journal_make_append_request(
    uint32_t mount_generation, const char *dataset_id,
    SdJournalRecordType type, uint64_t transaction_id,
    const uint8_t *payload, size_t payload_size, SdJournalRequest &request)
{
    request = {};
    if (mount_generation == 0U || !sd_dataset_id_valid(dataset_id) ||
        (payload == nullptr && payload_size != 0U) ||
        payload_size > request.payload.size()) {
        return false;
    }
    request.operation = SdJournalRequestOperation::Append;
    request.mount_generation = mount_generation;
    std::memcpy(request.dataset_id, dataset_id, sizeof(request.dataset_id));
    request.type = type;
    request.transaction_id = transaction_id;
    request.payload_size = payload_size;
    if (payload_size != 0U) {
        std::memcpy(request.payload.data(), payload, payload_size);
    }
    if (!valid_request(request)) {
        request = {};
        return false;
    }
    return true;
}

SdJournalRequestResult sd_journal_execute_request_with_ops(
    const SdJournalRequest &request, const SdJournalRequestContext &context,
    const SdJournalFilePaths &paths, const SdJournalFileOps &ops)
{
    SdJournalRequestResult result = {};
    if (!valid_request(request)) {
        return result;
    }
    result.status = validate_context(request, context);
    if (result.status != SdJournalRequestStatus::Ok) {
        return result;
    }

    if (request.operation == SdJournalRequestOperation::Probe) {
        const SdJournalFileProbe file = ops.probe_segment(
            ops.context, paths.segment, request.dataset_id);
        result.probe = wrap_file_probe(file);
        result.status = map_probe_status(file.status);
        return result;
    }

    const SdJournalAppendResult appended = sd_journal_append_file_with_ops(
        paths, request.dataset_id, request.type, request.transaction_id,
        request.payload.data(), request.payload_size, ops);
    result.status = map_append_status(appended.status);
    result.changed = appended.changed;
    result.probe = wrap_file_probe(appended.probe);
    return result;
}

SdJournalRequestResult sd_journal_execute_request(
    const SdJournalRequest &request, const SdJournalRequestContext &context,
    const SdJournalStorePaths &paths)
{
    SdJournalRequestResult result = {};
    if (!valid_request(request)) {
        return result;
    }
    result.status = validate_context(request, context);
    if (result.status != SdJournalRequestStatus::Ok) {
        return result;
    }

    if (request.operation == SdJournalRequestOperation::Probe) {
        result.probe = sd_journal_store_probe(paths, request.dataset_id);
        result.status = map_store_status(result.probe.status);
        return result;
    }
    const SdJournalStoreAppendResult appended = sd_journal_store_append(
        paths, request.dataset_id, request.type, request.transaction_id,
        request.payload.data(), request.payload_size);
    result.status = map_store_status(appended.status);
    result.changed = appended.changed;
    result.probe = appended.probe;
    return result;
}

}  // namespace sticky_storage
