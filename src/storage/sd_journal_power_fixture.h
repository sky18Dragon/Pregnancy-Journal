#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "sd_journal.h"

namespace sticky_storage {

inline constexpr char kJournalPowerFixtureObjectFile[] = "PWRTEST.BIN";
constexpr size_t kJournalPowerFixtureObjectBytes = 256U;
constexpr uint32_t kJournalPowerFixtureMutations = 4U;
constexpr size_t kJournalPartialFixtureFrameBytes = 344U;
constexpr size_t kJournalPartialFixtureTailBytes =
    kJournalPartialFixtureFrameBytes / 2U;
constexpr uint32_t kJournalPartialBaselineRecords = 7U;
constexpr uint64_t kJournalPartialBaselineTransaction = 7U;
constexpr uint64_t kJournalPartialTargetTransaction = 8U;
constexpr uint64_t kJournalPartialBaselineBytes = 2408U;
constexpr uint64_t kJournalPartialExpectedFileBytes =
    kJournalPartialBaselineBytes + kJournalPartialFixtureTailBytes;

struct SdJournalPowerFixturePaths {
    const char *application_root = nullptr;
    const char *records_directory = nullptr;
    const char *object = nullptr;
};

enum class SdJournalPowerFixtureStatus : uint8_t {
    Ok = 0,
    InvalidArgument,
    DirectoryError,
    OpenError,
    ShortWrite,
    SyncError,
    CloseError,
    VerifyError,
};

struct SdJournalPowerFixtureResult {
    SdJournalPowerFixtureStatus status =
        SdJournalPowerFixtureStatus::InvalidArgument;
    bool changed = false;
};

void sd_journal_power_fixture_object(
    std::array<uint8_t, kJournalPowerFixtureObjectBytes> &object);
uint32_t sd_journal_power_fixture_object_crc32();
size_t sd_journal_power_fixture_frame(
    const char *dataset_id, uint64_t transaction_id,
    std::array<uint8_t, kSdJournalMaxRecordBytes> &frame);

// Creates one immutable, deterministic object used only by the isolated power
// test. An existing exact copy is accepted idempotently; an existing partial
// or different file is preserved and reported as VerifyError.
SdJournalPowerFixtureResult sd_journal_power_fixture_prepare(
    const SdJournalPowerFixturePaths &paths);

}  // namespace sticky_storage
