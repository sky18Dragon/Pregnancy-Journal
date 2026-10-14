#include "sd_application_data_delete.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "sd_storage_layout.h"

namespace sticky_storage {
namespace {

constexpr size_t kPathCapacity = 384U;
constexpr uint8_t kMaximumDepth = 6U;

bool safe_mount_root(const char *root)
{
    return root != nullptr && root[0] == '/' && root[1] != '\0' &&
           std::strstr(root, "..") == nullptr;
}

bool child_path(const char *parent, const char *name, char *output,
                size_t capacity)
{
    if (parent == nullptr || name == nullptr || name[0] == '\0' ||
        std::strcmp(name, ".") == 0 || std::strcmp(name, "..") == 0 ||
        std::strchr(name, '/') != nullptr) {
        return false;
    }
    const int written = std::snprintf(output, capacity, "%s/%s", parent, name);
    return written > 0 && static_cast<size_t>(written) < capacity;
}

SdApplicationDataDeleteStatus remove_tree(
    const char *path, uint8_t depth, SdApplicationDataDeleteResult &result,
    const SdApplicationDataDeleteOps &ops)
{
    if (depth > kMaximumDepth)
        return SdApplicationDataDeleteStatus::DepthExceeded;
    struct stat info = {};
    if (stat(path, &info) != 0) {
        return errno == ENOENT ? SdApplicationDataDeleteStatus::Missing
                               : SdApplicationDataDeleteStatus::IoError;
    }
    if (!S_ISDIR(info.st_mode)) {
        if (ops.remove_file(path) != 0) return SdApplicationDataDeleteStatus::IoError;
        ++result.files_removed;
        return SdApplicationDataDeleteStatus::Ok;
    }

    DIR *directory = opendir(path);
    if (directory == nullptr) return SdApplicationDataDeleteStatus::IoError;
    SdApplicationDataDeleteStatus status = SdApplicationDataDeleteStatus::Ok;
    while (status == SdApplicationDataDeleteStatus::Ok) {
        errno = 0;
        dirent *entry = readdir(directory);
        if (entry == nullptr) {
            if (errno != 0) status = SdApplicationDataDeleteStatus::IoError;
            break;
        }
        if (std::strcmp(entry->d_name, ".") == 0 ||
            std::strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char child[kPathCapacity] = {};
        if (!child_path(path, entry->d_name, child, sizeof(child))) {
            status = SdApplicationDataDeleteStatus::PathTooLong;
            break;
        }
        status = remove_tree(child, static_cast<uint8_t>(depth + 1U), result, ops);
        if (status == SdApplicationDataDeleteStatus::Missing)
            status = SdApplicationDataDeleteStatus::Ok;
    }
    if (closedir(directory) != 0 &&
        status == SdApplicationDataDeleteStatus::Ok) {
        status = SdApplicationDataDeleteStatus::IoError;
    }
    if (status != SdApplicationDataDeleteStatus::Ok) return status;
    if (ops.remove_directory(path) != 0) return SdApplicationDataDeleteStatus::IoError;
    ++result.directories_removed;
    return SdApplicationDataDeleteStatus::Ok;
}

}  // namespace

SdApplicationDataDeleteResult sd_application_data_delete(
    const char *mount_root, const char *application_directory)
{
    return sd_application_data_delete_with_ops(mount_root, application_directory,
                                               {unlink, rmdir});
}

SdApplicationDataDeleteResult sd_application_data_delete_with_ops(
    const char *mount_root, const char *application_directory,
    const SdApplicationDataDeleteOps &ops)
{
    SdApplicationDataDeleteResult result = {};
    if (ops.remove_file == nullptr || ops.remove_directory == nullptr || !safe_mount_root(mount_root) || application_directory == nullptr ||
        std::strcmp(application_directory, kApplicationDirectory) != 0) {
        result.status = SdApplicationDataDeleteStatus::InvalidArgument;
        return result;
    }
    char root[kPathCapacity] = {};
    if (!child_path(mount_root, application_directory, root, sizeof(root))) {
        result.status = SdApplicationDataDeleteStatus::PathTooLong;
        return result;
    }
    result.status = remove_tree(root, 0U, result, ops);
    if (result.status == SdApplicationDataDeleteStatus::Missing)
        result.status = SdApplicationDataDeleteStatus::Ok;
    return result;
}

}  // namespace sticky_storage
