#include <cassert>
#include <cstring>
#include <utility>

#include "sd_manifest_request.h"

namespace {

using namespace sticky_storage;

constexpr char kDataset[] = "0123456789abcdef0123456789abcdef";

struct FakeFilesystem {
    SdManifestSlotRead slot_a = {};
    SdManifestSlotRead slot_b = {};
    SdManifestWriteIoResult write_result = SdManifestWriteIoResult::Ok;
    uint32_t directory_calls = 0U;
    uint32_t write_calls = 0U;
};

bool ensure_directory(void *context, const char *)
{
    ++static_cast<FakeFilesystem *>(context)->directory_calls;
    return true;
}

SdManifestWriteIoResult write_record(void *context, const char *path,
                                     const uint8_t *bytes, size_t size)
{
    auto &filesystem = *static_cast<FakeFilesystem *>(context);
    ++filesystem.write_calls;
    if (filesystem.write_result != SdManifestWriteIoResult::Ok) {
        return filesystem.write_result;
    }
    SdManifest decoded = {};
    assert(sd_manifest_decode(bytes, size, decoded));
    SdManifestSlotRead &slot = std::strcmp(path, "A") == 0
                                   ? filesystem.slot_a
                                   : filesystem.slot_b;
    slot.state = SdManifestSlotState::Valid;
    slot.manifest = decoded;
    return SdManifestWriteIoResult::Ok;
}

SdManifestSlotRead read_slot(void *context, const char *path)
{
    auto &filesystem = *static_cast<FakeFilesystem *>(context);
    return std::strcmp(path, "A") == 0 ? filesystem.slot_a
                                       : filesystem.slot_b;
}

SdManifestWriterOps fake_ops(FakeFilesystem &filesystem)
{
    return {&filesystem, ensure_directory, write_record, read_slot};
}

const SdManifestPaths kPaths = {"ROOT", "MANIFEST", "A", "B"};

SdManifest make_manifest(uint64_t generation)
{
    SdManifest manifest = {};
    assert(sd_manifest_make(kDataset, generation, manifest));
    return manifest;
}

}  // namespace

int main()
{
    using namespace sticky_storage;

    SdManifestRequest request = {};
    assert(!sd_manifest_make_initialize_request(0U, kDataset, true,
                                                request));
    assert(!sd_manifest_make_initialize_request(1U, "bad", true,
                                                request));
    assert(sd_manifest_make_initialize_request(7U, kDataset, false,
                                               request));

    FakeFilesystem filesystem = {};
    const SdManifestWriterOps ops = fake_ops(filesystem);
    SdManifestRequestContext context = {SdCardState::ReadyReadWrite, 7U};
    SdManifestRequestResult result = sd_manifest_execute_request_with_ops(
        request, context, kPaths, ops);
    assert(result.status == SdManifestRequestStatus::ConfirmationRequired);
    assert(filesystem.write_calls == 0U);

    request.user_confirmed = true;
    context.mount_generation = 8U;
    result = sd_manifest_execute_request_with_ops(request, context, kPaths,
                                                  ops);
    assert(result.status == SdManifestRequestStatus::StaleGeneration);

    context.mount_generation = 7U;
    for (const auto &test : {
             std::pair{SdCardState::Absent,
                       SdManifestRequestStatus::CardMissing},
             std::pair{SdCardState::Unmounted,
                       SdManifestRequestStatus::CardMissing},
             std::pair{SdCardState::ReadyReadOnly,
                       SdManifestRequestStatus::CardReadOnly},
             std::pair{SdCardState::Full,
                       SdManifestRequestStatus::NoSpace},
             std::pair{SdCardState::Fault,
                       SdManifestRequestStatus::CardUnavailable},
             std::pair{SdCardState::Ejecting,
                       SdManifestRequestStatus::CardUnavailable},
         }) {
        context.card_state = test.first;
        result = sd_manifest_execute_request_with_ops(request, context,
                                                      kPaths, ops);
        assert(result.status == test.second);
        assert(filesystem.write_calls == 0U);
    }

    context.card_state = SdCardState::ReadyReadWrite;
    result = sd_manifest_execute_request_with_ops(request, context, kPaths,
                                                  ops);
    assert(result.status == SdManifestRequestStatus::Ok && result.changed);
    assert(result.manifest.commit_generation == 1U);
    assert(filesystem.directory_calls == 2U && filesystem.write_calls == 1U);

    result = sd_manifest_execute_request_with_ops(request, context, kPaths,
                                                  ops);
    assert(result.status == SdManifestRequestStatus::Ok && !result.changed);
    assert(filesystem.write_calls == 1U);

    assert(sd_manifest_make_commit_request(7U, make_manifest(2U), request));
    result = sd_manifest_execute_request_with_ops(request, context, kPaths,
                                                  ops);
    assert(result.status == SdManifestRequestStatus::Ok && result.changed);
    assert(result.slot == SdManifestSlot::B);

    assert(sd_manifest_make_commit_request(7U, make_manifest(4U), request));
    result = sd_manifest_execute_request_with_ops(request, context, kPaths,
                                                  ops);
    assert(result.status == SdManifestRequestStatus::GenerationMismatch);

    assert(sd_manifest_make_commit_request(7U, make_manifest(3U), request));
    filesystem.write_result = SdManifestWriteIoResult::ShortWrite;
    result = sd_manifest_execute_request_with_ops(request, context, kPaths,
                                                  ops);
    assert(result.status == SdManifestRequestStatus::ShortWrite);

    request.mount_generation = 0U;
    result = sd_manifest_execute_request_with_ops(request, context, kPaths,
                                                  ops);
    assert(result.status == SdManifestRequestStatus::InvalidArgument);
}
