#include "sd_journal_power_fixture.h"

#include <cerrno>
#include <cstdio>
#include <cstring>

#include <sys/stat.h>
#include <unistd.h>

#include "storage_crc32.h"
#include "sd_journal_payload.h"

namespace sticky_storage {
namespace {

bool ensure_directory(const char *path)
{
    if (::mkdir(path, 0775) == 0) {
        return true;
    }
    if (errno != EEXIST) {
        return false;
    }
    struct stat info = {};
    return ::stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

bool paths_valid(const SdJournalPowerFixturePaths &paths)
{
    if (paths.application_root == nullptr ||
        paths.records_directory == nullptr || paths.object == nullptr) {
        return false;
    }
    char records[160U] = {};
    char object[160U] = {};
    const int records_written = std::snprintf(
        records, sizeof(records), "%s/RECORDS", paths.application_root);
    const int object_written = std::snprintf(
        object, sizeof(object), "%s/%s", paths.records_directory,
        kJournalPowerFixtureObjectFile);
    return records_written > 0 &&
           static_cast<size_t>(records_written) < sizeof(records) &&
           object_written > 0 &&
           static_cast<size_t>(object_written) < sizeof(object) &&
           std::strcmp(records, paths.records_directory) == 0 &&
           std::strcmp(object, paths.object) == 0;
}

bool read_exact_object(
    const char *path,
    const std::array<uint8_t, kJournalPowerFixtureObjectBytes> &expected,
    bool &exists)
{
    exists = false;
    std::FILE *file = std::fopen(path, "rb");
    if (file == nullptr) {
        return false;
    }
    exists = true;
    std::array<uint8_t, kJournalPowerFixtureObjectBytes + 1U> bytes = {};
    const size_t count = std::fread(bytes.data(), 1U, bytes.size(), file);
    const bool read_ok = std::ferror(file) == 0;
    const bool close_ok = std::fclose(file) == 0;
    return read_ok && close_ok && count == expected.size() &&
           std::memcmp(bytes.data(), expected.data(), expected.size()) == 0;
}

}  // namespace

void sd_journal_power_fixture_object(
    std::array<uint8_t, kJournalPowerFixtureObjectBytes> &object)
{
    object.fill(0U);
    constexpr uint8_t header[] = {'P', 'J', 'P', 'T', 1U, 0U, 0U, 0U};
    std::memcpy(object.data(), header, sizeof(header));
    for (size_t index = sizeof(header); index < object.size(); ++index) {
        object[index] = static_cast<uint8_t>((index * 37U + 11U) & 0xFFU);
    }
}

uint32_t sd_journal_power_fixture_object_crc32()
{
    std::array<uint8_t, kJournalPowerFixtureObjectBytes> object = {};
    sd_journal_power_fixture_object(object);
    return crc32(object.data(), object.size());
}

size_t sd_journal_power_fixture_frame(
    const char *dataset_id, uint64_t transaction_id,
    std::array<uint8_t, kSdJournalMaxRecordBytes> &frame)
{
    frame.fill(0U);
    if (!sd_dataset_id_valid(dataset_id) || transaction_id == 0U) {
        return 0U;
    }
    SdJournalTransactionPayload transaction = {};
    transaction.mutation_count = kJournalPowerFixtureMutations;
    const uint32_t object_crc32 =
        sd_journal_power_fixture_object_crc32();
    for (uint32_t index = 0U; index < kJournalPowerFixtureMutations;
         ++index) {
        char record_id[kRepositoryRecordIdCharacters + 1U] = {};
        std::snprintf(record_id, sizeof(record_id), "%016llx%016llx",
                      static_cast<unsigned long long>(
                          0x8000000000000000ULL + index),
                      static_cast<unsigned long long>(transaction_id));
        SdJournalMutationReference &mutation =
            transaction.mutations[index];
        if (!repository_key_make(RepositoryKind::Note, record_id,
                                 mutation.key)) {
            return 0U;
        }
        mutation.type = RepositoryMutationType::Upsert;
        mutation.expected_revision = 0U;
        mutation.committed_revision = 1U;
        mutation.object_bytes = kJournalPowerFixtureObjectBytes;
        mutation.object_crc32 = object_crc32;
        std::strcpy(mutation.object_file,
                    kJournalPowerFixtureObjectFile);
    }
    std::array<uint8_t, kSdJournalMaxPayloadBytes> payload = {};
    const size_t payload_size = sd_journal_encode_transaction_payload(
        transaction, payload.data(), payload.size());
    if (payload_size == 0U) {
        return 0U;
    }
    return sd_journal_encode_record(
        SdJournalRecordType::Commit, dataset_id, transaction_id,
        payload.data(), payload_size, frame.data(), frame.size());
}

SdJournalPowerFixtureResult sd_journal_power_fixture_prepare(
    const SdJournalPowerFixturePaths &paths)
{
    SdJournalPowerFixtureResult result = {};
    if (!paths_valid(paths)) {
        return result;
    }

    std::array<uint8_t, kJournalPowerFixtureObjectBytes> object = {};
    sd_journal_power_fixture_object(object);
    bool exists = false;
    if (read_exact_object(paths.object, object, exists)) {
        result.status = SdJournalPowerFixtureStatus::Ok;
        return result;
    }
    if (exists) {
        result.status = SdJournalPowerFixtureStatus::VerifyError;
        return result;
    }
    if (!ensure_directory(paths.application_root) ||
        !ensure_directory(paths.records_directory)) {
        result.status = SdJournalPowerFixtureStatus::DirectoryError;
        return result;
    }

    std::FILE *file = std::fopen(paths.object, "wb");
    if (file == nullptr) {
        result.status = SdJournalPowerFixtureStatus::OpenError;
        return result;
    }
    result.status = SdJournalPowerFixtureStatus::Ok;
    if (std::fwrite(object.data(), 1U, object.size(), file) != object.size()) {
        result.status = SdJournalPowerFixtureStatus::ShortWrite;
    } else if (std::fflush(file) != 0 || ::fsync(fileno(file)) != 0) {
        result.status = SdJournalPowerFixtureStatus::SyncError;
    }
    if (std::fclose(file) != 0 &&
        result.status == SdJournalPowerFixtureStatus::Ok) {
        result.status = SdJournalPowerFixtureStatus::CloseError;
    }
    if (result.status != SdJournalPowerFixtureStatus::Ok) {
        return result;
    }

    exists = false;
    if (!read_exact_object(paths.object, object, exists) || !exists) {
        result.status = SdJournalPowerFixtureStatus::VerifyError;
        return result;
    }
    result.changed = true;
    return result;
}

}  // namespace sticky_storage
