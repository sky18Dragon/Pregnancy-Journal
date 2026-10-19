#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>

#include <unistd.h>

#include "sd_recovery_loader.h"
#include "sd_journal_payload.h"
#include "sd_journal_store.h"
#include "sd_record_object_file.h"
#include "storage_crc32.h"

namespace {

sticky_storage::RepositoryKey note_key()
{
    sticky_storage::RepositoryKey key = {};
    assert(sticky_storage::repository_key_make(
        sticky_storage::RepositoryKind::Preference,
        "00000000000000000000000000000001", key));
    return key;
}

}  // namespace

int main()
{
    using namespace sticky_storage;
    constexpr char kDataset[] = "0123456789abcdef0123456789abcdef";
    constexpr char kObjectName[] = "REC00001.BIN";
    constexpr char kIndexName[] = "INDEX001.BIN";
    const std::array<uint8_t, 16U> object = {
        0x50U, 0x52U, 0x45U, 0x47U, 1U, 2U, 3U, 4U,
        5U, 6U, 7U, 8U, 9U, 10U, 11U, 12U};

    char temporary[] = "/tmp/sticky_recovery_loader_XXXXXX";
    char *root = ::mkdtemp(temporary);
    assert(root != nullptr);
    char application[192U] = {};
    char journal_directory[192U] = {};
    char index_directory[192U] = {};
    char records_directory[192U] = {};
    char object_path[192U] = {};
    char index_path[192U] = {};
    assert(std::snprintf(application, sizeof(application), "%s/PREGJNL",
                         root) > 0);
    assert(std::snprintf(journal_directory, sizeof(journal_directory),
                         "%s/JOURNAL", application) > 0);
    assert(std::snprintf(index_directory, sizeof(index_directory),
                         "%s/INDEX", application) > 0);
    assert(std::snprintf(records_directory, sizeof(records_directory),
                         "%s/RECORDS", application) > 0);
    assert(std::snprintf(object_path, sizeof(object_path), "%s/%s",
                         records_directory, kObjectName) > 0);
    assert(std::snprintf(index_path, sizeof(index_path), "%s/%s",
                         index_directory, kIndexName) > 0);

    const SdRecordObjectPaths object_paths = {
        application, records_directory, object_path};
    const SdRecordObjectWriteResult object_write =
        sd_record_object_write(object_paths, object.data(), object.size());
    assert(object_write.status == SdRecordObjectWriteStatus::Ok);

    SdJournalTransactionPayload transaction = {};
    transaction.mutation_count = 1U;
    transaction.mutations[0].type = RepositoryMutationType::Upsert;
    transaction.mutations[0].key = note_key();
    transaction.mutations[0].expected_revision = 0U;
    transaction.mutations[0].committed_revision = 1U;
    transaction.mutations[0].object_bytes = object.size();
    transaction.mutations[0].object_crc32 =
        crc32(object.data(), object.size());
    std::strcpy(transaction.mutations[0].object_file, kObjectName);
    std::array<uint8_t, kSdJournalMaxPayloadBytes> transaction_bytes = {};
    const size_t transaction_size = sd_journal_encode_transaction_payload(
        transaction, transaction_bytes.data(), transaction_bytes.size());
    assert(transaction_size != 0U);

    const SdJournalStorePaths journal_paths = {
        application, journal_directory};
    SdJournalStoreAppendResult appended = sd_journal_store_append(
        journal_paths, kDataset, SdJournalRecordType::Commit, 1U,
        transaction_bytes.data(), transaction_size);
    assert(appended.status == SdJournalStoreStatus::Ready);

    SdIndexSnapshot snapshot = {};
    std::strcpy(snapshot.dataset_id, kDataset);
    snapshot.last_transaction_id = 1U;
    snapshot.entry_count = 1U;
    snapshot.entries[0].key = note_key();
    snapshot.entries[0].revision = 1U;
    snapshot.entries[0].object_bytes = object.size();
    snapshot.entries[0].object_crc32 = crc32(object.data(), object.size());
    std::strcpy(snapshot.entries[0].object_file, kObjectName);
    std::array<uint8_t, kSdIndexSnapshotMaxBytes> index_bytes = {};
    const size_t index_size = sd_index_snapshot_encode(
        snapshot, index_bytes.data(), index_bytes.size());
    assert(index_size != 0U);
    const SdIndexFilePaths index_paths = {
        application, index_directory, index_path};
    const SdIndexWriteResult index_write = sd_index_file_write(
        index_paths, index_bytes.data(), index_size);
    assert(index_write.status == SdIndexWriteStatus::Ok);

    SdJournalCheckpointPayload checkpoint = {};
    checkpoint.last_transaction_id = 1U;
    checkpoint.segment_number = 1U;
    checkpoint.live_record_count = 1U;
    checkpoint.index_bytes = index_size;
    checkpoint.index_crc32 = crc32(index_bytes.data(), index_size);
    std::strcpy(checkpoint.index_file, kIndexName);
    std::array<uint8_t, kSdJournalMaxPayloadBytes> checkpoint_bytes = {};
    const size_t checkpoint_size = sd_journal_encode_checkpoint_payload(
        checkpoint, checkpoint_bytes.data(), checkpoint_bytes.size());
    assert(checkpoint_size != 0U);
    appended = sd_journal_store_append(
        journal_paths, kDataset, SdJournalRecordType::Checkpoint, 2U,
        checkpoint_bytes.data(), checkpoint_size);
    assert(appended.status == SdJournalStoreStatus::Ready);

    const SdRecoveryPaths recovery_paths = {
        application, journal_directory, index_directory, records_directory};
    SdRepositoryRecoveryIndex recovery;
    SdRecoveryWorkspace workspace;
    SdRecoveryLoadResult loaded = sd_recovery_load(
        recovery_paths, kDataset, recovery, workspace);
    assert(loaded.status == SdRecoveryLoadStatus::ReadyCheckpoint);
    assert(loaded.view_ready && loaded.checkpoint_used);
    assert(loaded.journal_records == 2U);
    assert(loaded.last_transaction_id == 2U);
    assert(loaded.snapshot_transaction_id == 1U);
    assert(loaded.stored_records == 1U && loaded.live_records == 1U);
    assert(loaded.verified_objects == 1U);
    SdRecoveredRecord recovered = {};
    assert(recovery.read(note_key(), recovered) ==
           SdRepositoryRecoveryStatus::Ok);
    assert(std::strcmp(recovered.object_file, kObjectName) == 0);

    std::FILE *file = std::fopen(object_path, "r+b");
    assert(file != nullptr);
    assert(std::fputc(0x00, file) != EOF);
    assert(std::fclose(file) == 0);
    loaded = sd_recovery_load(recovery_paths, kDataset, recovery, workspace);
    assert(loaded.status == SdRecoveryLoadStatus::ObjectMismatch);
    assert(!loaded.view_ready && recovery.live_count() == 0U);

    file = std::fopen(object_path, "wb");
    assert(file != nullptr);
    assert(std::fwrite(object.data(), 1U, object.size(), file) ==
           object.size());
    assert(std::fclose(file) == 0);
    assert(::unlink(index_path) == 0);
    loaded = sd_recovery_load(recovery_paths, kDataset, recovery, workspace);
    assert(loaded.status == SdRecoveryLoadStatus::IndexMissing);
    assert(!loaded.view_ready);

    std::filesystem::remove_all(root);
    return 0;
}
