#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

#include <sys/stat.h>
#include <unistd.h>

#include "sd_manifest_writer.h"

namespace {

constexpr char kDatasetA[] = "0123456789abcdef0123456789abcdef";
constexpr char kDatasetB[] = "fedcba9876543210fedcba9876543210";

struct FakeFilesystem {
    sticky_storage::SdManifestSlotRead slot_a = {};
    sticky_storage::SdManifestSlotRead slot_b = {};
    sticky_storage::SdManifestWriteIoResult write_result =
        sticky_storage::SdManifestWriteIoResult::Ok;
    bool directory_ok = true;
    bool corrupt_after_write = false;
    unsigned directory_calls = 0U;
    unsigned write_calls = 0U;
    const char *last_write_path = nullptr;
};

sticky_storage::SdManifestSlotRead valid_slot(const char *dataset,
                                               uint64_t generation)
{
    sticky_storage::SdManifestSlotRead slot = {};
    slot.state = sticky_storage::SdManifestSlotState::Valid;
    assert(sticky_storage::sd_manifest_make(dataset, generation,
                                            slot.manifest));
    return slot;
}

bool fake_ensure_directory(void *context, const char *)
{
    auto &filesystem = *static_cast<FakeFilesystem *>(context);
    ++filesystem.directory_calls;
    return filesystem.directory_ok;
}

sticky_storage::SdManifestWriteIoResult fake_write_record(
    void *context, const char *path, const uint8_t *bytes, size_t size)
{
    using namespace sticky_storage;
    auto &filesystem = *static_cast<FakeFilesystem *>(context);
    ++filesystem.write_calls;
    filesystem.last_write_path = path;

    SdManifestSlotRead &target = std::strcmp(path, "A") == 0
                                     ? filesystem.slot_a
                                     : filesystem.slot_b;
    if (filesystem.write_result != SdManifestWriteIoResult::Ok) {
        if (filesystem.write_result == SdManifestWriteIoResult::ShortWrite) {
            target = {};
            target.state = SdManifestSlotState::Invalid;
        }
        return filesystem.write_result;
    }

    SdManifest decoded = {};
    assert(sd_manifest_decode(bytes, size, decoded));
    target.state = filesystem.corrupt_after_write
                       ? SdManifestSlotState::Invalid
                       : SdManifestSlotState::Valid;
    target.manifest = filesystem.corrupt_after_write ? SdManifest{} : decoded;
    return SdManifestWriteIoResult::Ok;
}

sticky_storage::SdManifestSlotRead fake_read_slot(void *context,
                                                   const char *path)
{
    auto &filesystem = *static_cast<FakeFilesystem *>(context);
    return std::strcmp(path, "A") == 0 ? filesystem.slot_a
                                        : filesystem.slot_b;
}

sticky_storage::SdManifestWriterOps fake_ops(FakeFilesystem &filesystem)
{
    return {
        &filesystem,
        fake_ensure_directory,
        fake_write_record,
        fake_read_slot,
    };
}

sticky_storage::SdManifestPaths fake_paths()
{
    return {"ROOT", "MANIFEST", "A", "B"};
}

sticky_storage::SdManifest make_manifest(const char *dataset,
                                         uint64_t generation)
{
    sticky_storage::SdManifest manifest = {};
    assert(sticky_storage::sd_manifest_make(dataset, generation, manifest));
    return manifest;
}

void remove_tree_files(const std::string &prefix, const std::string &root,
                       const std::string &manifest_directory,
                       const std::string &slot_a, const std::string &slot_b)
{
    std::remove(slot_a.c_str());
    std::remove(slot_b.c_str());
    ::rmdir(manifest_directory.c_str());
    ::rmdir(root.c_str());
    ::rmdir(prefix.c_str());
}

}  // namespace

int main()
{
    using namespace sticky_storage;

    const SdManifestPaths paths = fake_paths();
    FakeFilesystem filesystem = {};
    SdManifestWriterOps ops = fake_ops(filesystem);

    SdManifestCommitResult result =
        sd_manifest_initialize_with_ops(paths, "bad", ops);
    assert(result.status == SdManifestCommitStatus::InvalidArgument);
    assert(filesystem.write_calls == 0U);

    result = sd_manifest_initialize_with_ops(paths, kDatasetA, ops);
    assert(result.status == SdManifestCommitStatus::Ok && result.changed);
    assert(result.slot == SdManifestSlot::A);
    assert(result.manifest.commit_generation == 1U);
    assert(filesystem.directory_calls == 2U);
    assert(filesystem.write_calls == 1U);
    assert(std::strcmp(filesystem.last_write_path, "A") == 0);

    result = sd_manifest_initialize_with_ops(paths, kDatasetA, ops);
    assert(result.status == SdManifestCommitStatus::Ok && !result.changed);
    assert(filesystem.directory_calls == 2U);
    assert(filesystem.write_calls == 1U);
    result = sd_manifest_initialize_with_ops(paths, kDatasetB, ops);
    assert(result.status == SdManifestCommitStatus::AlreadyInitialized);

    SdManifest second = make_manifest(kDatasetA, 2U);
    result = sd_manifest_commit_next_with_ops(paths, second, ops);
    assert(result.status == SdManifestCommitStatus::Ok && result.changed);
    assert(result.slot == SdManifestSlot::B);
    assert(std::strcmp(filesystem.last_write_path, "B") == 0);
    result = sd_manifest_commit_next_with_ops(paths, second, ops);
    assert(result.status == SdManifestCommitStatus::Ok && !result.changed);

    SdManifest fourth = make_manifest(kDatasetA, 4U);
    result = sd_manifest_commit_next_with_ops(paths, fourth, ops);
    assert(result.status == SdManifestCommitStatus::GenerationMismatch);
    SdManifest other_dataset = make_manifest(kDatasetB, 3U);
    result = sd_manifest_commit_next_with_ops(paths, other_dataset, ops);
    assert(result.status == SdManifestCommitStatus::DatasetMismatch);

    SdManifest third = make_manifest(kDatasetA, 3U);
    result = sd_manifest_commit_next_with_ops(paths, third, ops);
    assert(result.status == SdManifestCommitStatus::Ok && result.changed);
    assert(result.slot == SdManifestSlot::A);

    FakeFilesystem not_initialized = {};
    result = sd_manifest_commit_next_with_ops(
        paths, make_manifest(kDatasetA, 1U), fake_ops(not_initialized));
    assert(result.status == SdManifestCommitStatus::NotInitialized);

    FakeFilesystem corrupt = {};
    corrupt.slot_a.state = SdManifestSlotState::Invalid;
    result = sd_manifest_initialize_with_ops(paths, kDatasetA,
                                             fake_ops(corrupt));
    assert(result.status == SdManifestCommitStatus::Corrupt);
    assert(corrupt.write_calls == 0U);

    FakeFilesystem io_error = {};
    io_error.slot_a.state = SdManifestSlotState::IoError;
    result = sd_manifest_initialize_with_ops(paths, kDatasetA,
                                             fake_ops(io_error));
    assert(result.status == SdManifestCommitStatus::IoError);

    FakeFilesystem directory_error = {};
    directory_error.directory_ok = false;
    result = sd_manifest_initialize_with_ops(paths, kDatasetA,
                                             fake_ops(directory_error));
    assert(result.status == SdManifestCommitStatus::DirectoryError);
    assert(directory_error.write_calls == 0U);

    for (SdManifestWriteIoResult failure : {
             SdManifestWriteIoResult::OpenError,
             SdManifestWriteIoResult::ShortWrite,
             SdManifestWriteIoResult::SyncError,
             SdManifestWriteIoResult::CloseError,
         }) {
        FakeFilesystem failing = {};
        failing.slot_a = valid_slot(kDatasetA, 1U);
        failing.write_result = failure;
        result = sd_manifest_commit_next_with_ops(
            paths, make_manifest(kDatasetA, 2U), fake_ops(failing));
        assert(result.status != SdManifestCommitStatus::Ok);
        assert(failing.slot_a.state == SdManifestSlotState::Valid);
        assert(failing.slot_a.manifest.commit_generation == 1U);
    }

    FakeFilesystem verify_error = {};
    verify_error.slot_a = valid_slot(kDatasetA, 1U);
    verify_error.corrupt_after_write = true;
    result = sd_manifest_commit_next_with_ops(
        paths, make_manifest(kDatasetA, 2U), fake_ops(verify_error));
    assert(result.status == SdManifestCommitStatus::VerifyError);
    assert(verify_error.slot_a.state == SdManifestSlotState::Valid);

    FakeFilesystem overflow = {};
    overflow.slot_a = valid_slot(kDatasetA, UINT64_MAX);
    result = sd_manifest_commit_next_with_ops(
        paths, make_manifest(kDatasetA, 1U), fake_ops(overflow));
    assert(result.status == SdManifestCommitStatus::GenerationOverflow);

    const std::string prefix =
        "/tmp/sticky_manifest_writer_" + std::to_string(getpid());
    const std::string root = prefix + "/PREGJNL";
    const std::string manifest_directory = root + "/MANIFEST";
    const std::string slot_a = manifest_directory + "/MNF000A.JSN";
    const std::string slot_b = manifest_directory + "/MNF000B.JSN";
    remove_tree_files(prefix, root, manifest_directory, slot_a, slot_b);
    assert(::mkdir(prefix.c_str(), 0775) == 0);
    const SdManifestPaths real_paths = {
        root.c_str(), manifest_directory.c_str(), slot_a.c_str(),
        slot_b.c_str(),
    };
    result = sd_manifest_initialize(real_paths, kDatasetA);
    assert(result.status == SdManifestCommitStatus::Ok && result.changed);
    SdManifestProbeResult probe = sd_manifest_probe(slot_a.c_str(),
                                                   slot_b.c_str());
    assert(probe.state == SdManifestProbeState::Degraded);
    result = sd_manifest_commit_next(real_paths,
                                     make_manifest(kDatasetA, 2U));
    assert(result.status == SdManifestCommitStatus::Ok && result.changed);
    probe = sd_manifest_probe(slot_a.c_str(), slot_b.c_str());
    assert(probe.state == SdManifestProbeState::Ready);
    assert(probe.selected.commit_generation == 2U);
    remove_tree_files(prefix, root, manifest_directory, slot_a, slot_b);
}
