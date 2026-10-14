#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "sd_index_snapshot.h"

namespace sticky_storage {

inline constexpr char kIndexPowerFixtureFile[] = "IDXPWR01.BIN";
inline constexpr char kIndexPowerPartialFixtureFile[] = "IDXPWR02.BIN";
constexpr size_t kIndexPowerFixtureEntries = 45U;
constexpr size_t kIndexPowerFixtureBytes =
    kSdIndexSnapshotHeaderBytes +
    kIndexPowerFixtureEntries * kSdIndexSnapshotEntryBytes;
constexpr size_t kIndexPowerPartialFixtureBytes =
    kIndexPowerFixtureBytes / 2U;

size_t sd_index_power_fixture_bytes(
    const char *dataset_id, uint64_t last_transaction_id,
    uint8_t *bytes, size_t capacity);

template <size_t Capacity>
size_t sd_index_power_fixture_bytes(
    const char *dataset_id, uint64_t last_transaction_id,
    std::array<uint8_t, Capacity> &bytes)
{
    return sd_index_power_fixture_bytes(dataset_id, last_transaction_id,
                                         bytes.data(), bytes.size());
}

}  // namespace sticky_storage
