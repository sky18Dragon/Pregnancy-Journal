#include "sd_manifest_reader.h"

#include <array>
#include <cerrno>
#include <cstdio>

namespace sticky_storage {
namespace {

bool slot_valid(const SdManifestSlotRead &slot)
{
    return slot.state == SdManifestSlotState::Valid;
}

}  // namespace

SdManifestSlotRead sd_manifest_read_slot(const char *path)
{
    SdManifestSlotRead result = {};
    if (path == nullptr || path[0] == '\0') {
        result.state = SdManifestSlotState::IoError;
        return result;
    }

    errno = 0;
    std::FILE *file = std::fopen(path, "rb");
    if (file == nullptr) {
        result.state = errno == ENOENT ? SdManifestSlotState::Missing
                                       : SdManifestSlotState::IoError;
        return result;
    }

    std::array<uint8_t, kSdManifestMaxBytes> bytes = {};
    const size_t size = std::fread(bytes.data(), 1U, bytes.size(), file);
    const bool oversized = size == bytes.size() && std::fgetc(file) != EOF;
    const bool read_error = std::ferror(file) != 0;
    const bool close_error = std::fclose(file) != 0;
    if (read_error || close_error) {
        result.state = SdManifestSlotState::IoError;
        return result;
    }
    if (oversized || !sd_manifest_decode(bytes.data(), size,
                                         result.manifest)) {
        result.state = SdManifestSlotState::Invalid;
        result.manifest = {};
        return result;
    }

    result.state = SdManifestSlotState::Valid;
    return result;
}

SdManifestProbeResult sd_manifest_probe_slots(
    const SdManifestSlotRead &slot_a, const SdManifestSlotRead &slot_b)
{
    SdManifestProbeResult result = {};
    result.slot_a = slot_a;
    result.slot_b = slot_b;

    if (slot_valid(result.slot_a) &&
        sd_manifest_slot_for_generation(
            result.slot_a.manifest.commit_generation) != SdManifestSlot::A) {
        result.slot_a.state = SdManifestSlotState::Invalid;
        result.slot_a.manifest = {};
    }
    if (slot_valid(result.slot_b) &&
        sd_manifest_slot_for_generation(
            result.slot_b.manifest.commit_generation) != SdManifestSlot::B) {
        result.slot_b.state = SdManifestSlotState::Invalid;
        result.slot_b.manifest = {};
    }

    if (result.slot_a.state == SdManifestSlotState::IoError ||
        result.slot_b.state == SdManifestSlotState::IoError) {
        result.state = SdManifestProbeState::IoError;
        return result;
    }
    if (result.slot_a.state == SdManifestSlotState::Missing &&
        result.slot_b.state == SdManifestSlotState::Missing) {
        result.state = SdManifestProbeState::NotInitialized;
        return result;
    }

    const SdManifest *manifest_a = slot_valid(result.slot_a)
                                       ? &result.slot_a.manifest
                                       : nullptr;
    const SdManifest *manifest_b = slot_valid(result.slot_b)
                                       ? &result.slot_b.manifest
                                       : nullptr;
    if (!sd_manifest_select(manifest_a, manifest_b, result.selected,
                            result.selected_slot)) {
        result.state = SdManifestProbeState::Corrupt;
        return result;
    }

    result.state = slot_valid(result.slot_a) && slot_valid(result.slot_b)
                       ? SdManifestProbeState::Ready
                       : SdManifestProbeState::Degraded;
    return result;
}

SdManifestProbeResult sd_manifest_probe(const char *slot_a_path,
                                        const char *slot_b_path)
{
    return sd_manifest_probe_slots(sd_manifest_read_slot(slot_a_path),
                                   sd_manifest_read_slot(slot_b_path));
}

}  // namespace sticky_storage
