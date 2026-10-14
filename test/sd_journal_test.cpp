#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "sd_journal.h"
#include "sd_storage_layout.h"
#include "storage_contract.h"

namespace {

constexpr char kDatasetA[] = "0123456789abcdef0123456789abcdef";
constexpr char kDatasetB[] = "fedcba9876543210fedcba9876543210";

struct VisitorState {
    uint32_t count = 0U;
    uint64_t last_transaction_id = 0U;
    sticky_storage::SdJournalRecordType last_type =
        sticky_storage::SdJournalRecordType::Commit;
    std::array<uint8_t, 8U> last_payload = {};
    size_t last_payload_size = 0U;
    bool reject = false;
};

bool visit(void *context, const sticky_storage::SdJournalRecordView &record)
{
    auto &state = *static_cast<VisitorState *>(context);
    if (state.reject) {
        return false;
    }
    ++state.count;
    state.last_transaction_id = record.transaction_id;
    state.last_type = record.type;
    state.last_payload_size = record.payload_size;
    assert(record.payload_size <= state.last_payload.size());
    if (record.payload_size != 0U) {
        std::memcpy(state.last_payload.data(), record.payload,
                    record.payload_size);
    }
    return true;
}

}  // namespace

int main()
{
    using namespace sticky_storage;

    assert(fat83_name_valid(kJournalDirectory));
    assert(fat83_name_valid(kJournalFirstSegmentFile));
    assert(std::strlen(kJournalFirstSegmentPath) < kMaxPathBytes);

    constexpr std::array<uint8_t, 4U> first_payload = {1U, 2U, 3U, 4U};
    constexpr std::array<uint8_t, 3U> second_payload = {7U, 8U, 9U};
    std::array<uint8_t, kSdJournalMaxRecordBytes * 2U> journal = {};
    const size_t first_size = sd_journal_encode_record(
        SdJournalRecordType::Commit, kDatasetA, 1U, first_payload.data(),
        first_payload.size(), journal.data(), journal.size());
    assert(first_size == kSdJournalHeaderBytes + first_payload.size());
    const size_t second_size = sd_journal_encode_record(
        SdJournalRecordType::Tombstone, kDatasetA, 2U,
        second_payload.data(), second_payload.size(),
        journal.data() + first_size, journal.size() - first_size);
    assert(second_size == kSdJournalHeaderBytes + second_payload.size());

    SdJournalRecordView decoded_record = {};
    assert(sd_journal_decode_record(kDatasetA, journal.data(),
                                    kSdJournalHeaderBytes,
                                    decoded_record) ==
           SdJournalDecodeStatus::Incomplete);
    assert(decoded_record.encoded_size == first_size);
    assert(decoded_record.payload_size == first_payload.size());
    assert(sd_journal_decode_record(kDatasetA, journal.data(), first_size,
                                    decoded_record) ==
           SdJournalDecodeStatus::Ok);
    assert(decoded_record.payload_crc32 != 0U);
    assert(decoded_record.frame_crc32 != 0U);

    VisitorState visitor = {};
    SdJournalScanResult scan = sd_journal_scan(
        kDatasetA, journal.data(), first_size + second_size, visit, &visitor);
    assert(scan.status == SdJournalScanStatus::Ok);
    assert(scan.valid_bytes == first_size + second_size);
    assert(scan.tail_bytes == 0U);
    assert(scan.record_count == 2U);
    assert(scan.last_transaction_id == 2U);
    assert(visitor.count == 2U);
    assert(visitor.last_transaction_id == 2U);
    assert(visitor.last_type == SdJournalRecordType::Tombstone);
    assert(visitor.last_payload_size == second_payload.size());
    assert(std::memcmp(visitor.last_payload.data(), second_payload.data(),
                       second_payload.size()) == 0);

    for (size_t partial = 1U; partial < second_size; ++partial) {
        visitor = {};
        scan = sd_journal_scan(kDatasetA, journal.data(),
                               first_size + partial, visit, &visitor);
        assert(scan.status == SdJournalScanStatus::TailTruncated);
        assert(scan.valid_bytes == first_size);
        assert(scan.tail_bytes == partial);
        assert(scan.record_count == 1U);
        assert(scan.last_transaction_id == 1U);
        assert(visitor.count == 1U);
    }

    std::array<uint8_t, kSdJournalMaxRecordBytes> corrupted = {};
    std::memcpy(corrupted.data(), journal.data(), first_size);
    corrupted[kSdJournalHeaderBytes] ^= 0x80U;
    scan = sd_journal_scan(kDatasetA, corrupted.data(), first_size);
    assert(scan.status == SdJournalScanStatus::Corrupt);
    assert(scan.valid_bytes == 0U);

    std::memcpy(corrupted.data(), journal.data(), first_size);
    corrupted[68U] ^= 0x01U;
    scan = sd_journal_scan(kDatasetA, corrupted.data(), first_size);
    assert(scan.status == SdJournalScanStatus::Corrupt);

    std::memcpy(corrupted.data(), journal.data(), first_size);
    corrupted[4U] = 2U;
    scan = sd_journal_scan(kDatasetA, corrupted.data(), first_size);
    assert(scan.status == SdJournalScanStatus::UnsupportedSchema);

    std::memcpy(corrupted.data(), journal.data(), first_size);
    corrupted[16U] = 99U;
    scan = sd_journal_scan(kDatasetA, corrupted.data(), first_size);
    assert(scan.status == SdJournalScanStatus::UnsupportedRecordType);

    scan = sd_journal_scan(kDatasetB, journal.data(), first_size);
    assert(scan.status == SdJournalScanStatus::DatasetMismatch);

    std::array<uint8_t, kSdJournalMaxRecordBytes * 2U> reversed = {};
    const size_t newer_size = sd_journal_encode_record(
        SdJournalRecordType::Commit, kDatasetA, 2U, nullptr, 0U,
        reversed.data(), reversed.size());
    const size_t older_size = sd_journal_encode_record(
        SdJournalRecordType::Commit, kDatasetA, 1U, nullptr, 0U,
        reversed.data() + newer_size, reversed.size() - newer_size);
    scan = sd_journal_scan(kDatasetA, reversed.data(),
                           newer_size + older_size);
    assert(scan.status == SdJournalScanStatus::NonMonotonicTransaction);
    assert(scan.record_count == 1U);
    assert(scan.valid_bytes == newer_size);

    visitor = {};
    visitor.reject = true;
    scan = sd_journal_scan(kDatasetA, journal.data(), first_size, visit,
                           &visitor);
    assert(scan.status == SdJournalScanStatus::ConsumerRejected);
    assert(scan.record_count == 0U);
    assert(scan.valid_bytes == 0U);

    std::array<uint8_t, kSdJournalMaxRecordBytes + 1U> bad_tail = {};
    std::memcpy(bad_tail.data(), journal.data(), first_size);
    bad_tail[first_size] = 'X';
    scan = sd_journal_scan(kDatasetA, bad_tail.data(), first_size + 1U);
    assert(scan.status == SdJournalScanStatus::Corrupt);
    assert(scan.valid_bytes == first_size);
    assert(scan.tail_bytes == 1U);

    std::array<uint8_t, kSdJournalMaxPayloadBytes> max_payload = {};
    std::array<uint8_t, kSdJournalMaxPayloadBytes + 1U> oversized_payload = {};
    std::array<uint8_t, kSdJournalMaxRecordBytes> max_record = {};
    assert(sd_journal_encode_record(
               SdJournalRecordType::Checkpoint, kDatasetA, UINT64_MAX,
               max_payload.data(), max_payload.size(), max_record.data(),
               max_record.size()) == max_record.size());
    scan = sd_journal_scan(kDatasetA, max_record.data(), max_record.size());
    assert(scan.status == SdJournalScanStatus::Ok);
    assert(scan.record_count == 1U);
    assert(scan.last_transaction_id == UINT64_MAX);
    assert(sd_journal_encode_record(
               SdJournalRecordType::Commit, kDatasetA, 1U,
               oversized_payload.data(), oversized_payload.size(),
               max_record.data(), max_record.size()) == 0U);

    assert(sd_journal_encode_record(
               SdJournalRecordType::Commit, kDatasetA, 0U,
               first_payload.data(), first_payload.size(), max_record.data(),
               max_record.size()) == 0U);
    assert(sd_journal_encode_record(
               SdJournalRecordType::Commit, "invalid", 1U,
               first_payload.data(), first_payload.size(), max_record.data(),
               max_record.size()) == 0U);
    assert(sd_journal_encode_record(
               SdJournalRecordType::Commit, kDatasetA, 1U, nullptr, 1U,
               max_record.data(), max_record.size()) == 0U);
    assert(sd_journal_encode_record(
               SdJournalRecordType::Commit, kDatasetA, 1U,
               first_payload.data(), first_payload.size(), max_record.data(),
               kSdJournalHeaderBytes) == 0U);

    scan = sd_journal_scan(kDatasetA, nullptr, 0U);
    assert(scan.status == SdJournalScanStatus::Ok);
    assert(scan.record_count == 0U);
    scan = sd_journal_scan("invalid", journal.data(), first_size);
    assert(scan.status == SdJournalScanStatus::InvalidArgument);
    scan = sd_journal_scan(kDatasetA, nullptr, 1U);
    assert(scan.status == SdJournalScanStatus::InvalidArgument);
    return 0;
}
