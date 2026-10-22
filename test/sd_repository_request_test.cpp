#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <sys/stat.h>
#include <unistd.h>

#include "sd_repository_request.h"
#include "sd_manifest_request.h"
#include "sd_recovery_loader.h"

namespace {
constexpr char kDataset[] = "0123456789abcdef0123456789abcdef";

void make_path(char *output, size_t capacity, const char *left,
               const char *right)
{
    const int written = std::snprintf(output, capacity, "%s/%s", left, right);
    assert(written > 0 && static_cast<size_t>(written) < capacity);
}

sticky_storage::RepositoryKey key(unsigned id)
{
    char text[sticky_storage::kRepositoryRecordIdCharacters + 1U] = {};
    std::snprintf(text, sizeof(text), "%032x", id);
    sticky_storage::RepositoryKey result = {};
    assert(sticky_storage::repository_key_make(
        sticky_storage::RepositoryKind::Weight, text, result));
    return result;
}

sticky_storage::SdRepositoryRequestContext context()
{
    using namespace sticky_storage;
    SdRepositoryRequestContext value = {};
    value.card_state = SdCardState::ReadyReadWrite;
    value.mount_generation = 4U;
    value.capacity_known = true;
    value.free_bytes = 1024U * 1024U;
    value.manifest_state = SdManifestProbeState::Ready;
    value.manifest_selected = true;
    value.manifest.schema_version = kSdManifestSchemaVersion;
    std::strcpy(value.manifest.dataset_id, kDataset);
    value.manifest.commit_generation = 1U;
    value.recovery_view_ready = true;
    return value;
}
}  // namespace

int main()
{
    using namespace sticky_storage;
    char temporary[] = "/tmp/srr_XXXXXX";
    char *root = ::mkdtemp(temporary);
    assert(root != nullptr);
    char application[kMaxPathBytes] = {};
    char records[kMaxPathBytes] = {};
    char journal[kMaxPathBytes] = {};
    char segment[kMaxPathBytes] = {};
    char manifests[kMaxPathBytes] = {};
    char slot_a[kMaxPathBytes] = {};
    char slot_b[kMaxPathBytes] = {};
    char snapshots[kMaxPathBytes] = {};
    make_path(application, sizeof(application), root, "PREGJNL");
    make_path(records, sizeof(records), application, "RECORDS");
    make_path(journal, sizeof(journal), application, "JOURNAL");
    make_path(segment, sizeof(segment), journal, "TXN00001.LOG");
    make_path(manifests, sizeof(manifests), application, "MANIFEST");
    make_path(slot_a, sizeof(slot_a), manifests, "SLOTA.BIN");
    make_path(slot_b, sizeof(slot_b), manifests, "SLOTB.BIN");
    make_path(snapshots, sizeof(snapshots), application, "INDEX");
    const SdRepositoryRequestPaths paths = {
        application, records, {application, journal}};
    SdRepositoryRecoveryIndex index;
    // A successful explicit initialization must be followed by recovery:
    // no journal is a valid empty view, ready for reads and the first commit.
    const SdManifestPaths manifest_paths = {
        application, manifests, slot_a, slot_b};
    SdManifestRequest initialize = {};
    assert(sd_manifest_make_initialize_request(4U, kDataset, true, initialize));
    const auto initialized = sd_manifest_execute_request(
        initialize, {SdCardState::ReadyReadWrite, 4U}, manifest_paths);
    assert(initialized.status == SdManifestRequestStatus::Ok);
    const auto manifest = sd_manifest_probe(slot_a, slot_b);
    assert(manifest.state == SdManifestProbeState::Degraded);
    const SdRecoveryPaths recovery_paths = {
        application, journal, snapshots, records};
    SdRecoveryWorkspace workspace;
    auto loaded = sd_recovery_load(
        recovery_paths, manifest.selected.dataset_id, index, workspace);
    assert(loaded.status == SdRecoveryLoadStatus::Empty && loaded.view_ready);
    assert(loaded.last_transaction_id == 0U && index.live_count() == 0U);
    const RepositoryKey record_key = key(9U);
    const std::array<uint8_t, 5U> first = {1U, 3U, 5U, 7U, 9U};
    RepositoryMutation mutation = {};
    assert(repository_mutation_make_upsert(
        record_key, 0U, first.data(), first.size(), mutation));
    SdRepositoryRequest request = {};
    assert(sd_repository_make_commit_request(
        4U, kDataset, mutation, 1U, request));
    SdRepositoryRequestContext ready = context();
    ready.manifest_state = manifest.state;
    ready.manifest = manifest.selected;
    ready.recovery_view_ready = loaded.view_ready;
    SdRepositoryRequest empty_read = {};
    assert(sd_repository_make_read_request(4U, kDataset, record_key, empty_read));
    assert(sd_repository_execute_request(empty_read, ready, paths, index).status ==
           SdRepositoryRequestStatus::NotFound);
    SdRepositoryRequestResult result = sd_repository_execute_request(
        request, ready, paths, index);
    assert(result.status == SdRepositoryRequestStatus::Ok);
    assert(result.changed && result.batch.revisions[0] == 1U);
    assert(index.last_transaction_id() == 1U && index.live_count() == 1U);
    loaded = sd_recovery_load(
        recovery_paths, kDataset, index, workspace);
    assert(loaded.status == SdRecoveryLoadStatus::ReadyJournal);
    assert(loaded.view_ready && loaded.last_transaction_id == 1U &&
           loaded.live_records == 1U && loaded.verified_objects == 1U);

    char first_name[kSdJournalObjectNameBytes + 1U] = {};
    assert(sd_repository_object_name(first.data(), first.size(), first_name));
    char first_path[kMaxPathBytes] = {};
    make_path(first_path, sizeof(first_path), records, first_name);
    assert(::access(first_path, F_OK) == 0);

    SdRepositoryRequest read_request = {};
    assert(sd_repository_make_read_request(
        4U, kDataset, record_key, read_request));
    result = sd_repository_execute_request(read_request, ready, paths, index);
    assert(result.status == SdRepositoryRequestStatus::Ok);
    assert(result.record.revision == 1U &&
           result.record.payload_size == first.size());
    assert(std::memcmp(result.record.payload.data(), first.data(),
                       first.size()) == 0);

    RepositoryPageRequest page_request = {};
    page_request.repository = RepositoryKind::Weight;
    page_request.limit = 1U;
    SdRepositoryRequest list_request = {};
    assert(sd_repository_make_list_request(
        4U, kDataset, page_request, list_request));
    result = sd_repository_execute_request(list_request, ready, paths, index);
    assert(result.status == SdRepositoryRequestStatus::Ok);
    assert(result.page.count == 1U && result.page.entries[0].revision == 1U);

    // Exact transaction retry probes the same immutable object and journal
    // frame but does not publish a second mutation.
    result = sd_repository_execute_request(request, ready, paths, index);
    assert(result.status == SdRepositoryRequestStatus::Ok && !result.changed);
    RepositoryMutation conflicting = {};
    const std::array<uint8_t, 3U> conflict_bytes = {8U, 8U, 8U};
    assert(repository_mutation_make_upsert(
        record_key, 0U, conflict_bytes.data(), conflict_bytes.size(),
        conflicting));
    SdRepositoryRequest conflict_request = {};
    assert(sd_repository_make_commit_request(
        4U, kDataset, conflicting, 1U, conflict_request));
    result = sd_repository_execute_request(
        conflict_request, ready, paths, index);
    assert(result.status == SdRepositoryRequestStatus::TransactionConflict);

    const std::array<uint8_t, 4U> second = {2U, 4U, 6U, 8U};
    assert(repository_mutation_make_upsert(
        record_key, 0U, second.data(), second.size(), mutation));
    assert(sd_repository_make_commit_request(
        4U, kDataset, mutation, 2U, request));
    result = sd_repository_execute_request(request, ready, paths, index);
    assert(result.status == SdRepositoryRequestStatus::RevisionConflict);
    assert(repository_mutation_make_upsert(
        record_key, 1U, second.data(), second.size(), mutation));
    assert(sd_repository_make_commit_request(
        4U, kDataset, mutation, 2U, request));
    result = sd_repository_execute_request(request, ready, paths, index);
    assert(result.status == SdRepositoryRequestStatus::Ok && result.changed);
    assert(result.batch.revisions[0] == 2U);

    SdRepositoryRequest erase = {};
    assert(sd_repository_make_erase_request(
        4U, kDataset, record_key, 2U, 3U, erase));
    result = sd_repository_execute_request(erase, ready, paths, index);
    assert(result.status == SdRepositoryRequestStatus::Ok && result.changed);
    result = sd_repository_execute_request(erase, ready, paths, index);
    assert(result.status == SdRepositoryRequestStatus::Ok && !result.changed);
    result = sd_repository_execute_request(read_request, ready, paths, index);
    assert(result.status == SdRepositoryRequestStatus::Tombstoned);

    page_request.cursor = 0U;
    page_request.limit = kRepositoryPageEntries;
    page_request.include_tombstones = true;
    assert(sd_repository_make_list_request(
        4U, kDataset, page_request, list_request));
    result = sd_repository_execute_request(list_request, ready, paths, index);
    assert(result.status == SdRepositoryRequestStatus::Ok);
    assert(result.page.count == 1U && result.page.entries[0].tombstone);

    // Tombstones are permanent for a record ID.
    assert(repository_mutation_make_upsert(
        record_key, 3U, first.data(), first.size(), mutation));
    assert(sd_repository_make_commit_request(
        4U, kDataset, mutation, 4U, request));
    result = sd_repository_execute_request(request, ready, paths, index);
    assert(result.status == SdRepositoryRequestStatus::Tombstoned);

    // A zero service-level transaction is assigned atomically by the worker.
    const RepositoryKey automatic_key = key(11U);
    assert(repository_mutation_make_upsert(
        automatic_key, 0U, first.data(), first.size(), mutation));
    assert(sd_repository_make_commit_request(
        4U, kDataset, mutation, kRepositoryAutoTransactionId, request));
    result = sd_repository_execute_request(request, ready, paths, index);
    assert(result.status == SdRepositoryRequestStatus::Ok && result.changed);
    assert(result.batch.transaction_id == 4U);
    assert(index.last_transaction_id() == 4U);
    // A durable automatic commit must be validated against its assigned ID,
    // not the request's zero sentinel. Invalid recovery still rejects success.
    assert(request.transaction_id == 0U);
    sd_repository_validate_recovered_commit(result, true, 4U);
    assert(result.status == SdRepositoryRequestStatus::Ok);
    auto mismatched_recovery = result;
    sd_repository_validate_recovered_commit(mismatched_recovery, true, 3U);
    assert(mismatched_recovery.status == SdRepositoryRequestStatus::RecoveryError);
    auto unavailable_recovery = result;
    sd_repository_validate_recovered_commit(unavailable_recovery, false, 4U);
    assert(unavailable_recovery.status == SdRepositoryRequestStatus::RecoveryError);
    auto sentinel_recovery = result;
    sentinel_recovery.batch.transaction_id = 0U;
    sd_repository_validate_recovered_commit(sentinel_recovery, true, 0U);
    assert(sentinel_recovery.status == SdRepositoryRequestStatus::RecoveryError);
    auto failed_commit = result;
    failed_commit.status = SdRepositoryRequestStatus::JournalError;
    sd_repository_validate_recovered_commit(failed_commit, false, 0U);
    assert(failed_commit.status == SdRepositoryRequestStatus::JournalError);

    SdRepositoryRequestContext stale = ready;
    stale.mount_generation = 5U;
    result = sd_repository_execute_request(read_request, stale, paths, index);
    assert(result.status == SdRepositoryRequestStatus::StaleGeneration);
    assert(sd_repository_make_commit_request(
        4U, kDataset, mutation, 99U, request));
    result = sd_repository_execute_request(request, stale, paths, index);
    assert(result.status == SdRepositoryRequestStatus::StaleGeneration);
    SdRepositoryRequestContext unavailable = ready;
    unavailable.recovery_view_ready = false;
    result = sd_repository_execute_request(
        read_request, unavailable, paths, index);
    assert(result.status == SdRepositoryRequestStatus::RecoveryUnavailable);

    // A failed journal stage may leave a complete orphan, but it must not
    // become visible in the recovery index.
    SdRepositoryRecoveryIndex orphan_index;
    const RepositoryKey orphan_key = key(10U);
    assert(repository_mutation_make_upsert(
        orphan_key, 0U, first.data(), first.size(), mutation));
    assert(sd_repository_make_commit_request(
        4U, kDataset, mutation, 1U, request));
    char wrong_journal[kMaxPathBytes] = {};
    make_path(wrong_journal, sizeof(wrong_journal), application, "WRONG");
    const SdRepositoryRequestPaths bad_paths = {
        application, records, {application, wrong_journal}};
    result = sd_repository_execute_request(
        request, ready, bad_paths, orphan_index);
    assert(result.status == SdRepositoryRequestStatus::JournalError);
    SdRecoveredRecord recovered = {};
    assert(orphan_index.read(orphan_key, recovered) ==
           SdRepositoryRecoveryStatus::NotFound);

    char second_name[kSdJournalObjectNameBytes + 1U] = {};
    assert(sd_repository_object_name(second.data(), second.size(), second_name));
    char second_path[kMaxPathBytes] = {};
    make_path(second_path, sizeof(second_path), records, second_name);
    assert(::unlink(first_path) == 0);
    assert(::unlink(second_path) == 0);
    assert(::unlink(segment) == 0);
    assert(::rmdir(records) == 0);
    assert(::rmdir(journal) == 0);
    assert(::unlink(slot_a) == 0);
    assert(::rmdir(manifests) == 0);
    assert(::rmdir(application) == 0);
    assert(::rmdir(root) == 0);
    return 0;
}
