#include <array>
#include <cassert>
#include <cstring>

#include "sd_manifest.h"
#include "sd_storage_layout.h"
#include "storage_contract.h"

namespace {
constexpr char kDatasetA[] = "0123456789abcdef0123456789abcdef";
constexpr char kDatasetB[] = "fedcba9876543210fedcba9876543210";
}

int main()
{
    using namespace sticky_storage;

    assert(fat83_name_valid("PREGJNL"));
    assert(fat83_name_valid("MNF000A.JSN"));
    assert(!fat83_name_valid("pregjnl"));
    assert(!fat83_name_valid("PREGNANCY"));
    assert(!fat83_name_valid("MANIFEST.JSON"));
    assert(std::strlen(kManifestSlotAPath) < kMaxPathBytes);
    assert(std::strlen(kManifestSlotBPath) < kMaxPathBytes);

    SdManifest first = {};
    assert(sd_manifest_make(kDatasetA, 1U, first));
    assert(sd_manifest_valid(first));
    assert(!sd_manifest_make("short", 1U, first));
    assert(!sd_manifest_make("0123456789ABCDEF0123456789ABCDEF", 1U,
                             first));
    assert(!sd_manifest_make(kDatasetA, 0U, first));
    assert(sd_manifest_make(kDatasetA, 1U, first));

    std::array<char, kSdManifestMaxBytes> encoded = {};
    const size_t encoded_size =
        sd_manifest_encode(first, encoded.data(), encoded.size());
    assert(encoded_size > 0U && encoded_size < encoded.size());
    assert(encoded[encoded_size - 1U] == '\n');
    std::array<char, 32U> too_small = {};
    assert(sd_manifest_encode(first, too_small.data(), too_small.size()) == 0U);

    SdManifest decoded = {};
    assert(sd_manifest_decode(
        reinterpret_cast<const uint8_t *>(encoded.data()), encoded_size,
        decoded));
    assert(decoded.schema_version == kSdManifestSchemaVersion);
    assert(decoded.commit_generation == 1U);
    assert(std::strcmp(decoded.dataset_id, kDatasetA) == 0);

    auto corrupt = encoded;
    const char *crc_marker = std::strstr(corrupt.data(), "crc32");
    assert(crc_marker != nullptr);
    const size_t crc_digit = static_cast<size_t>(crc_marker - corrupt.data()) +
                             std::strlen("crc32\":\"");
    corrupt[crc_digit] = corrupt[crc_digit] == '0' ? '1' : '0';
    assert(!sd_manifest_decode(
        reinterpret_cast<const uint8_t *>(corrupt.data()), encoded_size,
        decoded));
    corrupt = encoded;
    corrupt[crc_digit] = 'a';
    assert(!sd_manifest_decode(
        reinterpret_cast<const uint8_t *>(corrupt.data()), encoded_size,
        decoded));
    corrupt = encoded;
    const char *id_marker = std::strstr(corrupt.data(), "dataset_id");
    assert(id_marker != nullptr);
    const size_t id_digit = static_cast<size_t>(id_marker - corrupt.data()) +
                            std::strlen("dataset_id\":\"");
    corrupt[id_digit] = 'A';
    assert(!sd_manifest_decode(
        reinterpret_cast<const uint8_t *>(corrupt.data()), encoded_size,
        decoded));
    assert(!sd_manifest_decode(
        reinterpret_cast<const uint8_t *>(encoded.data()), encoded_size - 1U,
        decoded));
    encoded[encoded_size] = 'x';
    assert(!sd_manifest_decode(
        reinterpret_cast<const uint8_t *>(encoded.data()), encoded_size + 1U,
        decoded));
    constexpr char kOverflowGeneration[] =
        "{\"magic\":\"PJMF\",\"schema\":1,\"dataset_id\":\""
        "0123456789abcdef0123456789abcdef\",\"generation\":"
        "18446744073709551616,\"crc32\":\"00000000\"}\n";
    assert(!sd_manifest_decode(
        reinterpret_cast<const uint8_t *>(kOverflowGeneration),
        sizeof(kOverflowGeneration) - 1U, decoded));

    SdManifest second = {};
    assert(sd_manifest_make(kDatasetA, 2U, second));
    SdManifest selected = {};
    SdManifestSlot slot = SdManifestSlot::A;
    assert(sd_manifest_select(&first, &second, selected, slot));
    assert(slot == SdManifestSlot::B && selected.commit_generation == 2U);
    assert(sd_manifest_slot_for_generation(1U) == SdManifestSlot::A);
    assert(sd_manifest_slot_for_generation(2U) == SdManifestSlot::B);

    SdManifest next = {};
    assert(sd_manifest_make_next(second, next));
    assert(std::strcmp(next.dataset_id, second.dataset_id) == 0);
    assert(next.commit_generation == second.commit_generation + 1U);
    SdManifest invalid = {};
    assert(!sd_manifest_make_next(invalid, next));
    SdManifest exhausted = second;
    exhausted.commit_generation = UINT64_MAX;
    assert(!sd_manifest_make_next(exhausted, next));

    SdManifest duplicate = second;
    assert(sd_manifest_select(&second, &duplicate, selected, slot));
    SdManifest conflict = {};
    assert(sd_manifest_make(kDatasetB, 2U, conflict));
    assert(!sd_manifest_select(&second, &conflict, selected, slot));
    assert(sd_manifest_make(kDatasetB, 3U, conflict));
    assert(!sd_manifest_select(&second, &conflict, selected, slot));
    assert(sd_manifest_select(nullptr, &second, selected, slot));
    assert(!sd_manifest_select(nullptr, nullptr, selected, slot));
}
