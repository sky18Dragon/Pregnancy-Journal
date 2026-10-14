#include "sd_manifest_request.h"

namespace sticky_storage {
namespace {

SdManifestRequestStatus map_card_state(SdCardState state)
{
    switch (state) {
    case SdCardState::Absent:
    case SdCardState::Unmounted:
        return SdManifestRequestStatus::CardMissing;
    case SdCardState::ReadyReadOnly:
        return SdManifestRequestStatus::CardReadOnly;
    case SdCardState::Full:
        return SdManifestRequestStatus::NoSpace;
    case SdCardState::Mounting:
    case SdCardState::Fault:
    case SdCardState::Ejecting:
        return SdManifestRequestStatus::CardUnavailable;
    case SdCardState::ReadyReadWrite:
        return SdManifestRequestStatus::Ok;
    }
    return SdManifestRequestStatus::CardUnavailable;
}

SdManifestRequestStatus map_commit_status(SdManifestCommitStatus status)
{
    switch (status) {
    case SdManifestCommitStatus::Ok:
        return SdManifestRequestStatus::Ok;
    case SdManifestCommitStatus::InvalidArgument:
        return SdManifestRequestStatus::InvalidArgument;
    case SdManifestCommitStatus::AlreadyInitialized:
        return SdManifestRequestStatus::AlreadyInitialized;
    case SdManifestCommitStatus::NotInitialized:
        return SdManifestRequestStatus::NotInitialized;
    case SdManifestCommitStatus::DatasetMismatch:
        return SdManifestRequestStatus::DatasetMismatch;
    case SdManifestCommitStatus::GenerationMismatch:
        return SdManifestRequestStatus::GenerationMismatch;
    case SdManifestCommitStatus::GenerationOverflow:
        return SdManifestRequestStatus::GenerationOverflow;
    case SdManifestCommitStatus::Corrupt:
        return SdManifestRequestStatus::Corrupt;
    case SdManifestCommitStatus::IoError:
        return SdManifestRequestStatus::IoError;
    case SdManifestCommitStatus::DirectoryError:
        return SdManifestRequestStatus::DirectoryError;
    case SdManifestCommitStatus::OpenError:
        return SdManifestRequestStatus::OpenError;
    case SdManifestCommitStatus::ShortWrite:
        return SdManifestRequestStatus::ShortWrite;
    case SdManifestCommitStatus::SyncError:
        return SdManifestRequestStatus::SyncError;
    case SdManifestCommitStatus::CloseError:
        return SdManifestRequestStatus::CloseError;
    case SdManifestCommitStatus::VerifyError:
        return SdManifestRequestStatus::VerifyError;
    }
    return SdManifestRequestStatus::IoError;
}

SdManifestRequestResult map_result(const SdManifestCommitResult &commit)
{
    SdManifestRequestResult result = {};
    result.status = map_commit_status(commit.status);
    result.changed = commit.changed;
    result.manifest = commit.manifest;
    result.slot = commit.slot;
    return result;
}

bool valid_request(const SdManifestRequest &request)
{
    if (request.mount_generation == 0U ||
        !sd_manifest_valid(request.manifest)) {
        return false;
    }
    if (request.operation == SdManifestRequestOperation::Initialize) {
        return request.manifest.commit_generation == 1U;
    }
    return request.operation == SdManifestRequestOperation::CommitNext;
}

}  // namespace

bool sd_manifest_make_initialize_request(uint32_t mount_generation,
                                         const char *dataset_id,
                                         bool user_confirmed,
                                         SdManifestRequest &request)
{
    request = {};
    if (mount_generation == 0U ||
        !sd_manifest_make(dataset_id, 1U, request.manifest)) {
        return false;
    }
    request.operation = SdManifestRequestOperation::Initialize;
    request.mount_generation = mount_generation;
    request.user_confirmed = user_confirmed;
    return true;
}

bool sd_manifest_make_commit_request(uint32_t mount_generation,
                                     const SdManifest &manifest,
                                     SdManifestRequest &request)
{
    request = {};
    if (mount_generation == 0U || !sd_manifest_valid(manifest)) {
        return false;
    }
    request.operation = SdManifestRequestOperation::CommitNext;
    request.mount_generation = mount_generation;
    request.manifest = manifest;
    return true;
}

SdManifestRequestResult sd_manifest_execute_request_with_ops(
    const SdManifestRequest &request,
    const SdManifestRequestContext &context, const SdManifestPaths &paths,
    const SdManifestWriterOps &ops)
{
    SdManifestRequestResult result = {};
    if (!valid_request(request)) {
        return result;
    }
    if (request.operation == SdManifestRequestOperation::Initialize &&
        !request.user_confirmed) {
        result.status = SdManifestRequestStatus::ConfirmationRequired;
        return result;
    }
    if (request.mount_generation != context.mount_generation) {
        result.status = SdManifestRequestStatus::StaleGeneration;
        return result;
    }

    result.status = map_card_state(context.card_state);
    if (result.status != SdManifestRequestStatus::Ok) {
        return result;
    }

    if (request.operation == SdManifestRequestOperation::Initialize) {
        return map_result(sd_manifest_initialize_with_ops(
            paths, request.manifest.dataset_id, ops));
    }
    return map_result(
        sd_manifest_commit_next_with_ops(paths, request.manifest, ops));
}

SdManifestRequestResult sd_manifest_execute_request(
    const SdManifestRequest &request,
    const SdManifestRequestContext &context,
    const SdManifestPaths &paths)
{
    if (!valid_request(request)) {
        return {};
    }
    if (request.operation == SdManifestRequestOperation::Initialize &&
        !request.user_confirmed) {
        SdManifestRequestResult result = {};
        result.status = SdManifestRequestStatus::ConfirmationRequired;
        return result;
    }
    if (request.mount_generation != context.mount_generation) {
        SdManifestRequestResult result = {};
        result.status = SdManifestRequestStatus::StaleGeneration;
        return result;
    }
    SdManifestRequestResult result = {};
    result.status = map_card_state(context.card_state);
    if (result.status != SdManifestRequestStatus::Ok) {
        return result;
    }

    if (request.operation == SdManifestRequestOperation::Initialize) {
        return map_result(sd_manifest_initialize(
            paths, request.manifest.dataset_id));
    }
    return map_result(sd_manifest_commit_next(paths, request.manifest));
}

}  // namespace sticky_storage
