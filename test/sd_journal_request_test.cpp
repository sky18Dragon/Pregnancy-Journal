#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "sd_journal_payload.h"
#include "sd_journal_request.h"

namespace {

constexpr char kDatasetA[] = "0123456789abcdef0123456789abcdef";
constexpr char kDatasetB[] = "fedcba9876543210fedcba9876543210";
constexpr sticky_storage::SdJournalFilePaths kPaths = {
    "/card/PREGJNL",
    "/card/PREGJNL/JOURNAL",
    "/card/PREGJNL/JOURNAL/TXN00001.LOG",
};

struct FakeFile {
    bool missing = true;
    bool directory_ok = true;
    uint32_t ensure_calls = 0U;
    uint32_t append_calls = 0U;
    uint64_t file_bytes = 0U;
    uint32_t record_count = 0U;
    sticky_storage::SdJournalRecordSummary last = {};
};

bool ensure_directory(void *context, const char *)
{
    auto &fake = *static_cast<FakeFile *>(context);
    ++fake.ensure_calls;
    return fake.directory_ok;
}

sticky_storage::SdJournalAppendIoResult append_record(
    void *context, const char *, const uint8_t *bytes, size_t size)
{
    auto &fake = *static_cast<FakeFile *>(context);
    sticky_storage::SdJournalRecordView record = {};
    assert(sticky_storage::sd_journal_decode_record(
               kDatasetA, bytes, size, record) ==
           sticky_storage::SdJournalDecodeStatus::Ok);
    ++fake.append_calls;
    fake.file_bytes += size;
    ++fake.record_count;
    fake.last.type = record.type;
    fake.last.transaction_id = record.transaction_id;
    fake.last.encoded_size = record.encoded_size;
    fake.last.payload_size = record.payload_size;
    fake.last.payload_crc32 = record.payload_crc32;
    fake.last.frame_crc32 = record.frame_crc32;
    fake.missing = false;
    return sticky_storage::SdJournalAppendIoResult::Ok;
}

sticky_storage::SdJournalFileProbe probe_segment(void *context, const char *,
                                                 const char *)
{
    auto &fake = *static_cast<FakeFile *>(context);
    sticky_storage::SdJournalFileProbe probe = {};
    if (fake.missing) {
        probe.status = sticky_storage::SdJournalFileProbeStatus::Missing;
        return probe;
    }
    probe.status = fake.record_count == 0U
                       ? sticky_storage::SdJournalFileProbeStatus::Empty
                       : sticky_storage::SdJournalFileProbeStatus::Ready;
    probe.file_bytes = fake.file_bytes;
    probe.valid_bytes = fake.file_bytes;
    probe.record_count = fake.record_count;
    probe.last = fake.last;
    return probe;
}

sticky_storage::SdJournalFileOps ops(FakeFile &fake)
{
    return {&fake, ensure_directory, append_record, probe_segment};
}

sticky_storage::SdJournalRequestContext context(
    sticky_storage::SdCardState state =
        sticky_storage::SdCardState::ReadyReadWrite,
    uint32_t generation = 7U, const char *dataset = kDatasetA)
{
    sticky_storage::SdJournalRequestContext value = {};
    value.card_state = state;
    value.mount_generation = generation;
    value.manifest_state = sticky_storage::SdManifestProbeState::Ready;
    value.manifest_selected = true;
    assert(sticky_storage::sd_manifest_make(dataset, 2U, value.manifest));
    return value;
}

}  // namespace

int main()
{
    using namespace sticky_storage;
    SdJournalTransactionPayload transaction = {};
    transaction.mutation_count = 1U;
    assert(repository_key_make(
        RepositoryKind::Preference, "00000000000000000000000000000001",
        transaction.mutations[0].key));
    transaction.mutations[0].type = RepositoryMutationType::Upsert;
    transaction.mutations[0].expected_revision = 0U;
    transaction.mutations[0].committed_revision = 1U;
    transaction.mutations[0].object_bytes = 4U;
    transaction.mutations[0].object_crc32 = 0x12345678U;
    std::strcpy(transaction.mutations[0].object_file, "OBJ00001.BIN");
    std::array<uint8_t, kSdJournalMaxPayloadBytes> payload = {};
    const size_t payload_size = sd_journal_encode_transaction_payload(
        transaction, payload.data(), payload.size());
    assert(payload_size != 0U);

    SdJournalRequest request = {};
    assert(sd_journal_make_append_request(
        7U, kDatasetA, SdJournalRecordType::Commit, 1U, payload.data(),
        payload_size, request));
    assert(request.payload_size == payload_size);
    assert(std::memcmp(request.payload.data(), payload.data(),
                       payload_size) == 0);

    FakeFile fake = {};
    SdJournalRequestResult result = sd_journal_execute_request_with_ops(
        request, context(), kPaths, ops(fake));
    assert(result.status == SdJournalRequestStatus::Ok);
    assert(result.changed);
    assert(result.probe.total_record_count == 1U);
    assert(result.probe.active.last.transaction_id == 1U);
    assert(fake.ensure_calls == 2U && fake.append_calls == 1U);

    result = sd_journal_execute_request_with_ops(
        request, context(), kPaths, ops(fake));
    assert(result.status == SdJournalRequestStatus::Ok);
    assert(!result.changed);
    assert(fake.append_calls == 1U);

    SdJournalRequest probe_request = {};
    assert(sd_journal_make_probe_request(7U, kDatasetA, probe_request));
    result = sd_journal_execute_request_with_ops(
        probe_request, context(SdCardState::ReadyReadOnly), kPaths,
        ops(fake));
    assert(result.status == SdJournalRequestStatus::Ok);
    assert(result.probe.total_record_count == 1U);

    result = sd_journal_execute_request_with_ops(
        request, context(SdCardState::ReadyReadOnly), kPaths, ops(fake));
    assert(result.status == SdJournalRequestStatus::CardReadOnly);
    result = sd_journal_execute_request_with_ops(
        request, context(SdCardState::Full), kPaths, ops(fake));
    assert(result.status == SdJournalRequestStatus::NoSpace);
    result = sd_journal_execute_request_with_ops(
        request, context(SdCardState::Absent), kPaths, ops(fake));
    assert(result.status == SdJournalRequestStatus::CardMissing);
    result = sd_journal_execute_request_with_ops(
        request, context(SdCardState::Fault), kPaths, ops(fake));
    assert(result.status == SdJournalRequestStatus::CardUnavailable);

    result = sd_journal_execute_request_with_ops(
        request, context(SdCardState::ReadyReadWrite, 8U), kPaths,
        ops(fake));
    assert(result.status == SdJournalRequestStatus::StaleGeneration);
    result = sd_journal_execute_request_with_ops(
        request, context(SdCardState::ReadyReadWrite, 7U, kDatasetB),
        kPaths, ops(fake));
    assert(result.status == SdJournalRequestStatus::DatasetMismatch);

    SdJournalRequestContext no_manifest = context();
    no_manifest.manifest_selected = false;
    no_manifest.manifest_state = SdManifestProbeState::NotInitialized;
    result = sd_journal_execute_request_with_ops(
        request, no_manifest, kPaths, ops(fake));
    assert(result.status == SdJournalRequestStatus::ManifestUnavailable);

    assert(!sd_journal_make_probe_request(0U, kDatasetA, probe_request));
    assert(!sd_journal_make_probe_request(7U, "invalid", probe_request));
    assert(!sd_journal_make_append_request(
        7U, kDatasetA, SdJournalRecordType::Commit, 0U, payload.data(),
        payload_size, request));
    assert(!sd_journal_make_append_request(
        7U, kDatasetA, static_cast<SdJournalRecordType>(99U), 1U,
        payload.data(), payload_size, request));
    assert(!sd_journal_make_append_request(
        7U, kDatasetA, SdJournalRecordType::Commit, 1U, nullptr, 1U,
        request));
    constexpr std::array<uint8_t, 4U> invalid_payload = {1U, 2U, 3U, 4U};
    assert(!sd_journal_make_append_request(
        7U, kDatasetA, SdJournalRecordType::Commit, 1U,
        invalid_payload.data(), invalid_payload.size(), request));
    return 0;
}
