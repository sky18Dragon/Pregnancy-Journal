#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>

#include <unistd.h>

#include "sd_record_object_file.h"
#include "storage_crc32.h"

namespace {

struct FakeObject {
    sticky_storage::SdRecordObjectProbeStatus before =
        sticky_storage::SdRecordObjectProbeStatus::Missing;
    sticky_storage::SdRecordObjectProbeStatus after =
        sticky_storage::SdRecordObjectProbeStatus::Match;
    sticky_storage::SdRecordObjectWriteIoResult write =
        sticky_storage::SdRecordObjectWriteIoResult::Ok;
    bool directories = true;
    unsigned probes = 0U;
    unsigned writes = 0U;
};

bool ensure_directory(void *context, const char *)
{
    return static_cast<FakeObject *>(context)->directories;
}

sticky_storage::SdRecordObjectWriteIoResult write_object(
    void *context, const char *, const uint8_t *, size_t)
{
    auto &fake = *static_cast<FakeObject *>(context);
    ++fake.writes;
    return fake.write;
}

sticky_storage::SdRecordObjectProbe probe_object(
    void *context, const char *, const uint8_t *, size_t size)
{
    auto &fake = *static_cast<FakeObject *>(context);
    sticky_storage::SdRecordObjectProbe probe = {};
    probe.status = fake.probes++ == 0U ? fake.before : fake.after;
    probe.file_bytes = probe.status ==
                               sticky_storage::SdRecordObjectProbeStatus::Match
                           ? size
                           : 0U;
    return probe;
}

}  // namespace

int main()
{
    using namespace sticky_storage;
    constexpr char kApplication[] = "/card/PREGJNL";
    constexpr char kRecords[] = "/card/PREGJNL/RECORDS";
    constexpr char kObject[] = "/card/PREGJNL/RECORDS/OBJ00001.BIN";
    const SdRecordObjectPaths paths = {kApplication, kRecords, kObject};
    const std::array<uint8_t, 8U> bytes = {0U, 1U, 2U, 3U,
                                           4U, 5U, 6U, 7U};

    FakeObject fake = {};
    SdRecordObjectFileOps ops = {
        &fake, ensure_directory, write_object, probe_object};
    SdRecordObjectWriteResult result = sd_record_object_write_with_ops(
        paths, bytes.data(), bytes.size(), ops);
    assert(result.status == SdRecordObjectWriteStatus::Ok);
    assert(result.changed && fake.writes == 1U && fake.probes == 2U);

    fake = {};
    fake.before = SdRecordObjectProbeStatus::Match;
    result = sd_record_object_write_with_ops(
        paths, bytes.data(), bytes.size(), ops);
    assert(result.status == SdRecordObjectWriteStatus::Ok);
    assert(!result.changed && fake.writes == 0U);

    fake = {};
    fake.before = SdRecordObjectProbeStatus::Mismatch;
    result = sd_record_object_write_with_ops(
        paths, bytes.data(), bytes.size(), ops);
    assert(result.status == SdRecordObjectWriteStatus::ExistingConflict);
    assert(fake.writes == 0U);

    fake = {};
    fake.write = SdRecordObjectWriteIoResult::ShortWrite;
    result = sd_record_object_write_with_ops(
        paths, bytes.data(), bytes.size(), ops);
    assert(result.status == SdRecordObjectWriteStatus::ShortWrite);
    assert(fake.probes == 1U);

    fake = {};
    fake.after = SdRecordObjectProbeStatus::Mismatch;
    result = sd_record_object_write_with_ops(
        paths, bytes.data(), bytes.size(), ops);
    assert(result.status == SdRecordObjectWriteStatus::VerifyError);

    fake = {};
    fake.directories = false;
    result = sd_record_object_write_with_ops(
        paths, bytes.data(), bytes.size(), ops);
    assert(result.status == SdRecordObjectWriteStatus::DirectoryError);

    assert(sd_record_object_write_with_ops(
               paths, nullptr, bytes.size(), ops).status ==
           SdRecordObjectWriteStatus::InvalidArgument);
    const SdRecordObjectPaths bad_path = {
        kApplication, kRecords, "/card/PREGJNL/OTHER/OBJ00001.BIN"};
    assert(sd_record_object_write_with_ops(
               bad_path, bytes.data(), bytes.size(), ops).status ==
           SdRecordObjectWriteStatus::InvalidArgument);

    char temporary[] = "/tmp/sticky_record_object_XXXXXX";
    char *root = ::mkdtemp(temporary);
    assert(root != nullptr);
    char application[192U] = {};
    char records[192U] = {};
    char object[192U] = {};
    assert(std::snprintf(application, sizeof(application), "%s/PREGJNL",
                         root) > 0);
    assert(std::snprintf(records, sizeof(records), "%s/RECORDS",
                         application) > 0);
    assert(std::snprintf(object, sizeof(object), "%s/OBJ00001.BIN",
                         records) > 0);
    const SdRecordObjectPaths real_paths = {application, records, object};
    result = sd_record_object_write(real_paths, bytes.data(), bytes.size());
    assert(result.status == SdRecordObjectWriteStatus::Ok && result.changed);
    assert(result.probe.file_bytes == bytes.size());
    assert(result.probe.file_crc32 == crc32(bytes.data(), bytes.size()));
    result = sd_record_object_write(real_paths, bytes.data(), bytes.size());
    assert(result.status == SdRecordObjectWriteStatus::Ok && !result.changed);

    std::array<uint8_t, 8U> readback = {};
    SdRecordObjectReadResult read = sd_record_object_read_reference(
        object, bytes.size(), crc32(bytes.data(), bytes.size()),
        readback.data(), readback.size());
    assert(read.status == SdRecordObjectReadStatus::Ok);
    assert(read.bytes == bytes.size() && readback == bytes);
    read = sd_record_object_read_reference(
        object, bytes.size(), crc32(bytes.data(), bytes.size()) ^ 1U,
        readback.data(), readback.size());
    assert(read.status == SdRecordObjectReadStatus::CrcMismatch);
    read = sd_record_object_read_reference(
        object, bytes.size() - 1U, crc32(bytes.data(), bytes.size() - 1U),
        readback.data(), readback.size());
    assert(read.status == SdRecordObjectReadStatus::SizeMismatch);
    read = sd_record_object_read_reference(
        "/tmp/DOES-NOT-EXIST.BIN", bytes.size(),
        crc32(bytes.data(), bytes.size()), readback.data(), readback.size());
    assert(read.status == SdRecordObjectReadStatus::Missing);

    std::FILE *file = std::fopen(object, "wb");
    assert(file != nullptr);
    assert(std::fwrite(bytes.data(), 1U, 3U, file) == 3U);
    assert(std::fclose(file) == 0);
    const SdRecordObjectProbe partial = sd_record_object_probe(
        object, bytes.data(), bytes.size());
    assert(partial.status == SdRecordObjectProbeStatus::Mismatch);
    result = sd_record_object_write(real_paths, bytes.data(), bytes.size());
    assert(result.status == SdRecordObjectWriteStatus::ExistingConflict);

    assert(::unlink(object) == 0);
    assert(::rmdir(records) == 0);
    assert(::rmdir(application) == 0);
    assert(::rmdir(root) == 0);
    return 0;
}
