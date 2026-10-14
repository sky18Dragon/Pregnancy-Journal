#pragma once

#include <cstddef>
#include <cstdint>

namespace sticky_storage {

constexpr uint32_t kSdManifestSchemaVersion = 1U;
constexpr size_t kDatasetIdCharacters = 32U;
constexpr size_t kSdManifestMaxBytes = 192U;

enum class SdManifestSlot : uint8_t {
    A = 0,
    B,
};

struct SdManifest {
    uint32_t schema_version = 0U;
    char dataset_id[kDatasetIdCharacters + 1U] = {};
    uint64_t commit_generation = 0U;
};

bool sd_dataset_id_valid(const char *dataset_id);
bool sd_manifest_make(const char *dataset_id, uint64_t commit_generation,
                      SdManifest &manifest);
// Builds the next generation for the same validated dataset. Returns false
// for invalid input or UINT64_MAX instead of wrapping the generation counter.
bool sd_manifest_make_next(const SdManifest &current, SdManifest &next);
bool sd_manifest_valid(const SdManifest &manifest);

// Encodes one canonical UTF-8/ASCII JSON record. CRC32 covers the canonical
// JSON payload before the crc32 member. Zero means invalid input/no capacity.
size_t sd_manifest_encode(const SdManifest &manifest, char *output,
                          size_t output_capacity);
bool sd_manifest_decode(const uint8_t *bytes, size_t size,
                        SdManifest &manifest);

SdManifestSlot sd_manifest_slot_for_generation(uint64_t generation);

// Selects the newest valid generation. Both copies must describe the same
// dataset; disagreement is treated as corruption instead of guessing.
bool sd_manifest_select(const SdManifest *slot_a, const SdManifest *slot_b,
                        SdManifest &selected, SdManifestSlot &selected_slot);

}  // namespace sticky_storage
