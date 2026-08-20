#include "pet_save_record.h"

#include <cstring>
#include <limits>

uint16_t pet_save_checksum(const uint8_t *data, size_t size)
{
    if (data == nullptr) {
        return 0U;
    }
    uint16_t sum = 0xFFFFU;
    for (size_t index = 0U; index < size; ++index) {
        sum ^= data[index];
        sum = static_cast<uint16_t>((sum << 3U) | (sum >> 13U));
        sum = static_cast<uint16_t>(sum + data[index]);
    }
    return sum;
}

PetSaveHeader pet_save_make_header(const void *payload,
                                   size_t payload_size,
                                   uint16_t format_version,
                                   uint32_t sequence,
                                   uint32_t saved_epoch_seconds)
{
    PetSaveHeader header = {};
    if (payload == nullptr || payload_size == 0U ||
        payload_size > std::numeric_limits<uint16_t>::max()) {
        header.magic = 0U;
        return header;
    }
    header.format_version = format_version;
    header.state_size = static_cast<uint16_t>(payload_size);
    header.sequence = sequence;
    header.saved_epoch_seconds = saved_epoch_seconds;
    header.checksum = pet_save_checksum(
        static_cast<const uint8_t *>(payload), payload_size);
    return header;
}

bool pet_save_validate_payload(const PetSaveHeader &header,
                               const void *payload,
                               size_t payload_size,
                               uint16_t format_version)
{
    if (payload == nullptr || payload_size == 0U ||
        payload_size > std::numeric_limits<uint16_t>::max() ||
        header.magic != kPetSaveMagic ||
        header.format_version != format_version ||
        header.state_size != payload_size) {
        return false;
    }
    return header.checksum == pet_save_checksum(
        static_cast<const uint8_t *>(payload), payload_size);
}

bool pet_save_sequence_newer(uint32_t candidate, uint32_t current)
{
    return static_cast<int32_t>(candidate - current) > 0;
}

int pet_save_select_newest_header(const PetSaveHeader *headers,
                                  const bool *valid,
                                  size_t count)
{
    if (headers == nullptr || valid == nullptr || count == 0U) {
        return -1;
    }
    int newest = -1;
    for (size_t index = 0U; index < count; ++index) {
        if (!valid[index]) {
            continue;
        }
        if (newest < 0 || pet_save_sequence_newer(
                              headers[index].sequence,
                              headers[static_cast<size_t>(newest)].sequence)) {
            newest = static_cast<int>(index);
        }
    }
    return newest;
}

size_t pet_save_select_write_header(const PetSaveHeader *headers,
                                    const bool *valid,
                                    size_t count)
{
    if (headers == nullptr || valid == nullptr || count == 0U) {
        return 0U;
    }
    for (size_t index = 0U; index < count; ++index) {
        if (!valid[index]) {
            return index;
        }
    }
    size_t oldest = 0U;
    for (size_t index = 1U; index < count; ++index) {
        if (pet_save_sequence_newer(headers[oldest].sequence,
                                    headers[index].sequence)) {
            oldest = index;
        }
    }
    return oldest;
}

PetSaveRecord pet_save_make_record(const PetCoreState &state,
                                   uint32_t sequence,
                                   uint32_t saved_epoch_seconds)
{
    PetSaveRecord record = {};
    record.state = state;
    record.header = pet_save_make_header(
        &record.state,
        sizeof(record.state),
        kPetSaveFormatVersion,
        sequence,
        saved_epoch_seconds);
    return record;
}

bool pet_save_validate(const PetSaveRecord &record)
{
    if (record.state.version != kPetCoreStateVersion) {
        return false;
    }
    return pet_save_validate_payload(
        record.header,
        &record.state,
        sizeof(record.state),
        kPetSaveFormatVersion);
}

int pet_save_select_newest(const PetSaveRecord *records, size_t count)
{
    if (records == nullptr || count == 0U) {
        return -1;
    }
    PetSaveHeader headers[kPetSaveSlotCount] = {};
    bool valid[kPetSaveSlotCount] = {};
    const size_t bounded_count = count > kPetSaveSlotCount
                                     ? kPetSaveSlotCount
                                     : count;
    for (size_t index = 0U; index < bounded_count; ++index) {
        headers[index] = records[index].header;
        valid[index] = pet_save_validate(records[index]);
    }
    return pet_save_select_newest_header(
        headers, valid, bounded_count);
}

size_t pet_save_select_write_slot(const PetSaveRecord *records, size_t count)
{
    if (records == nullptr || count == 0U) {
        return 0U;
    }
    PetSaveHeader headers[kPetSaveSlotCount] = {};
    bool valid[kPetSaveSlotCount] = {};
    const size_t bounded_count = count > kPetSaveSlotCount
                                     ? kPetSaveSlotCount
                                     : count;
    for (size_t index = 0U; index < bounded_count; ++index) {
        headers[index] = records[index].header;
        valid[index] = pet_save_validate(records[index]);
    }
    return pet_save_select_write_header(
        headers, valid, bounded_count);
}
