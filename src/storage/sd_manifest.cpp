#include "sd_manifest.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <limits>

#include "storage_crc32.h"

namespace sticky_storage {
namespace {

constexpr char kPayloadFormat[] =
    "{\"magic\":\"PJMF\",\"schema\":%u,\"dataset_id\":\"%s\","
    "\"generation\":%llu}";
constexpr char kRecordFormat[] =
    "{\"magic\":\"PJMF\",\"schema\":%u,\"dataset_id\":\"%s\","
    "\"generation\":%llu,\"crc32\":\"%08X\"}\n";

size_t encode_payload(const SdManifest &manifest, char *output,
                      size_t output_capacity)
{
    const int count = std::snprintf(
        output, output_capacity, kPayloadFormat,
        static_cast<unsigned>(manifest.schema_version), manifest.dataset_id,
        static_cast<unsigned long long>(manifest.commit_generation));
    if (count < 0 || static_cast<size_t>(count) >= output_capacity) {
        return 0U;
    }
    return static_cast<size_t>(count);
}

bool consume_literal(const char *&cursor, const char *end,
                     const char *literal)
{
    const size_t length = std::strlen(literal);
    if (static_cast<size_t>(end - cursor) < length ||
        std::memcmp(cursor, literal, length) != 0) {
        return false;
    }
    cursor += length;
    return true;
}

bool parse_generation(const char *&cursor, const char *end,
                      uint64_t &generation)
{
    if (cursor == end || *cursor < '1' || *cursor > '9') {
        return false;
    }
    generation = 0U;
    while (cursor != end && *cursor >= '0' && *cursor <= '9') {
        const uint64_t digit = static_cast<uint64_t>(*cursor - '0');
        if (generation >
            (std::numeric_limits<uint64_t>::max() - digit) / 10U) {
            return false;
        }
        generation = generation * 10U + digit;
        ++cursor;
    }
    return true;
}

bool parse_crc32(const char *&cursor, const char *end, uint32_t &checksum)
{
    checksum = 0U;
    for (size_t index = 0U; index < 8U; ++index) {
        if (cursor == end) {
            return false;
        }
        uint32_t digit = 0U;
        if (*cursor >= '0' && *cursor <= '9') {
            digit = static_cast<uint32_t>(*cursor - '0');
        } else if (*cursor >= 'A' && *cursor <= 'F') {
            digit = static_cast<uint32_t>(*cursor - 'A') + 10U;
        } else {
            return false;
        }
        checksum = (checksum << 4U) | digit;
        ++cursor;
    }
    return true;
}

}  // namespace

bool sd_dataset_id_valid(const char *dataset_id)
{
    if (dataset_id == nullptr) {
        return false;
    }
    for (size_t index = 0U; index < kDatasetIdCharacters; ++index) {
        const char value = dataset_id[index];
        if (!((value >= '0' && value <= '9') ||
              (value >= 'a' && value <= 'f'))) {
            return false;
        }
    }
    return dataset_id[kDatasetIdCharacters] == '\0';
}

bool sd_manifest_make(const char *dataset_id, uint64_t commit_generation,
                      SdManifest &manifest)
{
    if (!sd_dataset_id_valid(dataset_id) || commit_generation == 0U) {
        return false;
    }
    manifest = {};
    manifest.schema_version = kSdManifestSchemaVersion;
    std::memcpy(manifest.dataset_id, dataset_id,
                kDatasetIdCharacters + 1U);
    manifest.commit_generation = commit_generation;
    return true;
}

bool sd_manifest_make_next(const SdManifest &current, SdManifest &next)
{
    next = {};
    if (!sd_manifest_valid(current) ||
        current.commit_generation == UINT64_MAX) {
        return false;
    }
    return sd_manifest_make(current.dataset_id,
                            current.commit_generation + 1U, next);
}

bool sd_manifest_valid(const SdManifest &manifest)
{
    return manifest.schema_version == kSdManifestSchemaVersion &&
           sd_dataset_id_valid(manifest.dataset_id) &&
           manifest.commit_generation != 0U;
}

size_t sd_manifest_encode(const SdManifest &manifest, char *output,
                          size_t output_capacity)
{
    if (!sd_manifest_valid(manifest) || output == nullptr ||
        output_capacity == 0U) {
        return 0U;
    }

    std::array<char, kSdManifestMaxBytes> payload = {};
    const size_t payload_size =
        encode_payload(manifest, payload.data(), payload.size());
    if (payload_size == 0U) {
        return 0U;
    }
    const uint32_t checksum = crc32(
        reinterpret_cast<const uint8_t *>(payload.data()), payload_size);
    const int count = std::snprintf(
        output, output_capacity, kRecordFormat,
        static_cast<unsigned>(manifest.schema_version), manifest.dataset_id,
        static_cast<unsigned long long>(manifest.commit_generation),
        static_cast<unsigned>(checksum));
    if (count < 0 || static_cast<size_t>(count) >= output_capacity) {
        return 0U;
    }
    return static_cast<size_t>(count);
}

bool sd_manifest_decode(const uint8_t *bytes, size_t size,
                        SdManifest &manifest)
{
    if (bytes == nullptr || size == 0U || size >= kSdManifestMaxBytes) {
        return false;
    }
    std::array<char, kSdManifestMaxBytes> input = {};
    std::memcpy(input.data(), bytes, size);

    const char *cursor = input.data();
    const char *const end = cursor + size;
    if (!consume_literal(
            cursor, end,
            "{\"magic\":\"PJMF\",\"schema\":1,\"dataset_id\":\"")) {
        return false;
    }
    char dataset_id[kDatasetIdCharacters + 1U] = {};
    if (static_cast<size_t>(end - cursor) < kDatasetIdCharacters) {
        return false;
    }
    std::memcpy(dataset_id, cursor, kDatasetIdCharacters);
    cursor += kDatasetIdCharacters;
    if (!sd_dataset_id_valid(dataset_id) ||
        !consume_literal(cursor, end, "\",\"generation\":")) {
        return false;
    }
    uint64_t generation = 0U;
    if (!parse_generation(cursor, end, generation) ||
        !consume_literal(cursor, end, ",\"crc32\":\"")) {
        return false;
    }
    uint32_t parsed_checksum = 0U;
    if (!parse_crc32(cursor, end, parsed_checksum) ||
        !consume_literal(cursor, end, "\"}\n") || cursor != end) {
        return false;
    }

    SdManifest decoded = {};
    if (!sd_manifest_make(dataset_id, generation, decoded)) {
        return false;
    }

    std::array<char, kSdManifestMaxBytes> canonical = {};
    const size_t canonical_size =
        sd_manifest_encode(decoded, canonical.data(), canonical.size());
    const char *crc_marker = std::strstr(canonical.data(), "crc32\":\"");
    if (canonical_size != size || crc_marker == nullptr ||
        std::memcmp(canonical.data(), input.data(), size) != 0) {
        return false;
    }
    uint32_t canonical_checksum = 0U;
    const char *canonical_crc = crc_marker + std::strlen("crc32\":\"");
    const char *const canonical_end = canonical.data() + canonical_size;
    if (!parse_crc32(canonical_crc, canonical_end, canonical_checksum) ||
        canonical_checksum != parsed_checksum) {
        return false;
    }

    manifest = decoded;
    return true;
}

SdManifestSlot sd_manifest_slot_for_generation(uint64_t generation)
{
    return (generation & 1U) != 0U ? SdManifestSlot::A
                                   : SdManifestSlot::B;
}

bool sd_manifest_select(const SdManifest *slot_a, const SdManifest *slot_b,
                        SdManifest &selected, SdManifestSlot &selected_slot)
{
    const bool a_valid = slot_a != nullptr && sd_manifest_valid(*slot_a);
    const bool b_valid = slot_b != nullptr && sd_manifest_valid(*slot_b);
    if (!a_valid && !b_valid) {
        return false;
    }
    if (a_valid && b_valid &&
        std::strcmp(slot_a->dataset_id, slot_b->dataset_id) != 0) {
        return false;
    }
    if (!b_valid || (a_valid && slot_a->commit_generation >=
                                    slot_b->commit_generation)) {
        selected = *slot_a;
        selected_slot = SdManifestSlot::A;
    } else {
        selected = *slot_b;
        selected_slot = SdManifestSlot::B;
    }
    return true;
}

}  // namespace sticky_storage
