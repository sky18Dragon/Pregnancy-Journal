#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "kick.h"
#include "persistent_state.h"
#include "pregnancy_state.h"
#include "storage_repository.h"
#include "weight.h"

namespace pregnancy_repository {

enum class RecordCodecStatus : uint8_t {
    Ok = 0,
    InvalidArgument,
    BufferTooSmall,
    Corrupt,
    UnsupportedVersion,
};

struct EncodedPayload {
    std::array<uint8_t, sticky_storage::kRepositoryPayloadBytes> bytes = {};
    size_t size = 0U;
};

// Repository kind provides the namespace. Numeric IDs remain stable across
// rewrites and are rendered as exactly 32 lowercase hexadecimal characters.
bool pregnancy_profile_key(sticky_storage::RepositoryKey &key);
bool kick_key(uint32_t id, sticky_storage::RepositoryKey &key);
bool weight_key(uint32_t id, sticky_storage::RepositoryKey &key);
bool weight_preferences_key(sticky_storage::RepositoryKey &key);
bool device_preferences_key(sticky_storage::RepositoryKey &key);
bool numeric_record_id(const char *record_id, uint32_t &id);

RecordCodecStatus encode_pregnancy_profile(const PregnancyProfile &value,
                                            EncodedPayload &payload);
RecordCodecStatus decode_pregnancy_profile(const uint8_t *bytes, size_t size,
                                            PregnancyProfile &value);

RecordCodecStatus encode_kick(const KickSession &value, EncodedPayload &payload);
RecordCodecStatus decode_kick(const uint8_t *bytes, size_t size, KickSession &value);

RecordCodecStatus encode_weight(const WeightRecord &value,
                                EncodedPayload &payload);
RecordCodecStatus decode_weight(const uint8_t *bytes, size_t size,
                                WeightRecord &value);

RecordCodecStatus encode_weight_preferences(
    const WeightProfileSettings &value, EncodedPayload &payload);
RecordCodecStatus decode_weight_preferences(
    const uint8_t *bytes, size_t size, WeightProfileSettings &value);

RecordCodecStatus encode_device_preferences(
    const StickyDeviceSettings &value, EncodedPayload &payload);
RecordCodecStatus decode_device_preferences(
    const uint8_t *bytes, size_t size, StickyDeviceSettings &value);

}  // namespace pregnancy_repository
