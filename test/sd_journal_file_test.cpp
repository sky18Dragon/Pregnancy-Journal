#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <sys/stat.h>
#include <unistd.h>

#include "sd_journal_file.h"

namespace {

constexpr char kDatasetA[] = "0123456789abcdef0123456789abcdef";
constexpr char kDatasetB[] = "fedcba9876543210fedcba9876543210";
constexpr sticky_storage::SdJournalFilePaths kFakePaths = {
    "/card/PREGJNL",
    "/card/PREGJNL/JOURNAL",
    "/card/PREGJNL/JOURNAL/TXN00001.LOG",
};

struct FakeJournal {
    std::array<uint8_t, sticky_storage::kSdJournalMaxRecordBytes * 6U> bytes =
        {};
    size_t size = 0U;
    bool missing = true;
    bool directory_ok = true;
    bool corrupt_after_append = false;
    uint32_t ensure_calls = 0U;
    uint32_t append_calls = 0U;
    uint32_t probe_calls = 0U;
    uint32_t io_on_probe_call = 0U;
    sticky_storage::SdJournalAppendIoResult append_result =
        sticky_storage::SdJournalAppendIoResult::Ok;
};

struct LastRecordCapture {
    sticky_storage::SdJournalRecordSummary last = {};
};

bool capture_last(void *context,
                  const sticky_storage::SdJournalRecordView &record)
{
    auto &capture = *static_cast<LastRecordCapture *>(context);
    capture.last.type = record.type;
    capture.last.transaction_id = record.transaction_id;
    capture.last.encoded_size = record.encoded_size;
    capture.last.payload_size = record.payload_size;
    capture.last.payload_crc32 = record.payload_crc32;
    capture.last.frame_crc32 = record.frame_crc32;
    return true;
}

sticky_storage::SdJournalFileProbeStatus map_scan(
    sticky_storage::SdJournalScanStatus status)
{
    using sticky_storage::SdJournalFileProbeStatus;
    using sticky_storage::SdJournalScanStatus;
    switch (status) {
    case SdJournalScanStatus::Ok:
        return SdJournalFileProbeStatus::Ready;
    case SdJournalScanStatus::TailTruncated:
        return SdJournalFileProbeStatus::TailTruncated;
    case SdJournalScanStatus::Corrupt:
        return SdJournalFileProbeStatus::Corrupt;
    case SdJournalScanStatus::UnsupportedSchema:
        return SdJournalFileProbeStatus::UnsupportedSchema;
    case SdJournalScanStatus::UnsupportedRecordType:
        return SdJournalFileProbeStatus::UnsupportedRecordType;
    case SdJournalScanStatus::DatasetMismatch:
        return SdJournalFileProbeStatus::DatasetMismatch;
    case SdJournalScanStatus::NonMonotonicTransaction:
        return SdJournalFileProbeStatus::NonMonotonicTransaction;
    case SdJournalScanStatus::ConsumerRejected:
    case SdJournalScanStatus::InvalidArgument:
        return SdJournalFileProbeStatus::InvalidArgument;
    }
    return SdJournalFileProbeStatus::Corrupt;
}

bool fake_ensure_directory(void *context, const char *)
{
    auto &fake = *static_cast<FakeJournal *>(context);
    ++fake.ensure_calls;
    return fake.directory_ok;
}

sticky_storage::SdJournalAppendIoResult fake_append_record(
    void *context, const char *, const uint8_t *bytes, size_t size)
{
    auto &fake = *static_cast<FakeJournal *>(context);
    ++fake.append_calls;
    if (fake.append_result ==
        sticky_storage::SdJournalAppendIoResult::OpenError) {
        return fake.append_result;
    }

    size_t write_size = size;
    if (fake.append_result ==
        sticky_storage::SdJournalAppendIoResult::ShortWrite) {
        write_size /= 2U;
    }
    assert(fake.size + write_size <= fake.bytes.size());
    std::memcpy(fake.bytes.data() + fake.size, bytes, write_size);
    fake.size += write_size;
    fake.missing = false;
    if (fake.corrupt_after_append && write_size != 0U) {
        fake.bytes[fake.size - 1U] ^= 0x80U;
    }
    return fake.append_result;
}

sticky_storage::SdJournalFileProbe fake_probe_segment(
    void *context, const char *, const char *expected_dataset_id)
{
    auto &fake = *static_cast<FakeJournal *>(context);
    ++fake.probe_calls;
    sticky_storage::SdJournalFileProbe probe = {};
    if (fake.io_on_probe_call == fake.probe_calls) {
        probe.status = sticky_storage::SdJournalFileProbeStatus::IoError;
        return probe;
    }
    if (fake.missing) {
        probe.status = sticky_storage::SdJournalFileProbeStatus::Missing;
        return probe;
    }
    probe.file_bytes = fake.size;
    if (fake.size == 0U) {
        probe.status = sticky_storage::SdJournalFileProbeStatus::Empty;
        return probe;
    }

    LastRecordCapture capture = {};
    const sticky_storage::SdJournalScanResult scan =
        sticky_storage::sd_journal_scan(expected_dataset_id,
                                        fake.bytes.data(), fake.size,
                                        capture_last, &capture);
    probe.status = map_scan(scan.status);
    probe.valid_bytes = scan.valid_bytes;
    probe.record_count = scan.record_count;
    probe.last = capture.last;
    return probe;
}

sticky_storage::SdJournalFileOps fake_ops(FakeJournal &fake)
{
    return {
        &fake,
        fake_ensure_directory,
        fake_append_record,
        fake_probe_segment,
    };
}

void seed_record(FakeJournal &fake, const char *dataset_id,
                 uint64_t transaction_id, const uint8_t *payload,
                 size_t payload_size)
{
    const size_t written = sticky_storage::sd_journal_encode_record(
        sticky_storage::SdJournalRecordType::Commit, dataset_id,
        transaction_id, payload, payload_size,
        fake.bytes.data() + fake.size, fake.bytes.size() - fake.size);
    assert(written != 0U);
    fake.size += written;
    fake.missing = false;
}

}  // namespace

int main()
{
    using namespace sticky_storage;
    constexpr std::array<uint8_t, 3U> payload = {1U, 2U, 3U};
    constexpr std::array<uint8_t, 3U> changed_payload = {1U, 2U, 4U};

    FakeJournal fake = {};
    SdJournalAppendResult append = sd_journal_append_file_with_ops(
        kFakePaths, kDatasetA, SdJournalRecordType::Commit, 1U,
        payload.data(), payload.size(), fake_ops(fake));
    assert(append.status == SdJournalAppendStatus::Ok);
    assert(append.changed);
    assert(append.probe.status == SdJournalFileProbeStatus::Ready);
    assert(append.probe.record_count == 1U);
    assert(append.probe.last.transaction_id == 1U);
    assert(fake.ensure_calls == 2U);
    assert(fake.append_calls == 1U);
    const size_t first_file_size = fake.size;

    append = sd_journal_append_file_with_ops(
        kFakePaths, kDatasetA, SdJournalRecordType::Commit, 1U,
        payload.data(), payload.size(), fake_ops(fake));
    assert(append.status == SdJournalAppendStatus::Ok);
    assert(!append.changed);
    assert(fake.append_calls == 1U);
    assert(fake.size == first_file_size);

    append = sd_journal_append_file_with_ops(
        kFakePaths, kDatasetA, SdJournalRecordType::Commit, 1U,
        changed_payload.data(), changed_payload.size(), fake_ops(fake));
    assert(append.status == SdJournalAppendStatus::TransactionConflict);
    assert(fake.append_calls == 1U);

    append = sd_journal_append_file_with_ops(
        kFakePaths, kDatasetA, SdJournalRecordType::Commit, 3U,
        payload.data(), payload.size(), fake_ops(fake));
    assert(append.status == SdJournalAppendStatus::Ok);
    assert(append.changed);
    assert(append.probe.record_count == 2U);
    assert(append.probe.last.transaction_id == 3U);

    append = sd_journal_append_file_with_ops(
        kFakePaths, kDatasetA, SdJournalRecordType::Commit, 2U,
        payload.data(), payload.size(), fake_ops(fake));
    assert(append.status ==
           SdJournalAppendStatus::NonMonotonicTransaction);

    FakeJournal empty = {};
    empty.missing = false;
    append = sd_journal_append_file_with_ops(
        kFakePaths, kDatasetA, SdJournalRecordType::Tombstone, 5U, nullptr,
        0U, fake_ops(empty));
    assert(append.status == SdJournalAppendStatus::Ok);
    assert(append.changed);

    FakeJournal wrong_dataset = {};
    seed_record(wrong_dataset, kDatasetA, 1U, payload.data(), payload.size());
    append = sd_journal_append_file_with_ops(
        kFakePaths, kDatasetB, SdJournalRecordType::Commit, 2U,
        payload.data(), payload.size(), fake_ops(wrong_dataset));
    assert(append.status == SdJournalAppendStatus::DatasetMismatch);
    assert(wrong_dataset.append_calls == 0U);

    FakeJournal truncated = {};
    seed_record(truncated, kDatasetA, 1U, payload.data(), payload.size());
    ++truncated.size;
    truncated.bytes[truncated.size - 1U] = 'P';
    append = sd_journal_append_file_with_ops(
        kFakePaths, kDatasetA, SdJournalRecordType::Commit, 2U,
        payload.data(), payload.size(), fake_ops(truncated));
    assert(append.status == SdJournalAppendStatus::TailTruncated);
    assert(truncated.append_calls == 0U);

    FakeJournal corrupt = {};
    seed_record(corrupt, kDatasetA, 1U, payload.data(), payload.size());
    corrupt.bytes[0] = 'X';
    append = sd_journal_append_file_with_ops(
        kFakePaths, kDatasetA, SdJournalRecordType::Commit, 2U,
        payload.data(), payload.size(), fake_ops(corrupt));
    assert(append.status == SdJournalAppendStatus::Corrupt);

    FakeJournal unsupported = {};
    seed_record(unsupported, kDatasetA, 1U, payload.data(), payload.size());
    unsupported.bytes[4] = 2U;
    append = sd_journal_append_file_with_ops(
        kFakePaths, kDatasetA, SdJournalRecordType::Commit, 2U,
        payload.data(), payload.size(), fake_ops(unsupported));
    assert(append.status == SdJournalAppendStatus::UnsupportedSchema);

    FakeJournal directory_failure = {};
    directory_failure.directory_ok = false;
    append = sd_journal_append_file_with_ops(
        kFakePaths, kDatasetA, SdJournalRecordType::Commit, 1U,
        payload.data(), payload.size(), fake_ops(directory_failure));
    assert(append.status == SdJournalAppendStatus::DirectoryError);
    assert(directory_failure.append_calls == 0U);

    FakeJournal open_failure = {};
    open_failure.append_result = SdJournalAppendIoResult::OpenError;
    append = sd_journal_append_file_with_ops(
        kFakePaths, kDatasetA, SdJournalRecordType::Commit, 1U,
        payload.data(), payload.size(), fake_ops(open_failure));
    assert(append.status == SdJournalAppendStatus::OpenError);
    assert(open_failure.size == 0U);

    FakeJournal short_write = {};
    short_write.append_result = SdJournalAppendIoResult::ShortWrite;
    append = sd_journal_append_file_with_ops(
        kFakePaths, kDatasetA, SdJournalRecordType::Commit, 1U,
        payload.data(), payload.size(), fake_ops(short_write));
    assert(append.status == SdJournalAppendStatus::ShortWrite);
    assert(short_write.size != 0U);
    assert(short_write.size < kSdJournalHeaderBytes + payload.size());
    short_write.append_result = SdJournalAppendIoResult::Ok;
    append = sd_journal_append_file_with_ops(
        kFakePaths, kDatasetA, SdJournalRecordType::Commit, 1U,
        payload.data(), payload.size(), fake_ops(short_write));
    assert(append.status == SdJournalAppendStatus::TailTruncated);
    assert(short_write.append_calls == 1U);

    FakeJournal sync_failure = {};
    sync_failure.append_result = SdJournalAppendIoResult::SyncError;
    append = sd_journal_append_file_with_ops(
        kFakePaths, kDatasetA, SdJournalRecordType::Commit, 1U,
        payload.data(), payload.size(), fake_ops(sync_failure));
    assert(append.status == SdJournalAppendStatus::SyncError);
    sync_failure.append_result = SdJournalAppendIoResult::Ok;
    append = sd_journal_append_file_with_ops(
        kFakePaths, kDatasetA, SdJournalRecordType::Commit, 1U,
        payload.data(), payload.size(), fake_ops(sync_failure));
    assert(append.status == SdJournalAppendStatus::Ok);
    assert(!append.changed);
    assert(sync_failure.append_calls == 1U);

    FakeJournal close_failure = {};
    close_failure.append_result = SdJournalAppendIoResult::CloseError;
    append = sd_journal_append_file_with_ops(
        kFakePaths, kDatasetA, SdJournalRecordType::Commit, 1U,
        payload.data(), payload.size(), fake_ops(close_failure));
    assert(append.status == SdJournalAppendStatus::CloseError);
    close_failure.append_result = SdJournalAppendIoResult::Ok;
    append = sd_journal_append_file_with_ops(
        kFakePaths, kDatasetA, SdJournalRecordType::Commit, 1U,
        payload.data(), payload.size(), fake_ops(close_failure));
    assert(append.status == SdJournalAppendStatus::Ok);
    assert(!append.changed);
    assert(close_failure.append_calls == 1U);

    FakeJournal verify_failure = {};
    verify_failure.corrupt_after_append = true;
    append = sd_journal_append_file_with_ops(
        kFakePaths, kDatasetA, SdJournalRecordType::Commit, 1U,
        payload.data(), payload.size(), fake_ops(verify_failure));
    assert(append.status == SdJournalAppendStatus::VerifyError);
    assert(!append.changed);

    FakeJournal probe_failure = {};
    probe_failure.io_on_probe_call = 2U;
    append = sd_journal_append_file_with_ops(
        kFakePaths, kDatasetA, SdJournalRecordType::Commit, 1U,
        payload.data(), payload.size(), fake_ops(probe_failure));
    assert(append.status == SdJournalAppendStatus::ProbeIoError);

    append = sd_journal_append_file_with_ops(
        {}, kDatasetA, SdJournalRecordType::Commit, 1U, payload.data(),
        payload.size(), fake_ops(fake));
    assert(append.status == SdJournalAppendStatus::InvalidArgument);
    append = sd_journal_append_file_with_ops(
        kFakePaths, "invalid", SdJournalRecordType::Commit, 1U,
        payload.data(), payload.size(), fake_ops(fake));
    assert(append.status == SdJournalAppendStatus::InvalidArgument);

    char temp_template[] = "/tmp/sticky_journal_XXXXXX";
    char *temp_root = ::mkdtemp(temp_template);
    assert(temp_root != nullptr);
    std::array<char, 256U> application_root = {};
    std::array<char, 256U> journal_directory = {};
    std::array<char, 256U> segment = {};
    assert(std::snprintf(application_root.data(), application_root.size(),
                         "%s/PREGJNL", temp_root) > 0);
    assert(std::snprintf(journal_directory.data(), journal_directory.size(),
                         "%s/PREGJNL/JOURNAL", temp_root) > 0);
    assert(std::snprintf(segment.data(), segment.size(),
                         "%s/PREGJNL/JOURNAL/TXN00001.LOG", temp_root) > 0);
    const SdJournalFilePaths real_paths = {
        application_root.data(), journal_directory.data(), segment.data()};

    SdJournalFileProbe real_probe =
        sd_journal_probe_file(segment.data(), kDatasetA);
    assert(real_probe.status == SdJournalFileProbeStatus::Missing);
    append = sd_journal_append_file(
        real_paths, kDatasetA, SdJournalRecordType::Commit, 1U,
        payload.data(), payload.size());
    assert(append.status == SdJournalAppendStatus::Ok);
    assert(append.changed);
    append = sd_journal_append_file(
        real_paths, kDatasetA, SdJournalRecordType::Checkpoint, 2U, nullptr,
        0U);
    assert(append.status == SdJournalAppendStatus::Ok);
    assert(append.probe.record_count == 2U);
    assert(append.probe.last.transaction_id == 2U);
    const uint64_t complete_bytes = append.probe.file_bytes;

    append = sd_journal_append_file(
        real_paths, kDatasetA, SdJournalRecordType::Checkpoint, 2U, nullptr,
        0U);
    assert(append.status == SdJournalAppendStatus::Ok);
    assert(!append.changed);
    assert(append.probe.file_bytes == complete_bytes);

    assert(::truncate(segment.data(),
                      static_cast<off_t>(complete_bytes - 1U)) == 0);
    real_probe = sd_journal_probe_file(segment.data(), kDatasetA);
    assert(real_probe.status == SdJournalFileProbeStatus::TailTruncated);
    assert(real_probe.record_count == 1U);
    append = sd_journal_append_file(
        real_paths, kDatasetA, SdJournalRecordType::Commit, 3U,
        payload.data(), payload.size());
    assert(append.status == SdJournalAppendStatus::TailTruncated);

    assert(std::remove(segment.data()) == 0);
    assert(::rmdir(journal_directory.data()) == 0);
    assert(::rmdir(application_root.data()) == 0);
    assert(::rmdir(temp_root) == 0);
    return 0;
}
