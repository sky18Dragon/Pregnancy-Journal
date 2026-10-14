#include "sd_repository_request.h"

#include <cstdio>
#include <cstring>

#include "sd_journal_payload.h"
#include "sd_storage_layout.h"
#include "sd_storage_policy.h"
#include "storage_contract.h"
#include "storage_crc32.h"

namespace sticky_storage {
namespace {

bool dataset_copy(uint32_t mount_generation, const char *dataset_id,
                  SdRepositoryRequest &request)
{
    if (mount_generation == 0U || !sd_dataset_id_valid(dataset_id)) {
        return false;
    }
    request.mount_generation = mount_generation;
    std::memcpy(request.dataset_id, dataset_id, sizeof(request.dataset_id));
    return true;
}

bool valid_request(const SdRepositoryRequest &request)
{
    if (request.mount_generation == 0U ||
        !sd_dataset_id_valid(request.dataset_id)) {
        return false;
    }
    switch (request.operation) {
    case SdRepositoryRequestOperation::Read:
        return repository_key_valid(request.key) &&
               request.transaction_id == 0U;
    case SdRepositoryRequestOperation::List:
        return repository_kind_valid(request.page.repository) &&
               request.page.limit != 0U &&
               request.page.limit <= kRepositoryPageEntries &&
               request.transaction_id == 0U;
    case SdRepositoryRequestOperation::Commit:
        return request.mutation.type == RepositoryMutationType::Upsert &&
               repository_key_valid(request.mutation.key) &&
               request.mutation.expected_revision != UINT32_MAX &&
               request.mutation.payload_size != 0U &&
               request.mutation.payload_size <=
                   request.mutation.payload.size();
    case SdRepositoryRequestOperation::Erase:
        return request.mutation.type == RepositoryMutationType::Delete &&
               repository_key_valid(request.mutation.key) &&
               request.mutation.expected_revision != 0U &&
               request.mutation.expected_revision != UINT32_MAX &&
               request.mutation.payload_size == 0U;
    }
    return false;
}

SdRepositoryRequestStatus validate_context(
    const SdRepositoryRequest &request,
    const SdRepositoryRequestContext &context, bool write, bool retry)
{
    if (request.mount_generation != context.mount_generation) {
        return SdRepositoryRequestStatus::StaleGeneration;
    }
    switch (context.card_state) {
    case SdCardState::Absent:
    case SdCardState::Unmounted:
        return SdRepositoryRequestStatus::CardMissing;
    case SdCardState::ReadyReadOnly:
        if (write) return SdRepositoryRequestStatus::CardReadOnly;
        break;
    case SdCardState::Full:
        if (write && !retry) return SdRepositoryRequestStatus::CardFull;
        break;
    case SdCardState::ReadyReadWrite:
        break;
    case SdCardState::Mounting:
    case SdCardState::Fault:
    case SdCardState::Ejecting:
        return SdRepositoryRequestStatus::CardUnavailable;
    }
    if (!context.manifest_selected ||
        (context.manifest_state != SdManifestProbeState::Ready &&
         context.manifest_state != SdManifestProbeState::Degraded)) {
        return SdRepositoryRequestStatus::ManifestUnavailable;
    }
    if (std::strcmp(request.dataset_id, context.manifest.dataset_id) != 0) {
        return SdRepositoryRequestStatus::DatasetMismatch;
    }
    if (!context.recovery_view_ready) {
        return SdRepositoryRequestStatus::RecoveryUnavailable;
    }
    if (write && !retry && !context.capacity_known) {
        return SdRepositoryRequestStatus::CapacityUnavailable;
    }
    return SdRepositoryRequestStatus::Ok;
}

SdRepositoryRequestStatus map_read_status(
    SdRepositoryRecoveryStatus status)
{
    switch (status) {
    case SdRepositoryRecoveryStatus::Ok:
        return SdRepositoryRequestStatus::Ok;
    case SdRepositoryRecoveryStatus::InvalidArgument:
        return SdRepositoryRequestStatus::InvalidArgument;
    case SdRepositoryRecoveryStatus::NotFound:
        return SdRepositoryRequestStatus::NotFound;
    case SdRepositoryRecoveryStatus::Tombstoned:
        return SdRepositoryRequestStatus::Tombstoned;
    case SdRepositoryRecoveryStatus::CapacityReached:
        return SdRepositoryRequestStatus::CapacityReached;
    case SdRepositoryRecoveryStatus::RevisionConflict:
        return SdRepositoryRequestStatus::RevisionConflict;
    case SdRepositoryRecoveryStatus::TransactionConflict:
    case SdRepositoryRecoveryStatus::NonMonotonicTransaction:
        return SdRepositoryRequestStatus::TransactionConflict;
    case SdRepositoryRecoveryStatus::UnsupportedPayload:
    case SdRepositoryRecoveryStatus::CheckpointMismatch:
        return SdRepositoryRequestStatus::RecoveryError;
    }
    return SdRepositoryRequestStatus::RecoveryError;
}

bool object_path(const SdRepositoryRequestPaths &paths, const char *name,
                 char output[kMaxPathBytes])
{
    if (paths.application_root == nullptr || paths.records_directory == nullptr ||
        paths.journal.application_root == nullptr ||
        paths.journal.journal_directory == nullptr || name == nullptr) {
        return false;
    }
    const int written = std::snprintf(output, kMaxPathBytes, "%s/%s",
                                      paths.records_directory, name);
    return written > 0 && static_cast<size_t>(written) < kMaxPathBytes;
}

bool same_retry(const RepositoryMutation &mutation,
                const SdJournalMutationReference &reference,
                const SdRecoveredRecord &current,
                SdRepositoryRecoveryStatus current_status)
{
    if (mutation.type == RepositoryMutationType::Delete) {
        return current_status == SdRepositoryRecoveryStatus::Tombstoned &&
               current.revision == reference.committed_revision;
    }
    return current_status == SdRepositoryRecoveryStatus::Ok &&
           current.revision == reference.committed_revision &&
           current.object_bytes == reference.object_bytes &&
           current.object_crc32 == reference.object_crc32;
}

SdRepositoryRequestStatus validate_new_mutation(
    const RepositoryMutation &mutation,
    const SdRepositoryRecoveryIndex &index,
    SdRecoveredRecord &current)
{
    const SdRepositoryRecoveryStatus found = index.read(mutation.key, current);
    if (found == SdRepositoryRecoveryStatus::Tombstoned) {
        return SdRepositoryRequestStatus::Tombstoned;
    }
    if (mutation.type == RepositoryMutationType::Delete) {
        if (found == SdRepositoryRecoveryStatus::NotFound) {
            return SdRepositoryRequestStatus::NotFound;
        }
        if (found != SdRepositoryRecoveryStatus::Ok) {
            return map_read_status(found);
        }
        return current.revision == mutation.expected_revision
                   ? SdRepositoryRequestStatus::Ok
                   : SdRepositoryRequestStatus::RevisionConflict;
    }
    if (found == SdRepositoryRecoveryStatus::NotFound) {
        if (mutation.expected_revision != 0U) {
            return SdRepositoryRequestStatus::RevisionConflict;
        }
        return index.stored_count() < kSdRecoveryRecordCapacity
                   ? SdRepositoryRequestStatus::Ok
                   : SdRepositoryRequestStatus::CapacityReached;
    }
    if (found != SdRepositoryRecoveryStatus::Ok) {
        return map_read_status(found);
    }
    return current.revision == mutation.expected_revision
               ? SdRepositoryRequestStatus::Ok
               : SdRepositoryRequestStatus::RevisionConflict;
}

}  // namespace

bool sd_repository_make_read_request(
    uint32_t mount_generation, const char *dataset_id,
    const RepositoryKey &key, SdRepositoryRequest &request)
{
    request = {};
    request.operation = SdRepositoryRequestOperation::Read;
    request.key = key;
    if (!dataset_copy(mount_generation, dataset_id, request) ||
        !valid_request(request)) {
        request = {};
        return false;
    }
    return true;
}

bool sd_repository_make_list_request(
    uint32_t mount_generation, const char *dataset_id,
    const RepositoryPageRequest &page, SdRepositoryRequest &request)
{
    request = {};
    request.operation = SdRepositoryRequestOperation::List;
    request.page = page;
    if (!dataset_copy(mount_generation, dataset_id, request) ||
        !valid_request(request)) {
        request = {};
        return false;
    }
    return true;
}

bool sd_repository_make_commit_request(
    uint32_t mount_generation, const char *dataset_id,
    const RepositoryMutation &mutation, uint64_t transaction_id,
    SdRepositoryRequest &request)
{
    request = {};
    request.operation = SdRepositoryRequestOperation::Commit;
    request.mutation = mutation;
    request.transaction_id = transaction_id;
    if (!dataset_copy(mount_generation, dataset_id, request) ||
        !valid_request(request)) {
        request = {};
        return false;
    }
    return true;
}

bool sd_repository_make_erase_request(
    uint32_t mount_generation, const char *dataset_id,
    const RepositoryKey &key, uint32_t expected_revision,
    uint64_t transaction_id, SdRepositoryRequest &request)
{
    request = {};
    request.operation = SdRepositoryRequestOperation::Erase;
    request.transaction_id = transaction_id;
    if (!repository_mutation_make_delete(key, expected_revision,
                                         request.mutation) ||
        !dataset_copy(mount_generation, dataset_id, request) ||
        !valid_request(request)) {
        request = {};
        return false;
    }
    return true;
}

bool sd_repository_object_name(const uint8_t *payload, size_t payload_size,
                               char name[kSdJournalObjectNameBytes + 1U])
{
    if (name == nullptr) return false;
    name[0] = '\0';
    if (payload == nullptr || payload_size == 0U ||
        payload_size > kRepositoryPayloadBytes) {
        return false;
    }
    // CRC-protected payloads have a constant CRC residue. Use an independent
    // hash for names; immutable byte comparison below resolves collisions.
    uint32_t hash = 2166136261U;
    for (size_t i = 0; i < payload_size; ++i)
        hash = (hash ^ payload[i]) * 16777619U;
    const int written = std::snprintf(
        name, kSdJournalObjectNameBytes + 1U, "%08X.BIN",
        static_cast<unsigned>(hash));
    return written == 12 && fat83_name_valid(name);
}

SdRepositoryRequestResult sd_repository_execute_request(
    const SdRepositoryRequest &request,
    const SdRepositoryRequestContext &context,
    const SdRepositoryRequestPaths &paths,
    SdRepositoryRecoveryIndex &index)
{
    SdRepositoryRequestResult result = {};
    if (!valid_request(request)) return result;

    const bool write = request.operation == SdRepositoryRequestOperation::Commit ||
                       request.operation == SdRepositoryRequestOperation::Erase;
    const uint64_t last_transaction = index.last_transaction_id();
    const bool automatic_transaction =
        write && request.transaction_id == kRepositoryAutoTransactionId;
    if (automatic_transaction && last_transaction == UINT64_MAX) {
        result.status = SdRepositoryRequestStatus::TransactionConflict;
        return result;
    }
    const uint64_t transaction_id = automatic_transaction
                                        ? last_transaction + 1U
                                        : request.transaction_id;
    const bool retry = write && !automatic_transaction &&
                       transaction_id == last_transaction;
    result.status = validate_context(request, context, write, retry);
    if (result.status != SdRepositoryRequestStatus::Ok) return result;
    if (write && !retry && transaction_id != last_transaction + 1U) {
        result.status = SdRepositoryRequestStatus::TransactionConflict;
        return result;
    }

    if (request.operation == SdRepositoryRequestOperation::List) {
        const RepositoryStatus listed = index.list(request.page, result.page);
        result.status = listed == RepositoryStatus::Ok
                            ? SdRepositoryRequestStatus::Ok
                            : SdRepositoryRequestStatus::InvalidArgument;
        return result;
    }

    if (request.operation == SdRepositoryRequestOperation::Read) {
        SdRecoveredRecord recovered = {};
        result.recovery_status = index.read(request.key, recovered);
        result.status = map_read_status(result.recovery_status);
        if (result.status != SdRepositoryRequestStatus::Ok) return result;
        char path[kMaxPathBytes] = {};
        if (!object_path(paths, recovered.object_file, path)) {
            result.status = SdRepositoryRequestStatus::InvalidArgument;
            return result;
        }
        result.object_read_status = sd_record_object_read_reference(
            path, recovered.object_bytes, recovered.object_crc32,
            result.record.payload.data(), result.record.payload.size()).status;
        if (result.object_read_status != SdRecordObjectReadStatus::Ok) {
            result.status = SdRepositoryRequestStatus::ObjectError;
            return result;
        }
        result.record.key = recovered.key;
        result.record.revision = recovered.revision;
        result.record.payload_size = recovered.object_bytes;
        return result;
    }

    SdJournalMutationReference reference = {};
    reference.type = request.mutation.type;
    reference.key = request.mutation.key;
    reference.expected_revision = request.mutation.expected_revision;
    reference.committed_revision = request.mutation.expected_revision + 1U;
    if (request.mutation.type == RepositoryMutationType::Upsert) {
        reference.object_bytes = request.mutation.payload_size;
        reference.object_crc32 = crc32(request.mutation.payload.data(),
                                       request.mutation.payload_size);
        if (!sd_repository_object_name(request.mutation.payload.data(),
                                       request.mutation.payload_size,
                                       reference.object_file)) {
            result.status = SdRepositoryRequestStatus::InvalidArgument;
            return result;
        }
    }

    SdRecoveredRecord current = {};
    if (retry) {
        const SdRepositoryRecoveryStatus found =
            index.read(request.mutation.key, current);
        if (!same_retry(request.mutation, reference, current, found)) {
            result.status = SdRepositoryRequestStatus::TransactionConflict;
            return result;
        }
        if (request.mutation.type == RepositoryMutationType::Upsert) {
            // Journal references remain authoritative, including legacy CRC names
            // and collision-resolved names. CRC alone cannot identify a retry.
            std::memcpy(reference.object_file, current.object_file,
                        sizeof(reference.object_file));
            char path[kMaxPathBytes] = {};
            std::array<uint8_t, kRepositoryPayloadBytes> bytes = {};
            if (!object_path(paths, current.object_file, path) ||
                sd_record_object_read_reference(path, current.object_bytes,
                    current.object_crc32, bytes.data(), bytes.size()).status !=
                    SdRecordObjectReadStatus::Ok ||
                std::memcmp(bytes.data(), request.mutation.payload.data(),
                            request.mutation.payload_size) != 0) {
                result.status = SdRepositoryRequestStatus::TransactionConflict;
                return result;
            }
        }
    } else {
        result.status = validate_new_mutation(request.mutation, index, current);
        if (result.status != SdRepositoryRequestStatus::Ok) return result;
        const uint64_t journal_bytes = kSdJournalHeaderBytes +
                                      kSdJournalTransactionHeaderBytes +
                                      kSdJournalMutationBytes;
        const uint64_t requested_bytes = request.mutation.payload_size +
                                         journal_bytes;
        if (sd_storage_check_write(
                SdStorageWriteClass::Business, requested_bytes,
                {context.free_bytes, 0U, 0U}) != SdStoragePolicyStatus::Ok) {
            result.status = SdRepositoryRequestStatus::CardFull;
            return result;
        }
    }

    if (request.mutation.type == RepositoryMutationType::Upsert) {
        char path[kMaxPathBytes] = {};
        if (!object_path(paths, reference.object_file, path)) {
            result.status = SdRepositoryRequestStatus::InvalidArgument;
            return result;
        }
        unsigned base = 0U;
        std::sscanf(reference.object_file, "%8x", &base);
        constexpr unsigned kMaximumNameProbes = 32U;
        for (unsigned probe = 0U; probe < kMaximumNameProbes; ++probe) {
            if (probe != 0U) {
                std::snprintf(reference.object_file, sizeof(reference.object_file),
                              "%08X.BIN", base + probe * 0x9E3779B9U);
                if (!object_path(paths, reference.object_file, path)) break;
            }
            const SdRecordObjectPaths object_paths = {
                paths.application_root, paths.records_directory, path};
            result.object_status = sd_record_object_write(
                object_paths, request.mutation.payload.data(),
                request.mutation.payload_size).status;
            if (result.object_status != SdRecordObjectWriteStatus::ExistingConflict || retry)
                break;
        }
        if (result.object_status != SdRecordObjectWriteStatus::Ok) {
            result.status = SdRepositoryRequestStatus::ObjectError;
            return result;
        }
    }

    SdJournalTransactionPayload transaction = {};
    transaction.mutation_count = 1U;
    transaction.mutations[0] = reference;
    std::array<uint8_t, kSdJournalMaxPayloadBytes> payload = {};
    const size_t payload_size = sd_journal_encode_transaction_payload(
        transaction, payload.data(), payload.size());
    if (payload_size == 0U) {
        result.status = SdRepositoryRequestStatus::InvalidArgument;
        return result;
    }
    const SdJournalRecordType record_type =
        request.mutation.type == RepositoryMutationType::Delete
            ? SdJournalRecordType::Tombstone
            : SdJournalRecordType::Commit;
    const SdJournalStoreAppendResult appended = sd_journal_store_append(
        paths.journal, request.dataset_id, record_type,
        transaction_id, payload.data(), payload_size);
    result.journal_status = appended.status;
    if (appended.status != SdJournalStoreStatus::Ready) {
        result.status = SdRepositoryRequestStatus::JournalError;
        return result;
    }

    SdRepositoryRecoveryResult applied = {};
    result.recovery_status = index.apply(
        record_type, transaction_id, payload.data(), payload_size,
        applied);
    if (result.recovery_status != SdRepositoryRecoveryStatus::Ok) {
        result.status = SdRepositoryRequestStatus::RecoveryError;
        return result;
    }
    result.changed = appended.changed && applied.changed;
    result.batch.changed = result.changed;
    result.batch.transaction_id = transaction_id;
    result.batch.mutation_count = 1U;
    result.batch.revisions[0] = reference.committed_revision;
    result.status = SdRepositoryRequestStatus::Ok;
    return result;
}

void sd_repository_validate_recovered_commit(
    SdRepositoryRequestResult &result, bool view_ready,
    uint64_t recovered_transaction_id)
{
    if (result.status != SdRepositoryRequestStatus::Ok || !result.changed)
        return;
    if (!view_ready || result.batch.transaction_id == 0U ||
        recovered_transaction_id != result.batch.transaction_id)
        result.status = SdRepositoryRequestStatus::RecoveryError;
}

}  // namespace sticky_storage
