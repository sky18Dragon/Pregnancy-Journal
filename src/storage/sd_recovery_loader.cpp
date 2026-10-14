#include "sd_recovery_loader.h"

#include <cerrno>
#include <cstdio>
#include <cstring>

#include "sd_journal_store.h"
#include "sd_record_object_file.h"
#include "sd_storage_layout.h"
#include "storage_crc32.h"

namespace sticky_storage {
namespace {

constexpr size_t kRecoveryPathBytes = 160U;

bool paths_valid(const SdRecoveryPaths &paths)
{
    return paths.application_root != nullptr &&
           paths.journal_directory != nullptr &&
           paths.index_directory != nullptr &&
           paths.records_directory != nullptr &&
           paths.application_root[0] != '\0' &&
           paths.journal_directory[0] != '\0' &&
           paths.index_directory[0] != '\0' &&
           paths.records_directory[0] != '\0';
}

struct DiscoveryContext {
    SdRecoveryLoadStatus error = SdRecoveryLoadStatus::NotAttempted;
    uint64_t previous_transaction_id = 0U;
    uint32_t record_count = 0U;
    bool checkpoint_found = false;
    uint64_t checkpoint_transaction_id = 0U;
    SdJournalCheckpointPayload checkpoint = {};
};

bool discover_record(void *opaque, uint32_t segment_number,
                     const SdJournalRecordView &record)
{
    auto &context = *static_cast<DiscoveryContext *>(opaque);
    ++context.record_count;
    if (!sd_journal_payload_valid_for_record(
            record.type, record.transaction_id, record.payload,
            record.payload_size)) {
        context.error = SdRecoveryLoadStatus::PayloadInvalid;
        return false;
    }
    if (record.type == SdJournalRecordType::Checkpoint) {
        SdJournalCheckpointPayload checkpoint = {};
        if (sd_journal_decode_checkpoint_payload(
                record.payload, record.payload_size, checkpoint) !=
                SdJournalPayloadStatus::Ok ||
            checkpoint.last_transaction_id !=
                context.previous_transaction_id ||
            checkpoint.segment_number != segment_number) {
            context.error = SdRecoveryLoadStatus::CheckpointInvalid;
            return false;
        }
        context.checkpoint_found = true;
        context.checkpoint_transaction_id = record.transaction_id;
        context.checkpoint = checkpoint;
    }
    context.previous_transaction_id = record.transaction_id;
    return true;
}

SdRecoveryLoadStatus map_journal_failure(SdJournalStoreStatus status)
{
    switch (status) {
    case SdJournalStoreStatus::IoError:
        return SdRecoveryLoadStatus::IoError;
    case SdJournalStoreStatus::InvalidArgument:
        return SdRecoveryLoadStatus::InvalidArgument;
    default:
        return SdRecoveryLoadStatus::JournalInvalid;
    }
}

SdRecoveryLoadStatus load_checkpoint_snapshot(
    const SdRecoveryPaths &paths, const char *dataset_id,
    const SdJournalCheckpointPayload &checkpoint,
    SdRecoveryWorkspace &workspace)
{
    if (checkpoint.index_bytes < kSdIndexSnapshotHeaderBytes ||
        checkpoint.index_bytes > workspace.index_bytes.size()) {
        return SdRecoveryLoadStatus::IndexMismatch;
    }
    char path[kRecoveryPathBytes] = {};
    const int written = std::snprintf(path, sizeof(path), "%s/%s",
                                      paths.index_directory,
                                      checkpoint.index_file);
    if (written <= 0 || static_cast<size_t>(written) >= sizeof(path) ||
        !fat83_name_valid(checkpoint.index_file)) {
        return SdRecoveryLoadStatus::CheckpointInvalid;
    }

    errno = 0;
    std::FILE *file = std::fopen(path, "rb");
    if (file == nullptr) {
        return errno == ENOENT ? SdRecoveryLoadStatus::IndexMissing
                               : SdRecoveryLoadStatus::IoError;
    }
    const size_t expected = checkpoint.index_bytes;
    const size_t read = std::fread(workspace.index_bytes.data(), 1U,
                                   expected, file);
    const int extra = std::fgetc(file);
    const bool read_ok = std::ferror(file) == 0;
    const bool close_ok = std::fclose(file) == 0;
    const bool io_ok = read_ok && close_ok;
    if (!io_ok) {
        return SdRecoveryLoadStatus::IoError;
    }
    if (read != expected || extra != EOF ||
        crc32(workspace.index_bytes.data(), expected) !=
            checkpoint.index_crc32) {
        return SdRecoveryLoadStatus::IndexMismatch;
    }
    std::memset(&workspace.snapshot, 0, sizeof(workspace.snapshot));
    if (sd_index_snapshot_decode(workspace.index_bytes.data(), expected,
                                 workspace.snapshot) !=
            SdIndexSnapshotStatus::Ok ||
        std::strcmp(workspace.snapshot.dataset_id, dataset_id) != 0 ||
        workspace.snapshot.last_transaction_id !=
            checkpoint.last_transaction_id) {
        return SdRecoveryLoadStatus::IndexMismatch;
    }
    size_t live = 0U;
    for (size_t index = 0U; index < workspace.snapshot.entry_count; ++index) {
        live += workspace.snapshot.entries[index].tombstone ? 0U : 1U;
    }
    return live == checkpoint.live_record_count
               ? SdRecoveryLoadStatus::ReadyCheckpoint
               : SdRecoveryLoadStatus::IndexMismatch;
}

struct ReplayContext {
    SdRepositoryRecoveryIndex *index = nullptr;
    uint64_t skip_through_transaction_id = 0U;
    uint64_t expected_checkpoint_transaction_id = 0U;
    bool checkpoint_seen = false;
    SdRecoveryLoadStatus error = SdRecoveryLoadStatus::NotAttempted;
};

bool replay_record(void *opaque, uint32_t,
                   const SdJournalRecordView &record)
{
    auto &context = *static_cast<ReplayContext *>(opaque);
    if (record.transaction_id <= context.skip_through_transaction_id) {
        return true;
    }
    SdRepositoryRecoveryResult applied = {};
    if (context.index->apply(record.type, record.transaction_id,
                             record.payload, record.payload_size,
                             applied) != SdRepositoryRecoveryStatus::Ok) {
        context.error = record.type == SdJournalRecordType::Checkpoint
                            ? SdRecoveryLoadStatus::CheckpointInvalid
                            : SdRecoveryLoadStatus::PayloadInvalid;
        return false;
    }
    if (record.transaction_id ==
        context.expected_checkpoint_transaction_id) {
        context.checkpoint_seen = true;
    }
    return true;
}

SdRecoveryLoadStatus verify_objects(
    const SdRecoveryPaths &paths, const SdRepositoryRecoveryIndex &index,
    uint32_t &verified)
{
    verified = 0U;
    for (size_t slot = 0U; slot < kSdRecoveryRecordCapacity; ++slot) {
        SdRecoveredRecord record = {};
        if (!index.record_at(slot, record) || record.tombstone) {
            continue;
        }
        char path[kRecoveryPathBytes] = {};
        const int written = std::snprintf(path, sizeof(path), "%s/%s",
                                          paths.records_directory,
                                          record.object_file);
        if (written <= 0 || static_cast<size_t>(written) >= sizeof(path) ||
            !fat83_name_valid(record.object_file)) {
            return SdRecoveryLoadStatus::ObjectMismatch;
        }
        const SdRecordObjectProbe probe =
            sd_record_object_probe_reference(path, record.object_bytes,
                                             record.object_crc32);
        if (probe.status == SdRecordObjectProbeStatus::Missing) {
            return SdRecoveryLoadStatus::ObjectMissing;
        }
        if (probe.status == SdRecordObjectProbeStatus::IoError) {
            return SdRecoveryLoadStatus::IoError;
        }
        if (probe.status != SdRecordObjectProbeStatus::Match) {
            return SdRecoveryLoadStatus::ObjectMismatch;
        }
        ++verified;
    }
    return SdRecoveryLoadStatus::ReadyJournal;
}

}  // namespace

SdRecoveryLoadResult sd_recovery_load(
    const SdRecoveryPaths &paths, const char *dataset_id,
    SdRepositoryRecoveryIndex &index, SdRecoveryWorkspace &workspace)
{
    SdRecoveryLoadResult result = {};
    index.reset();
    std::memset(&workspace.snapshot, 0, sizeof(workspace.snapshot));
    if (!paths_valid(paths) || !sd_dataset_id_valid(dataset_id)) {
        result.status = SdRecoveryLoadStatus::InvalidArgument;
        return result;
    }

    const SdJournalStorePaths journal_paths = {
        paths.application_root, paths.journal_directory};
    DiscoveryContext discovery = {};
    const SdJournalStoreProbe discovered = sd_journal_store_replay(
        journal_paths, dataset_id, discover_record, &discovery);
    result.journal_records = discovery.record_count;
    if (discovered.status == SdJournalStoreStatus::Missing ||
        discovered.status == SdJournalStoreStatus::Empty) {
        result.status = SdRecoveryLoadStatus::Empty;
        result.view_ready = true;
        return result;
    }
    const bool tail_truncated =
        discovered.status == SdJournalStoreStatus::TailTruncated;
    if (discovered.status == SdJournalStoreStatus::ConsumerRejected) {
        result.status = discovery.error;
        return result;
    }
    if (discovered.status != SdJournalStoreStatus::Ready &&
        !tail_truncated) {
        result.status = map_journal_failure(discovered.status);
        return result;
    }

    uint64_t skip_through = 0U;
    if (discovery.checkpoint_found) {
        result.checkpoint_transaction_id =
            discovery.checkpoint_transaction_id;
        result.snapshot_transaction_id =
            discovery.checkpoint.last_transaction_id;
        result.index_bytes = discovery.checkpoint.index_bytes;
        const SdRecoveryLoadStatus loaded = load_checkpoint_snapshot(
            paths, dataset_id, discovery.checkpoint, workspace);
        if (loaded != SdRecoveryLoadStatus::ReadyCheckpoint) {
            result.status = loaded;
            return result;
        }
        if (index.restore_snapshot(workspace.snapshot) !=
            SdRepositoryRecoveryStatus::Ok) {
            result.status = SdRecoveryLoadStatus::IndexMismatch;
            return result;
        }
        skip_through = workspace.snapshot.last_transaction_id;
        result.checkpoint_used = true;
    }

    ReplayContext replay = {
        &index, skip_through,
        discovery.checkpoint_found
            ? discovery.checkpoint_transaction_id
            : 0U,
        false, SdRecoveryLoadStatus::NotAttempted};
    const SdJournalStoreProbe replayed = sd_journal_store_replay(
        journal_paths, dataset_id, replay_record, &replay);
    if (replayed.status == SdJournalStoreStatus::ConsumerRejected) {
        result.status = replay.error;
        index.reset();
        return result;
    }
    if (replayed.status != SdJournalStoreStatus::Ready &&
        replayed.status != SdJournalStoreStatus::TailTruncated) {
        result.status = map_journal_failure(replayed.status);
        index.reset();
        return result;
    }
    if (discovery.checkpoint_found && !replay.checkpoint_seen) {
        result.status = SdRecoveryLoadStatus::CheckpointInvalid;
        index.reset();
        return result;
    }

    const SdRecoveryLoadStatus objects =
        verify_objects(paths, index, result.verified_objects);
    if (objects != SdRecoveryLoadStatus::ReadyJournal) {
        result.status = objects;
        index.reset();
        return result;
    }
    result.view_ready = true;
    result.last_transaction_id = index.last_transaction_id();
    result.stored_records = static_cast<uint32_t>(index.stored_count());
    result.live_records = static_cast<uint32_t>(index.live_count());
    result.status = tail_truncated
                        ? SdRecoveryLoadStatus::TailTruncated
                        : (discovery.checkpoint_found
                               ? SdRecoveryLoadStatus::ReadyCheckpoint
                               : SdRecoveryLoadStatus::ReadyJournal);
    return result;
}

}  // namespace sticky_storage
