#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

#include <unistd.h>

#include "sd_manifest_reader.h"

namespace {

constexpr char kDatasetA[] = "0123456789abcdef0123456789abcdef";
constexpr char kDatasetB[] = "fedcba9876543210fedcba9876543210";

sticky_storage::SdManifestSlotRead valid_slot(const char *dataset,
                                               uint64_t generation)
{
    sticky_storage::SdManifestSlotRead slot = {};
    slot.state = sticky_storage::SdManifestSlotState::Valid;
    assert(sticky_storage::sd_manifest_make(dataset, generation,
                                            slot.manifest));
    return slot;
}

void write_bytes(const std::string &path, const char *bytes, size_t size)
{
    std::FILE *file = std::fopen(path.c_str(), "wb");
    assert(file != nullptr);
    assert(std::fwrite(bytes, 1U, size, file) == size);
    assert(std::fclose(file) == 0);
}

}  // namespace

int main()
{
    using namespace sticky_storage;

    const SdManifestSlotRead missing = {};
    SdManifestProbeResult result = sd_manifest_probe_slots(missing, missing);
    assert(result.state == SdManifestProbeState::NotInitialized);

    const SdManifestSlotRead first = valid_slot(kDatasetA, 1U);
    const SdManifestSlotRead second = valid_slot(kDatasetA, 2U);
    result = sd_manifest_probe_slots(first, second);
    assert(result.state == SdManifestProbeState::Ready);
    assert(result.selected_slot == SdManifestSlot::B);
    assert(result.selected.commit_generation == 2U);

    result = sd_manifest_probe_slots(first, missing);
    assert(result.state == SdManifestProbeState::Degraded);
    assert(result.selected_slot == SdManifestSlot::A);

    const SdManifestSlotRead wrong_slot = valid_slot(kDatasetA, 2U);
    result = sd_manifest_probe_slots(wrong_slot, missing);
    assert(result.state == SdManifestProbeState::Corrupt);
    assert(result.slot_a.state == SdManifestSlotState::Invalid);

    SdManifestSlotRead invalid = {};
    invalid.state = SdManifestSlotState::Invalid;
    result = sd_manifest_probe_slots(invalid, missing);
    assert(result.state == SdManifestProbeState::Corrupt);
    result = sd_manifest_probe_slots(first, invalid);
    assert(result.state == SdManifestProbeState::Degraded);

    SdManifestSlotRead io_error = {};
    io_error.state = SdManifestSlotState::IoError;
    result = sd_manifest_probe_slots(first, io_error);
    assert(result.state == SdManifestProbeState::IoError);

    const SdManifestSlotRead conflict_a = valid_slot(kDatasetA, 3U);
    const SdManifestSlotRead conflict_b = valid_slot(kDatasetB, 4U);
    result = sd_manifest_probe_slots(conflict_a, conflict_b);
    assert(result.state == SdManifestProbeState::Corrupt);

    const std::string prefix =
        "/tmp/sticky_manifest_reader_" + std::to_string(getpid());
    const std::string slot_a_path = prefix + "_a.json";
    const std::string slot_b_path = prefix + "_b.json";
    std::remove(slot_a_path.c_str());
    std::remove(slot_b_path.c_str());
    result = sd_manifest_probe(slot_a_path.c_str(), slot_b_path.c_str());
    assert(result.state == SdManifestProbeState::NotInitialized);

    std::array<char, kSdManifestMaxBytes> encoded = {};
    const size_t encoded_size =
        sd_manifest_encode(first.manifest, encoded.data(), encoded.size());
    assert(encoded_size > 0U);
    write_bytes(slot_a_path, encoded.data(), encoded_size);
    result = sd_manifest_probe(slot_a_path.c_str(), slot_b_path.c_str());
    assert(result.state == SdManifestProbeState::Degraded);
    assert(result.selected.commit_generation == 1U);

    encoded[0] = '[';
    write_bytes(slot_b_path, encoded.data(), encoded_size);
    result = sd_manifest_probe(slot_a_path.c_str(), slot_b_path.c_str());
    assert(result.state == SdManifestProbeState::Degraded);

    std::array<char, kSdManifestMaxBytes + 1U> oversized = {};
    write_bytes(slot_a_path, oversized.data(), oversized.size());
    std::remove(slot_b_path.c_str());
    result = sd_manifest_probe(slot_a_path.c_str(), slot_b_path.c_str());
    assert(result.state == SdManifestProbeState::Corrupt);

    assert(sd_manifest_read_slot(nullptr).state ==
           SdManifestSlotState::IoError);
    std::remove(slot_a_path.c_str());
    std::remove(slot_b_path.c_str());
}
