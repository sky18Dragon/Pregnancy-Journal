#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <sys/stat.h>
#include <unistd.h>

#include "sd_journal_store.h"

namespace {
constexpr char kDataset[] = "0123456789abcdef0123456789abcdef";

void make_path(char *output, size_t capacity, const char *left,
               const char *right)
{
    const int written = std::snprintf(output, capacity, "%s/%s", left, right);
    assert(written > 0 && static_cast<size_t>(written) < capacity);
}

void write_full_segment(const char *path, uint64_t first_transaction,
                        uint32_t records)
{
    std::FILE *file = std::fopen(path, "wb");
    assert(file != nullptr);
    std::array<uint8_t, sticky_storage::kSdJournalMaxPayloadBytes> payload =
        {};
    std::array<uint8_t, sticky_storage::kSdJournalMaxRecordBytes> frame = {};
    for (uint32_t index = 0U; index < records; ++index) {
        const size_t size = sticky_storage::sd_journal_encode_record(
            sticky_storage::SdJournalRecordType::Commit, kDataset,
            first_transaction + index, payload.data(), payload.size(),
            frame.data(), frame.size());
        assert(size == frame.size());
        assert(std::fwrite(frame.data(), 1U, size, file) == size);
    }
    assert(std::fflush(file) == 0);
    assert(::fsync(fileno(file)) == 0);
    assert(std::fclose(file) == 0);
}
}

int main()
{
    using namespace sticky_storage;
    char temp_template[] = "/tmp/sticky_store_XXXXXX";
    char *temp = ::mkdtemp(temp_template);
    assert(temp != nullptr);
    char root[kMaxPathBytes] = {};
    char journal[kMaxPathBytes] = {};
    char segment1[kMaxPathBytes] = {};
    char segment2[kMaxPathBytes] = {};
    char segment3[kMaxPathBytes] = {};
    char unexpected[kMaxPathBytes] = {};
    make_path(root, sizeof(root), temp, "PREGJNL");
    make_path(journal, sizeof(journal), root, "JOURNAL");
    make_path(segment1, sizeof(segment1), journal, "TXN00001.LOG");
    make_path(segment2, sizeof(segment2), journal, "TXN00002.LOG");
    make_path(segment3, sizeof(segment3), journal, "TXN00003.LOG");
    make_path(unexpected, sizeof(unexpected), journal, "OTHER.BIN");
    const SdJournalStorePaths paths = {root, journal};

    SdJournalStoreProbe probe = sd_journal_store_probe(paths, kDataset);
    assert(probe.status == SdJournalStoreStatus::Missing);
    constexpr std::array<uint8_t, 3U> payload = {1U, 2U, 3U};
    SdJournalStoreAppendResult append = sd_journal_store_append(
        paths, kDataset, SdJournalRecordType::Commit, 1U, payload.data(),
        payload.size());
    assert(append.status == SdJournalStoreStatus::Ready);
    assert(append.changed && append.probe.segment_count == 1U);
    assert(append.probe.active_segment == 1U);
    assert(append.probe.total_record_count == 1U);

    append = sd_journal_store_append(
        paths, kDataset, SdJournalRecordType::Commit, 1U, payload.data(),
        payload.size());
    assert(append.status == SdJournalStoreStatus::Ready);
    assert(!append.changed);
    constexpr std::array<uint8_t, 2U> different = {9U, 9U};
    append = sd_journal_store_append(
        paths, kDataset, SdJournalRecordType::Commit, 1U,
        different.data(), different.size());
    assert(append.status == SdJournalStoreStatus::TransactionConflict);

    // 239 maximum frames leave only 200 bytes in a 256 KiB segment. The next
    // maximum frame must start TXN00002.LOG.
    write_full_segment(segment1, 1U, 239U);
    append = sd_journal_store_append(
        paths, kDataset, SdJournalRecordType::Commit, 240U,
        std::array<uint8_t, kSdJournalMaxPayloadBytes>{}.data(),
        kSdJournalMaxPayloadBytes);
    assert(append.status == SdJournalStoreStatus::Ready);
    assert(append.changed);
    assert(append.probe.segment_count == 2U);
    assert(append.probe.active_segment == 2U);
    assert(append.probe.total_record_count == 240U);
    assert(append.probe.active.first_transaction_id == 240U);

    assert(std::rename(segment2, segment3) == 0);
    probe = sd_journal_store_probe(paths, kDataset);
    assert(probe.status == SdJournalStoreStatus::SegmentGap);
    assert(std::rename(segment3, segment2) == 0);

    std::FILE *extra = std::fopen(unexpected, "wb");
    assert(extra != nullptr && std::fclose(extra) == 0);
    probe = sd_journal_store_probe(paths, kDataset);
    assert(probe.status == SdJournalStoreStatus::UnexpectedEntry);
    assert(std::remove(unexpected) == 0);

    // Keep one complete record in the final segment and truncate the next.
    // Store-level totals must retain both the earlier segments and this valid
    // active-segment prefix while still reporting TailTruncated.
    write_full_segment(segment2, 240U, 2U);
    assert(::truncate(
               segment2,
               static_cast<off_t>(kSdJournalMaxRecordBytes +
                                  kSdJournalMaxRecordBytes / 2U)) == 0);
    probe = sd_journal_store_probe(paths, kDataset);
    assert(probe.status == SdJournalStoreStatus::TailTruncated);
    assert(probe.total_record_count == 240U);
    assert(probe.active.record_count == 1U);
    assert(probe.active.last.transaction_id == 240U);
    assert(probe.total_bytes ==
           239ULL * kSdJournalMaxRecordBytes +
               kSdJournalMaxRecordBytes + kSdJournalMaxRecordBytes / 2U);
    append = sd_journal_store_append(
        paths, kDataset, SdJournalRecordType::Commit, 241U, payload.data(),
        payload.size());
    assert(append.status == SdJournalStoreStatus::TailTruncated);

    assert(std::remove(segment2) == 0);
    assert(std::remove(segment1) == 0);
    assert(::rmdir(journal) == 0);
    assert(::rmdir(root) == 0);
    assert(::rmdir(temp) == 0);
    return 0;
}
