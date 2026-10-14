#include "sd_journal.h"

#include <array>
#include <cstring>

#include "sd_manifest.h"
#include "storage_crc32.h"

namespace sticky_storage {
namespace {

constexpr std::array<uint8_t, 4U> kMagic = {'P', 'J', 'T', 'L'};
constexpr size_t kSchemaOffset = 4U;
constexpr size_t kHeaderSizeOffset = 6U;
constexpr size_t kTotalSizeOffset = 8U;
constexpr size_t kPayloadSizeOffset = 12U;
constexpr size_t kRecordTypeOffset = 16U;
constexpr size_t kReservedOffset = 17U;
constexpr size_t kReservedBytes = 7U;
constexpr size_t kTransactionIdOffset = 24U;
constexpr size_t kDatasetIdOffset = 32U;
constexpr size_t kPayloadCrcOffset = 64U;
constexpr size_t kRecordCrcOffset = 68U;

void write_u16(uint8_t *bytes, uint16_t value)
{
    bytes[0] = static_cast<uint8_t>(value & 0xFFU);
    bytes[1] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
}

void write_u32(uint8_t *bytes, uint32_t value)
{
    for (size_t index = 0U; index < 4U; ++index) {
        bytes[index] = static_cast<uint8_t>((value >> (index * 8U)) & 0xFFU);
    }
}

void write_u64(uint8_t *bytes, uint64_t value)
{
    for (size_t index = 0U; index < 8U; ++index) {
        bytes[index] = static_cast<uint8_t>((value >> (index * 8U)) & 0xFFU);
    }
}

uint16_t read_u16(const uint8_t *bytes)
{
    return static_cast<uint16_t>(bytes[0]) |
           static_cast<uint16_t>(static_cast<uint16_t>(bytes[1]) << 8U);
}

uint32_t read_u32(const uint8_t *bytes)
{
    uint32_t value = 0U;
    for (size_t index = 0U; index < 4U; ++index) {
        value |= static_cast<uint32_t>(bytes[index]) << (index * 8U);
    }
    return value;
}

uint64_t read_u64(const uint8_t *bytes)
{
    uint64_t value = 0U;
    for (size_t index = 0U; index < 8U; ++index) {
        value |= static_cast<uint64_t>(bytes[index]) << (index * 8U);
    }
    return value;
}

bool record_type_valid(uint8_t value)
{
    return value == static_cast<uint8_t>(SdJournalRecordType::Commit) ||
           value == static_cast<uint8_t>(SdJournalRecordType::Tombstone) ||
           value == static_cast<uint8_t>(SdJournalRecordType::Checkpoint);
}

bool magic_prefix_valid(const uint8_t *bytes, size_t size)
{
    return size == 0U ||
           (size <= kMagic.size() &&
            std::memcmp(bytes, kMagic.data(), size) == 0);
}

uint32_t record_crc(const uint8_t *header, const uint8_t *payload,
                    size_t payload_size)
{
    uint32_t checksum = crc32_update(kCrc32Initial, header, kRecordCrcOffset);
    checksum = crc32_update(checksum, payload, payload_size);
    return checksum ^ kCrc32FinalXor;
}

SdJournalScanStatus scan_status(SdJournalDecodeStatus status)
{
    switch (status) {
    case SdJournalDecodeStatus::Ok:
        return SdJournalScanStatus::Ok;
    case SdJournalDecodeStatus::Incomplete:
        return SdJournalScanStatus::TailTruncated;
    case SdJournalDecodeStatus::Corrupt:
        return SdJournalScanStatus::Corrupt;
    case SdJournalDecodeStatus::UnsupportedSchema:
        return SdJournalScanStatus::UnsupportedSchema;
    case SdJournalDecodeStatus::UnsupportedRecordType:
        return SdJournalScanStatus::UnsupportedRecordType;
    case SdJournalDecodeStatus::DatasetMismatch:
        return SdJournalScanStatus::DatasetMismatch;
    case SdJournalDecodeStatus::InvalidArgument:
        return SdJournalScanStatus::InvalidArgument;
    }
    return SdJournalScanStatus::Corrupt;
}

}  // namespace

SdJournalDecodeStatus sd_journal_decode_record(
    const char *expected_dataset_id, const uint8_t *bytes, size_t size,
    SdJournalRecordView &record)
{
    record = {};
    if (!sd_dataset_id_valid(expected_dataset_id) ||
        (bytes == nullptr && size != 0U)) {
        return SdJournalDecodeStatus::InvalidArgument;
    }
    if (size < kMagic.size()) {
        return magic_prefix_valid(bytes, size)
                   ? SdJournalDecodeStatus::Incomplete
                   : SdJournalDecodeStatus::Corrupt;
    }
    if (std::memcmp(bytes, kMagic.data(), kMagic.size()) != 0) {
        return SdJournalDecodeStatus::Corrupt;
    }
    if (size < kSdJournalHeaderBytes) {
        return SdJournalDecodeStatus::Incomplete;
    }
    if (read_u16(bytes + kSchemaOffset) != kSdJournalSchemaVersion) {
        return SdJournalDecodeStatus::UnsupportedSchema;
    }
    if (read_u16(bytes + kHeaderSizeOffset) != kSdJournalHeaderBytes) {
        return SdJournalDecodeStatus::Corrupt;
    }

    const uint32_t total_size = read_u32(bytes + kTotalSizeOffset);
    const uint32_t payload_size = read_u32(bytes + kPayloadSizeOffset);
    if (payload_size > kSdJournalMaxPayloadBytes ||
        total_size != kSdJournalHeaderBytes + payload_size) {
        return SdJournalDecodeStatus::Corrupt;
    }
    record.payload_size = payload_size;
    record.encoded_size = total_size;
    if (total_size > size) {
        return SdJournalDecodeStatus::Incomplete;
    }
    const uint8_t type = bytes[kRecordTypeOffset];
    if (!record_type_valid(type)) {
        return SdJournalDecodeStatus::UnsupportedRecordType;
    }
    for (size_t index = 0U; index < kReservedBytes; ++index) {
        if (bytes[kReservedOffset + index] != 0U) {
            return SdJournalDecodeStatus::Corrupt;
        }
    }

    const uint64_t transaction_id = read_u64(bytes + kTransactionIdOffset);
    if (transaction_id == 0U) {
        return SdJournalDecodeStatus::Corrupt;
    }
    char dataset_id[kDatasetIdCharacters + 1U] = {};
    std::memcpy(dataset_id, bytes + kDatasetIdOffset,
                kDatasetIdCharacters);
    if (!sd_dataset_id_valid(dataset_id)) {
        return SdJournalDecodeStatus::Corrupt;
    }

    const uint8_t *payload = bytes + kSdJournalHeaderBytes;
    const uint32_t payload_crc = read_u32(bytes + kPayloadCrcOffset);
    const uint32_t frame_crc = read_u32(bytes + kRecordCrcOffset);
    if (payload_crc != crc32(payload, payload_size) ||
        frame_crc != record_crc(bytes, payload, payload_size)) {
        return SdJournalDecodeStatus::Corrupt;
    }
    if (std::strcmp(dataset_id, expected_dataset_id) != 0) {
        return SdJournalDecodeStatus::DatasetMismatch;
    }

    record.type = static_cast<SdJournalRecordType>(type);
    record.transaction_id = transaction_id;
    std::memcpy(record.dataset_id, dataset_id, sizeof(dataset_id));
    record.payload = payload;
    record.payload_crc32 = payload_crc;
    record.frame_crc32 = frame_crc;
    return SdJournalDecodeStatus::Ok;
}

size_t sd_journal_encode_record(SdJournalRecordType type,
                                const char *dataset_id,
                                uint64_t transaction_id,
                                const uint8_t *payload, size_t payload_size,
                                uint8_t *output, size_t output_capacity)
{
    if (!record_type_valid(static_cast<uint8_t>(type)) ||
        !sd_dataset_id_valid(dataset_id) || transaction_id == 0U ||
        (payload == nullptr && payload_size != 0U) ||
        payload_size > kSdJournalMaxPayloadBytes || output == nullptr ||
        output_capacity < kSdJournalHeaderBytes + payload_size) {
        return 0U;
    }

    const size_t total_size = kSdJournalHeaderBytes + payload_size;
    std::memset(output, 0, total_size);
    std::memcpy(output, kMagic.data(), kMagic.size());
    write_u16(output + kSchemaOffset, kSdJournalSchemaVersion);
    write_u16(output + kHeaderSizeOffset,
              static_cast<uint16_t>(kSdJournalHeaderBytes));
    write_u32(output + kTotalSizeOffset, static_cast<uint32_t>(total_size));
    write_u32(output + kPayloadSizeOffset,
              static_cast<uint32_t>(payload_size));
    output[kRecordTypeOffset] = static_cast<uint8_t>(type);
    write_u64(output + kTransactionIdOffset, transaction_id);
    std::memcpy(output + kDatasetIdOffset, dataset_id,
                kDatasetIdCharacters);
    if (payload_size != 0U) {
        std::memcpy(output + kSdJournalHeaderBytes, payload, payload_size);
    }
    write_u32(output + kPayloadCrcOffset, crc32(payload, payload_size));
    write_u32(output + kRecordCrcOffset,
              record_crc(output, output + kSdJournalHeaderBytes,
                         payload_size));
    return total_size;
}

SdJournalScanResult sd_journal_scan(const char *expected_dataset_id,
                                    const uint8_t *bytes, size_t size,
                                    SdJournalRecordVisitor visitor,
                                    void *visitor_context)
{
    SdJournalScanResult result = {};
    if (!sd_dataset_id_valid(expected_dataset_id) ||
        (bytes == nullptr && size != 0U)) {
        result.status = SdJournalScanStatus::InvalidArgument;
        return result;
    }

    result.status = SdJournalScanStatus::Ok;
    size_t offset = 0U;
    while (offset < size) {
        SdJournalRecordView record = {};
        const SdJournalDecodeStatus decoded = sd_journal_decode_record(
            expected_dataset_id, bytes + offset, size - offset, record);
        if (decoded != SdJournalDecodeStatus::Ok) {
            result.status = scan_status(decoded);
            result.valid_bytes = offset;
            result.tail_bytes = size - offset;
            return result;
        }
        if (record.transaction_id <= result.last_transaction_id) {
            result.status = SdJournalScanStatus::NonMonotonicTransaction;
            result.valid_bytes = offset;
            result.tail_bytes = size - offset;
            return result;
        }
        if (visitor != nullptr && !visitor(visitor_context, record)) {
            result.status = SdJournalScanStatus::ConsumerRejected;
            result.valid_bytes = offset;
            result.tail_bytes = size - offset;
            return result;
        }

        offset += record.encoded_size;
        ++result.record_count;
        result.last_transaction_id = record.transaction_id;
        result.valid_bytes = offset;
    }
    return result;
}

}  // namespace sticky_storage
