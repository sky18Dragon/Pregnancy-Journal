#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

#include "sd_application_data_delete.h"

namespace {

int fail_directory(const char *) { return -1; }

bool exists(const char *path)
{
    struct stat info = {};
    return lstat(path, &info) == 0;
}

void make_dir(const char *path) { assert(mkdir(path, 0700) == 0); }
void make_file(const char *path, const char *text)
{
    std::ofstream output(path, std::ios::binary);
    output << text;
    assert(output.good());
}

}  // namespace

int main()
{
    char mount_template[] = "/tmp/sticky-delete-XXXXXX";
    char *mount = mkdtemp(mount_template);
    assert(mount != nullptr);
    char app[256] = {};
    char records[256] = {};
    char object[256] = {};
    char other[256] = {};
    char keep[256] = {};
    std::snprintf(app, sizeof(app), "%s/PREGJNL", mount);
    std::snprintf(records, sizeof(records), "%s/RECORDS", app);
    std::snprintf(object, sizeof(object), "%s/ONE.BIN", records);
    std::snprintf(other, sizeof(other), "%s/PHOTOS", mount);
    std::snprintf(keep, sizeof(keep), "%s/KEEP.JPG", other);
    make_dir(app);
    make_dir(records);
    make_file(object, "app data");
    make_dir(other);
    make_file(keep, "user photo");

    using namespace sticky_storage;
    const auto partial = sd_application_data_delete_with_ops(
        mount, "PREGJNL", {unlink, fail_directory});
    assert(partial.status == SdApplicationDataDeleteStatus::IoError);
    assert(partial.files_removed == 1U && exists(app) && !exists(object));
    assert(exists(keep));
    make_file(object, "app data");
    SdApplicationDataDeleteResult result =
        sd_application_data_delete(mount, "PREGJNL");
    assert(result.status == SdApplicationDataDeleteStatus::Ok);
    assert(result.files_removed == 1U && result.directories_removed == 2U);
    assert(!exists(app));
    assert(exists(keep));
    assert(sd_application_data_delete(mount, "PREGJNL").status ==
           SdApplicationDataDeleteStatus::Ok);
    assert(sd_application_data_delete(mount, "PHOTOS").status ==
           SdApplicationDataDeleteStatus::InvalidArgument);

    assert(unlink(keep) == 0);
    assert(rmdir(other) == 0);
    assert(rmdir(mount) == 0);
    return 0;
}
