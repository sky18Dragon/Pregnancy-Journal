#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <sys/stat.h>
#include <unistd.h>

#include "sd_journal_power_fixture.h"

int main()
{
    using namespace sticky_storage;
    constexpr char kDataset[] = "0123456789abcdef0123456789abcdef";
    static_assert(kJournalPartialFixtureFrameBytes == 344U);
    static_assert(kJournalPartialFixtureTailBytes == 172U);
    std::array<uint8_t, kSdJournalMaxRecordBytes> frame = {};
    const size_t frame_size = sd_journal_power_fixture_frame(
        kDataset, kJournalPartialTargetTransaction, frame);
    assert(frame_size == kJournalPartialFixtureFrameBytes);
    SdJournalRecordView decoded = {};
    assert(sd_journal_decode_record(kDataset, frame.data(), frame_size,
                                    decoded) == SdJournalDecodeStatus::Ok);
    assert(decoded.transaction_id == kJournalPartialTargetTransaction);
    const SdJournalScanResult partial = sd_journal_scan(
        kDataset, frame.data(), kJournalPartialFixtureTailBytes);
    assert(partial.status == SdJournalScanStatus::TailTruncated);
    assert(partial.valid_bytes == 0U);
    assert(partial.tail_bytes == kJournalPartialFixtureTailBytes);

    char temporary[] = "/tmp/sticky_power_fixture_XXXXXX";
    char *root = ::mkdtemp(temporary);
    assert(root != nullptr);

    char application[192U] = {};
    char records[192U] = {};
    char object[192U] = {};
    assert(std::snprintf(application, sizeof(application), "%s/PREGJNL",
                         root) > 0);
    assert(std::snprintf(records, sizeof(records), "%s/RECORDS",
                         application) > 0);
    assert(std::snprintf(object, sizeof(object), "%s/PWRTEST.BIN",
                         records) > 0);
    const SdJournalPowerFixturePaths paths = {application, records, object};

    SdJournalPowerFixtureResult result =
        sd_journal_power_fixture_prepare(paths);
    assert(result.status == SdJournalPowerFixtureStatus::Ok);
    assert(result.changed);
    assert(sd_journal_power_fixture_object_crc32() != 0U);

    result = sd_journal_power_fixture_prepare(paths);
    assert(result.status == SdJournalPowerFixtureStatus::Ok);
    assert(!result.changed);

    std::FILE *file = std::fopen(object, "wb");
    assert(file != nullptr);
    const uint8_t bad = 0xA5U;
    assert(std::fwrite(&bad, 1U, 1U, file) == 1U);
    assert(std::fclose(file) == 0);
    result = sd_journal_power_fixture_prepare(paths);
    assert(result.status == SdJournalPowerFixtureStatus::VerifyError);
    assert(!result.changed);

    assert(::unlink(object) == 0);
    assert(::rmdir(records) == 0);
    assert(::rmdir(application) == 0);
    assert(::rmdir(root) == 0);
    return 0;
}
