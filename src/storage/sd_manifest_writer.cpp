#include "sd_manifest_writer.h"

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>

#include <sys/stat.h>
#include <unistd.h>

namespace sticky_storage {
namespace {

bool valid_path(const char *path)
{
    return path != nullptr && path[0] != '\0';
}

bool valid_paths(const SdManifestPaths &paths)
{
    return valid_path(paths.application_root) &&
           valid_path(paths.manifest_directory) && valid_path(paths.slot_a) &&
           valid_path(paths.slot_b) &&
           std::strcmp(paths.slot_a, paths.slot_b) != 0;
}

bool valid_ops(const SdManifestWriterOps &ops)
{
    return ops.ensure_directory != nullptr && ops.write_record != nullptr &&
           ops.read_slot != nullptr;
}

bool same_manifest(const SdManifest &left, const SdManifest &right)
{
    return left.schema_version == right.schema_version &&
           left.commit_generation == right.commit_generation &&
           std::strcmp(left.dataset_id, right.dataset_id) == 0;
}

SdManifestCommitStatus map_write_result(SdManifestWriteIoResult result)
{
    switch (result) {
    case SdManifestWriteIoResult::Ok:
        return SdManifestCommitStatus::Ok;
    case SdManifestWriteIoResult::OpenError:
        return SdManifestCommitStatus::OpenError;
    case SdManifestWriteIoResult::ShortWrite:
        return SdManifestCommitStatus::ShortWrite;
    case SdManifestWriteIoResult::SyncError:
        return SdManifestCommitStatus::SyncError;
    case SdManifestWriteIoResult::CloseError:
        return SdManifestCommitStatus::CloseError;
    }
    return SdManifestCommitStatus::IoError;
}

SdManifestProbeResult probe_with_ops(const SdManifestPaths &paths,
                                     const SdManifestWriterOps &ops)
{
    return sd_manifest_probe_slots(ops.read_slot(ops.context, paths.slot_a),
                                   ops.read_slot(ops.context, paths.slot_b));
}

SdManifestCommitStatus map_probe_failure(SdManifestProbeState state)
{
    return state == SdManifestProbeState::IoError
               ? SdManifestCommitStatus::IoError
               : SdManifestCommitStatus::Corrupt;
}

SdManifestCommitResult write_and_verify(const SdManifestPaths &paths,
                                        const SdManifest &manifest,
                                        const SdManifestWriterOps &ops)
{
    SdManifestCommitResult result = {};
    result.manifest = manifest;
    result.slot = sd_manifest_slot_for_generation(
        manifest.commit_generation);

    std::array<char, kSdManifestMaxBytes> encoded = {};
    const size_t encoded_size =
        sd_manifest_encode(manifest, encoded.data(), encoded.size());
    if (encoded_size == 0U) {
        result.status = SdManifestCommitStatus::InvalidArgument;
        return result;
    }

    const char *target = result.slot == SdManifestSlot::A ? paths.slot_a
                                                           : paths.slot_b;
    const SdManifestWriteIoResult write_result = ops.write_record(
        ops.context, target,
        reinterpret_cast<const uint8_t *>(encoded.data()), encoded_size);
    result.status = map_write_result(write_result);
    if (result.status != SdManifestCommitStatus::Ok) {
        return result;
    }

    const SdManifestSlotRead readback = ops.read_slot(ops.context, target);
    if (readback.state != SdManifestSlotState::Valid ||
        !same_manifest(readback.manifest, manifest)) {
        result.status = readback.state == SdManifestSlotState::IoError
                            ? SdManifestCommitStatus::IoError
                            : SdManifestCommitStatus::VerifyError;
        return result;
    }

    const SdManifestProbeResult committed = probe_with_ops(paths, ops);
    if ((committed.state != SdManifestProbeState::Ready &&
         committed.state != SdManifestProbeState::Degraded) ||
        committed.selected_slot != result.slot ||
        !same_manifest(committed.selected, manifest)) {
        result.status = committed.state == SdManifestProbeState::IoError
                            ? SdManifestCommitStatus::IoError
                            : SdManifestCommitStatus::VerifyError;
        return result;
    }

    result.changed = true;
    return result;
}

bool ensure_directory_default(void *, const char *path)
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

SdManifestWriteIoResult write_record_default(void *, const char *path,
                                             const uint8_t *bytes,
                                             size_t size)
{
    std::FILE *file = std::fopen(path, "wb");
    if (file == nullptr) {
        return SdManifestWriteIoResult::OpenError;
    }

    SdManifestWriteIoResult result = SdManifestWriteIoResult::Ok;
    if (std::fwrite(bytes, 1U, size, file) != size) {
        result = SdManifestWriteIoResult::ShortWrite;
    } else if (std::fflush(file) != 0 || ::fsync(fileno(file)) != 0) {
        result = SdManifestWriteIoResult::SyncError;
    }
    if (std::fclose(file) != 0 && result == SdManifestWriteIoResult::Ok) {
        result = SdManifestWriteIoResult::CloseError;
    }
    return result;
}

SdManifestSlotRead read_slot_default(void *, const char *path)
{
    return sd_manifest_read_slot(path);
}

const SdManifestWriterOps kDefaultOps = {
    nullptr,
    ensure_directory_default,
    write_record_default,
    read_slot_default,
};

}  // namespace

SdManifestCommitResult sd_manifest_initialize_with_ops(
    const SdManifestPaths &paths, const char *dataset_id,
    const SdManifestWriterOps &ops)
{
    SdManifestCommitResult result = {};
    if (!valid_paths(paths) || !valid_ops(ops) ||
        !sd_dataset_id_valid(dataset_id)) {
        return result;
    }

    const SdManifestProbeResult before = probe_with_ops(paths, ops);
    if (before.state == SdManifestProbeState::Ready ||
        before.state == SdManifestProbeState::Degraded) {
        if (before.selected.commit_generation == 1U &&
            std::strcmp(before.selected.dataset_id, dataset_id) == 0) {
            result.status = SdManifestCommitStatus::Ok;
            result.manifest = before.selected;
            result.slot = before.selected_slot;
            return result;
        }
        result.status = SdManifestCommitStatus::AlreadyInitialized;
        return result;
    }
    if (before.state != SdManifestProbeState::NotInitialized) {
        result.status = map_probe_failure(before.state);
        return result;
    }
    if (!ops.ensure_directory(ops.context, paths.application_root) ||
        !ops.ensure_directory(ops.context, paths.manifest_directory)) {
        result.status = SdManifestCommitStatus::DirectoryError;
        return result;
    }

    SdManifest first = {};
    if (!sd_manifest_make(dataset_id, 1U, first)) {
        return result;
    }
    return write_and_verify(paths, first, ops);
}

SdManifestCommitResult sd_manifest_initialize(
    const SdManifestPaths &paths, const char *dataset_id)
{
    return sd_manifest_initialize_with_ops(paths, dataset_id, kDefaultOps);
}

SdManifestCommitResult sd_manifest_commit_next_with_ops(
    const SdManifestPaths &paths, const SdManifest &next,
    const SdManifestWriterOps &ops)
{
    SdManifestCommitResult result = {};
    if (!valid_paths(paths) || !valid_ops(ops) || !sd_manifest_valid(next)) {
        return result;
    }

    const SdManifestProbeResult before = probe_with_ops(paths, ops);
    if (before.state == SdManifestProbeState::NotInitialized) {
        result.status = SdManifestCommitStatus::NotInitialized;
        return result;
    }
    if (before.state != SdManifestProbeState::Ready &&
        before.state != SdManifestProbeState::Degraded) {
        result.status = map_probe_failure(before.state);
        return result;
    }
    if (std::strcmp(before.selected.dataset_id, next.dataset_id) != 0) {
        result.status = SdManifestCommitStatus::DatasetMismatch;
        return result;
    }
    if (same_manifest(before.selected, next)) {
        result.status = SdManifestCommitStatus::Ok;
        result.manifest = before.selected;
        result.slot = before.selected_slot;
        return result;
    }
    if (before.selected.commit_generation == UINT64_MAX) {
        result.status = SdManifestCommitStatus::GenerationOverflow;
        return result;
    }
    if (next.commit_generation != before.selected.commit_generation + 1U) {
        result.status = SdManifestCommitStatus::GenerationMismatch;
        return result;
    }
    if (sd_manifest_slot_for_generation(next.commit_generation) ==
        before.selected_slot) {
        result.status = SdManifestCommitStatus::Corrupt;
        return result;
    }

    return write_and_verify(paths, next, ops);
}

SdManifestCommitResult sd_manifest_commit_next(
    const SdManifestPaths &paths, const SdManifest &next)
{
    return sd_manifest_commit_next_with_ops(paths, next, kDefaultOps);
}

}  // namespace sticky_storage
