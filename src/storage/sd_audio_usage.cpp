#include "sd_audio_usage.h"

#include <cerrno>
#include <cstdio>
#include <cstring>

#include <dirent.h>
#include <sys/stat.h>

#include "sd_storage_layout.h"
#include "storage_contract.h"

namespace sticky_storage {
namespace {

bool audio_name_valid(const char *name)
{
    if (!fat83_name_valid(name)) {
        return false;
    }
    const char *extension = std::strrchr(name, '.');
    return extension != nullptr &&
           (std::strcmp(extension, ".WAV") == 0 ||
            std::strcmp(extension, ".PCM") == 0 ||
            std::strcmp(extension, ".TMP") == 0);
}

}  // namespace

SdAudioUsageProbe sd_audio_usage_probe(const char *directory)
{
    SdAudioUsageProbe result = {};
    if (directory == nullptr || directory[0] == '\0') {
        return result;
    }

    errno = 0;
    DIR *stream = ::opendir(directory);
    if (stream == nullptr) {
        result.status = errno == ENOENT ? SdAudioUsageStatus::Missing
                                        : SdAudioUsageStatus::IoError;
        return result;
    }

    result.status = SdAudioUsageStatus::Ready;
    size_t entries = 0U;
    while (dirent *entry = ::readdir(stream)) {
        if (std::strcmp(entry->d_name, ".") == 0 ||
            std::strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (++entries > kSdAudioInventoryEntryLimit) {
            result.status = SdAudioUsageStatus::TooManyEntries;
            break;
        }
        if (!audio_name_valid(entry->d_name)) {
            result.status = SdAudioUsageStatus::UnexpectedEntry;
            break;
        }

        char path[kMaxPathBytes] = {};
        const int length = std::snprintf(path, sizeof(path), "%s/%s",
                                         directory, entry->d_name);
        if (length <= 0 || static_cast<size_t>(length) >= sizeof(path)) {
            result.status = SdAudioUsageStatus::UnexpectedEntry;
            break;
        }
        struct stat info = {};
        if (::stat(path, &info) != 0 || !S_ISREG(info.st_mode) ||
            info.st_size < 0) {
            result.status = SdAudioUsageStatus::UnexpectedEntry;
            break;
        }
        const uint64_t bytes = static_cast<uint64_t>(info.st_size);
        if (result.items == UINT32_MAX ||
            bytes > UINT64_MAX - result.bytes) {
            result.status = SdAudioUsageStatus::Overflow;
            break;
        }
        ++result.items;
        result.bytes += bytes;
    }
    if (::closedir(stream) != 0 && result.status == SdAudioUsageStatus::Ready) {
        result.status = SdAudioUsageStatus::IoError;
    }
    if (result.status != SdAudioUsageStatus::Ready &&
        result.status != SdAudioUsageStatus::Missing) {
        result.items = 0U;
        result.bytes = 0U;
    }
    return result;
}

}  // namespace sticky_storage
