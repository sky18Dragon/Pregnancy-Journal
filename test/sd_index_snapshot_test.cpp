#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>

#include <unistd.h>

#include "sd_index_power_fixture.h"
#include "sd_index_snapshot.h"
#include "sd_journal_power_fixture.h"

namespace {

struct FakeFile {
    sticky_storage::SdIndexFileProbeStatus before =
        sticky_storage::SdIndexFileProbeStatus::Missing;
    sticky_storage::SdIndexFileProbeStatus after =
        sticky_storage::SdIndexFileProbeStatus::Match;
    sticky_storage::SdIndexWriteIoResult write =
        sticky_storage::SdIndexWriteIoResult::Ok;
    bool directories = true;
    unsigned probes = 0U;
    unsigned writes = 0U;
};

bool ensure_directory(void *context, const char *)
{
    return static_cast<FakeFile *>(context)->directories;
}

sticky_storage::SdIndexWriteIoResult write_file(
    void *context, const char *, const uint8_t *, size_t)
{
    auto &fake = *static_cast<FakeFile *>(context);
    ++fake.writes;
    return fake.write;
}

sticky_storage::SdIndexFileProbe probe_file(
    void *context, const char *, const uint8_t *, size_t size)
{
    auto &fake = *static_cast<FakeFile *>(context);
    sticky_storage::SdIndexFileProbe probe = {};
    probe.status = fake.probes++ == 0U ? fake.before : fake.after;
    probe.file_bytes = probe.status ==
                               sticky_storage::SdIndexFileProbeStatus::Match
                           ? size
                           : 0U;
    return probe;
}

}  // namespace

int main()
{
    using namespace sticky_storage;
    constexpr char kDataset[] = "0123456789abcdef0123456789abcdef";
    std::array<uint8_t, kSdIndexSnapshotMaxBytes> bytes = {};
    const size_t size = sd_index_power_fixture_bytes(kDataset, 36U, bytes);
    static_assert(kIndexPowerFixtureBytes == 2960U);
    static_assert(kIndexPowerPartialFixtureBytes == 1480U);
    assert(size == kIndexPowerFixtureBytes);
    assert(size == kSdIndexSnapshotHeaderBytes +
                       kIndexPowerFixtureEntries *
                           kSdIndexSnapshotEntryBytes);
    assert(sd_index_snapshot_validate(bytes.data(), size) ==
           SdIndexSnapshotStatus::Ok);

    SdIndexSnapshot decoded = {};
    assert(sd_index_snapshot_decode(bytes.data(), size, decoded) ==
           SdIndexSnapshotStatus::Ok);
    assert(std::strcmp(decoded.dataset_id, kDataset) == 0);
    assert(decoded.last_transaction_id == 36U);
    assert(decoded.entry_count == kIndexPowerFixtureEntries);
    assert(decoded.entries[0].key.repository == RepositoryKind::Preference);
    assert(decoded.entries[0].object_bytes ==
           kJournalPowerFixtureObjectBytes);
    assert(std::strcmp(decoded.entries[0].object_file, "PWRTEST.BIN") == 0);

    auto corrupt = bytes;
    corrupt[kSdIndexSnapshotHeaderBytes + 3U] ^= 0x80U;
    assert(sd_index_snapshot_validate(corrupt.data(), size) ==
           SdIndexSnapshotStatus::Corrupt);
    assert(sd_index_snapshot_decode(corrupt.data(), size, decoded) ==
           SdIndexSnapshotStatus::Corrupt);
    assert(sd_index_snapshot_decode(bytes.data(), size - 1U, decoded) ==
           SdIndexSnapshotStatus::Incomplete);
    assert(sd_index_snapshot_validate(bytes.data(), size - 1U) ==
           SdIndexSnapshotStatus::Incomplete);
    assert(sd_index_power_fixture_bytes("bad", 36U, bytes) == 0U);
    assert(sd_index_power_fixture_bytes(kDataset, 0U, bytes) == 0U);
    const size_t restored_size =
        sd_index_power_fixture_bytes(kDataset, 36U, bytes);
    assert(restored_size == size);

    constexpr char kApplication[] = "/card/PREGJNL";
    constexpr char kIndex[] = "/card/PREGJNL/INDEX";
    constexpr char kFile[] = "/card/PREGJNL/INDEX/IDXPWR01.BIN";
    const SdIndexFilePaths paths = {kApplication, kIndex, kFile};
    FakeFile fake = {};
    const SdIndexFileOps ops = {
        &fake, ensure_directory, write_file, probe_file};
    SdIndexWriteResult result = sd_index_file_write_with_ops(
        paths, bytes.data(), size, ops);
    assert(result.status == SdIndexWriteStatus::Ok && result.changed);
    assert(fake.writes == 1U && fake.probes == 2U);

    fake = {};
    fake.before = SdIndexFileProbeStatus::Match;
    result = sd_index_file_write_with_ops(paths, bytes.data(), size, ops);
    assert(result.status == SdIndexWriteStatus::Ok && !result.changed);
    assert(fake.writes == 0U);

    fake = {};
    fake.before = SdIndexFileProbeStatus::Mismatch;
    result = sd_index_file_write_with_ops(paths, bytes.data(), size, ops);
    assert(result.status == SdIndexWriteStatus::ExistingConflict);
    assert(fake.writes == 0U);

    fake = {};
    fake.write = SdIndexWriteIoResult::ShortWrite;
    result = sd_index_file_write_with_ops(paths, bytes.data(), size, ops);
    assert(result.status == SdIndexWriteStatus::ShortWrite);

    fake = {};
    fake.after = SdIndexFileProbeStatus::Mismatch;
    result = sd_index_file_write_with_ops(paths, bytes.data(), size, ops);
    assert(result.status == SdIndexWriteStatus::VerifyError);

    fake = {};
    fake.directories = false;
    result = sd_index_file_write_with_ops(paths, bytes.data(), size, ops);
    assert(result.status == SdIndexWriteStatus::DirectoryError);

    const SdIndexFilePaths bad_paths = {
        kApplication, kIndex, "/card/PREGJNL/RECORDS/IDXPWR01.BIN"};
    assert(sd_index_file_write_with_ops(
               bad_paths, bytes.data(), size, ops).status ==
           SdIndexWriteStatus::InvalidArgument);

    char temporary[] = "/tmp/sticky_index_snapshot_XXXXXX";
    char *root = ::mkdtemp(temporary);
    assert(root != nullptr);
    char application[192U] = {};
    char index_directory[192U] = {};
    char index_file[192U] = {};
    assert(std::snprintf(application, sizeof(application), "%s/PREGJNL",
                         root) > 0);
    assert(std::snprintf(index_directory, sizeof(index_directory), "%s/INDEX",
                         application) > 0);
    assert(std::snprintf(index_file, sizeof(index_file), "%s/IDXPWR01.BIN",
                         index_directory) > 0);
    const SdIndexFilePaths real_paths = {
        application, index_directory, index_file};
    result = sd_index_file_write(real_paths, bytes.data(), size);
    assert(result.status == SdIndexWriteStatus::Ok && result.changed);
    assert(result.probe.file_bytes == size);
    result = sd_index_file_write(real_paths, bytes.data(), size);
    assert(result.status == SdIndexWriteStatus::Ok && !result.changed);

    std::FILE *file = std::fopen(index_file, "wb");
    assert(file != nullptr);
    assert(std::fwrite(bytes.data(), 1U, 117U, file) == 117U);
    assert(std::fclose(file) == 0);
    const SdIndexFileProbe partial =
        sd_index_file_probe(index_file, bytes.data(), size);
    assert(partial.status == SdIndexFileProbeStatus::Mismatch);
    result = sd_index_file_write(real_paths, bytes.data(), size);
    assert(result.status == SdIndexWriteStatus::ExistingConflict);

    assert(::unlink(index_file) == 0);
    assert(::rmdir(index_directory) == 0);
    assert(::rmdir(application) == 0);
    assert(::rmdir(root) == 0);
    // Expanded maximum preserves the schema and round-trips tombstones.
    SdIndexSnapshot maximum = {};
    std::strcpy(maximum.dataset_id, kDataset);
    maximum.last_transaction_id = 2000U;
    maximum.entry_count = kSdIndexSnapshotEntryCapacity;
    for (size_t item = 0U; item < maximum.entry_count; ++item) {
        auto &entry = maximum.entries[item];
        char id[33] = {};
        std::snprintf(id, sizeof(id), "%032llx", static_cast<unsigned long long>(item + 1U));
        assert(repository_key_make(RepositoryKind::Kicks, id, entry.key));
        entry.revision = 2U;
        entry.tombstone = item % 2U == 0U;
        if (!entry.tombstone) {
            entry.object_bytes = 32U;
            std::strcpy(entry.object_file, "LIVE.BIN");
        }
    }
    const auto maximum_size = sd_index_snapshot_encode(maximum, bytes.data(), bytes.size());
    assert(maximum_size == kSdIndexSnapshotMaxBytes);
    assert(sd_index_snapshot_validate(bytes.data(), maximum_size) == SdIndexSnapshotStatus::Ok);
    assert(sd_index_snapshot_decode(bytes.data(), maximum_size, decoded) == SdIndexSnapshotStatus::Ok);
    assert(decoded.entry_count == 1024U);
    assert(decoded.entries[1023].revision == 2U && !decoded.entries[1023].tombstone);
    assert(decoded.entries[1022].tombstone);
    maximum.entry_count++;
    assert(sd_index_snapshot_encode(maximum, bytes.data(), bytes.size()) == 0U);

    // Invalid input must discard the entire previous snapshot, including its
    // tail entries, so a later recovery cannot observe stale records.
    assert(sd_index_snapshot_decode(nullptr, 0U, decoded) ==
           SdIndexSnapshotStatus::InvalidArgument);
    assert(decoded.dataset_id[0] == '\0');
    assert(decoded.last_transaction_id == 0U && decoded.entry_count == 0U);
    for (const auto &entry : decoded.entries) {
        assert(entry.key.record_id[0] == '\0');
        assert(entry.revision == 0U && !entry.tombstone);
        assert(entry.object_bytes == 0U && entry.object_crc32 == 0U);
        assert(entry.object_file[0] == '\0');
    }
    return 0;
}
