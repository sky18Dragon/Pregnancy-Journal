#include "sd_journal_payload.h"

#include <array>
#include <cstring>

#include "sd_storage_layout.h"

namespace sticky_storage {
namespace {

constexpr std::array<uint8_t, 4U> kTransactionMagic = {'P', 'J', 'T', 'X'};
constexpr std::array<uint8_t, 4U> kCheckpointMagic = {'P', 'J', 'C', 'P'};

void write_u16(uint8_t *bytes, uint16_t value)
{
    bytes[0] = static_cast<uint8_t>(value & 0xFFU);
    bytes[1] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
}

void write_u32(uint8_t *bytes, uint32_t value)
{
    for (size_t index = 0U; index < 4U; ++index) {
        bytes[index] =
            static_cast<uint8_t>((value >> (index * 8U)) & 0xFFU);
    }
}

void write_u64(uint8_t *bytes, uint64_t value)
{
    for (size_t index = 0U; index < 8U; ++index) {
        bytes[index] =
            static_cast<uint8_t>((value >> (index * 8U)) & 0xFFU);
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

bool name_valid(const char *name)
{
    return name != nullptr && std::strlen(name) <= kSdJournalObjectNameBytes &&
           fat83_name_valid(name);
}

bool zero_name(const char *name)
{
    return name != nullptr && name[0] == '\0';
}

bool mutation_valid(const SdJournalMutationReference &mutation)
{
    if (!repository_key_valid(mutation.key) ||
        mutation.committed_revision == 0U ||
        mutation.expected_revision == UINT32_MAX ||
        mutation.committed_revision != mutation.expected_revision + 1U) {
        return false;
    }
    if (mutation.type == RepositoryMutationType::Delete) {
        return mutation.object_bytes == 0U && mutation.object_crc32 == 0U &&
               zero_name(mutation.object_file);
    }
    return mutation.type == RepositoryMutationType::Upsert &&
           mutation.object_bytes != 0U && name_valid(mutation.object_file);
}

bool reserved_zero(const uint8_t *bytes, size_t count)
{
    for (size_t index = 0U; index < count; ++index) {
        if (bytes[index] != 0U) {
            return false;
        }
    }
    return true;
}

void encode_name(const char *name, uint8_t *output)
{
    std::memset(output, 0, kSdJournalObjectNameBytes);
    std::memcpy(output, name, std::strlen(name));
}

bool decode_name(const uint8_t *bytes, char *name)
{
    size_t length = 0U;
    while (length < kSdJournalObjectNameBytes && bytes[length] != 0U) {
        ++length;
    }
    if (length < kSdJournalObjectNameBytes &&
        !reserved_zero(bytes + length, kSdJournalObjectNameBytes - length)) {
        return false;
    }
    std::memcpy(name, bytes, length);
    name[length] = '\0';
    return true;
}

}  // namespace

size_t sd_journal_encode_transaction_payload(
    const SdJournalTransactionPayload &payload, uint8_t *output,
    size_t output_capacity)
{
    if (payload.mutation_count == 0U ||
        payload.mutation_count > payload.mutations.size() || output == nullptr) {
        return 0U;
    }
    const size_t total_size = kSdJournalTransactionHeaderBytes +
                              payload.mutation_count * kSdJournalMutationBytes;
    if (output_capacity < total_size) {
        return 0U;
    }
    for (size_t index = 0U; index < payload.mutation_count; ++index) {
        if (!mutation_valid(payload.mutations[index])) {
            return 0U;
        }
    }

    std::memset(output, 0, total_size);
    std::memcpy(output, kTransactionMagic.data(), kTransactionMagic.size());
    write_u16(output + 4U, kSdJournalPayloadSchemaVersion);
    write_u16(output + 6U,
              static_cast<uint16_t>(kSdJournalTransactionHeaderBytes));
    write_u16(output + 8U, static_cast<uint16_t>(total_size));
    output[10U] = static_cast<uint8_t>(payload.mutation_count);

    for (size_t index = 0U; index < payload.mutation_count; ++index) {
        const SdJournalMutationReference &mutation = payload.mutations[index];
        uint8_t *encoded = output + kSdJournalTransactionHeaderBytes +
                           index * kSdJournalMutationBytes;
        encoded[0] = static_cast<uint8_t>(mutation.type);
        encoded[1] = static_cast<uint8_t>(mutation.key.repository);
        write_u32(encoded + 4U, mutation.expected_revision);
        write_u32(encoded + 8U, mutation.committed_revision);
        write_u32(encoded + 12U, mutation.object_bytes);
        write_u32(encoded + 16U, mutation.object_crc32);
        std::memcpy(encoded + 20U, mutation.key.record_id,
                    kRepositoryRecordIdCharacters);
        encode_name(mutation.object_file, encoded + 52U);
    }
    return total_size;
}

SdJournalPayloadStatus sd_journal_decode_transaction_payload(
    const uint8_t *bytes, size_t size, SdJournalTransactionPayload &payload)
{
    payload = {};
    if (bytes == nullptr && size != 0U) {
        return SdJournalPayloadStatus::InvalidArgument;
    }
    if (size < kSdJournalTransactionHeaderBytes) {
        return SdJournalPayloadStatus::Incomplete;
    }
    if (std::memcmp(bytes, kTransactionMagic.data(),
                    kTransactionMagic.size()) != 0) {
        return SdJournalPayloadStatus::Corrupt;
    }
    if (read_u16(bytes + 4U) != kSdJournalPayloadSchemaVersion) {
        return SdJournalPayloadStatus::UnsupportedSchema;
    }
    if (read_u16(bytes + 6U) != kSdJournalTransactionHeaderBytes ||
        !reserved_zero(bytes + 11U, 5U)) {
        return SdJournalPayloadStatus::Corrupt;
    }
    const size_t count = bytes[10U];
    if (count == 0U || count > payload.mutations.size()) {
        return SdJournalPayloadStatus::Corrupt;
    }
    const size_t expected_size = kSdJournalTransactionHeaderBytes +
                                 count * kSdJournalMutationBytes;
    if (read_u16(bytes + 8U) != expected_size) {
        return SdJournalPayloadStatus::Corrupt;
    }
    if (size < expected_size) {
        return SdJournalPayloadStatus::Incomplete;
    }
    if (size != expected_size) {
        return SdJournalPayloadStatus::Corrupt;
    }

    payload.mutation_count = count;
    for (size_t index = 0U; index < count; ++index) {
        const uint8_t *encoded = bytes + kSdJournalTransactionHeaderBytes +
                                 index * kSdJournalMutationBytes;
        if (!reserved_zero(encoded + 2U, 2U)) {
            return SdJournalPayloadStatus::Corrupt;
        }
        SdJournalMutationReference &mutation = payload.mutations[index];
        mutation.type = static_cast<RepositoryMutationType>(encoded[0]);
        mutation.key.repository = static_cast<RepositoryKind>(encoded[1]);
        mutation.expected_revision = read_u32(encoded + 4U);
        mutation.committed_revision = read_u32(encoded + 8U);
        mutation.object_bytes = read_u32(encoded + 12U);
        mutation.object_crc32 = read_u32(encoded + 16U);
        std::memcpy(mutation.key.record_id, encoded + 20U,
                    kRepositoryRecordIdCharacters);
        mutation.key.record_id[kRepositoryRecordIdCharacters] = '\0';
        if (!decode_name(encoded + 52U, mutation.object_file) ||
            !mutation_valid(mutation)) {
            return SdJournalPayloadStatus::Corrupt;
        }
    }
    return SdJournalPayloadStatus::Ok;
}

size_t sd_journal_encode_checkpoint_payload(
    const SdJournalCheckpointPayload &payload, uint8_t *output,
    size_t output_capacity)
{
    if (payload.last_transaction_id == 0U || payload.segment_number == 0U ||
        payload.index_bytes == 0U || !name_valid(payload.index_file) ||
        output == nullptr || output_capacity < kSdJournalCheckpointBytes) {
        return 0U;
    }
    std::memset(output, 0, kSdJournalCheckpointBytes);
    std::memcpy(output, kCheckpointMagic.data(), kCheckpointMagic.size());
    write_u16(output + 4U, kSdJournalPayloadSchemaVersion);
    write_u16(output + 6U,
              static_cast<uint16_t>(kSdJournalCheckpointBytes));
    write_u64(output + 8U, payload.last_transaction_id);
    write_u32(output + 16U, payload.segment_number);
    write_u32(output + 20U, payload.live_record_count);
    write_u32(output + 24U, payload.index_bytes);
    write_u32(output + 28U, payload.index_crc32);
    encode_name(payload.index_file, output + 32U);
    return kSdJournalCheckpointBytes;
}

SdJournalPayloadStatus sd_journal_decode_checkpoint_payload(
    const uint8_t *bytes, size_t size, SdJournalCheckpointPayload &payload)
{
    payload = {};
    if (bytes == nullptr && size != 0U) {
        return SdJournalPayloadStatus::InvalidArgument;
    }
    if (size < kSdJournalCheckpointBytes) {
        return SdJournalPayloadStatus::Incomplete;
    }
    if (size != kSdJournalCheckpointBytes ||
        std::memcmp(bytes, kCheckpointMagic.data(),
                    kCheckpointMagic.size()) != 0 ||
        read_u16(bytes + 6U) != kSdJournalCheckpointBytes ||
        !reserved_zero(bytes + 44U, 20U)) {
        return SdJournalPayloadStatus::Corrupt;
    }
    if (read_u16(bytes + 4U) != kSdJournalPayloadSchemaVersion) {
        return SdJournalPayloadStatus::UnsupportedSchema;
    }
    payload.last_transaction_id = read_u64(bytes + 8U);
    payload.segment_number = read_u32(bytes + 16U);
    payload.live_record_count = read_u32(bytes + 20U);
    payload.index_bytes = read_u32(bytes + 24U);
    payload.index_crc32 = read_u32(bytes + 28U);
    if (!decode_name(bytes + 32U, payload.index_file) ||
        payload.last_transaction_id == 0U || payload.segment_number == 0U ||
        payload.index_bytes == 0U || !name_valid(payload.index_file)) {
        return SdJournalPayloadStatus::Corrupt;
    }
    return SdJournalPayloadStatus::Ok;
}

bool sd_journal_payload_valid_for_record(SdJournalRecordType type,
                                         uint64_t transaction_id,
                                         const uint8_t *bytes, size_t size)
{
    if (transaction_id == 0U || (bytes == nullptr && size != 0U)) {
        return false;
    }

    if (type == SdJournalRecordType::Checkpoint) {
        SdJournalCheckpointPayload checkpoint = {};
        return sd_journal_decode_checkpoint_payload(bytes, size, checkpoint) ==
                   SdJournalPayloadStatus::Ok &&
               checkpoint.last_transaction_id < transaction_id;
    }
    if (type != SdJournalRecordType::Commit &&
        type != SdJournalRecordType::Tombstone) {
        return false;
    }

    SdJournalTransactionPayload transaction = {};
    if (sd_journal_decode_transaction_payload(bytes, size, transaction) !=
        SdJournalPayloadStatus::Ok) {
        return false;
    }

    bool has_upsert = false;
    bool has_delete = false;
    for (size_t index = 0U; index < transaction.mutation_count; ++index) {
        has_upsert |= transaction.mutations[index].type ==
                      RepositoryMutationType::Upsert;
        has_delete |= transaction.mutations[index].type ==
                      RepositoryMutationType::Delete;
    }
    return type == SdJournalRecordType::Commit ? has_upsert :
                                                 has_delete && !has_upsert;
}

}  // namespace sticky_storage
