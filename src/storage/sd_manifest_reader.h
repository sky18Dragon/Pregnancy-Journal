#pragma once

#include <cstdint>

#include "sd_manifest.h"

namespace sticky_storage {

enum class SdManifestSlotState : uint8_t {
    Missing = 0,
    Valid,
    Invalid,
    IoError,
};

enum class SdManifestProbeState : uint8_t {
    NotInitialized = 0,
    Ready,
    Degraded,
    Corrupt,
    IoError,
};

struct SdManifestSlotRead {
    SdManifestSlotState state = SdManifestSlotState::Missing;
    SdManifest manifest = {};
};

struct SdManifestProbeResult {
    SdManifestProbeState state = SdManifestProbeState::NotInitialized;
    SdManifestSlotRead slot_a = {};
    SdManifestSlotRead slot_b = {};
    SdManifest selected = {};
    SdManifestSlot selected_slot = SdManifestSlot::A;
};

// Opens and validates one manifest file without creating or modifying it.
SdManifestSlotRead sd_manifest_read_slot(const char *path);

// Reads both manifest slots and selects the newest valid copy. This operation
// is deliberately read-only; a missing dataset remains NotInitialized.
SdManifestProbeResult sd_manifest_probe(const char *slot_a_path,
                                        const char *slot_b_path);

// Pure selection helper used by host tests and future recovery code.
SdManifestProbeResult sd_manifest_probe_slots(
    const SdManifestSlotRead &slot_a, const SdManifestSlotRead &slot_b);

}  // namespace sticky_storage
