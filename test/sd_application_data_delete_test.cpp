#include <cassert>
#include <cstdlib>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include "sd_application_data_delete.h"

namespace {

int fail_directory(const char *) { return -1; }

bool exists(const std::string &path)
{
    struct stat info = {};
    return lstat(path.c_str(), &info) == 0;
}

void make_dir(const std::string &path) { assert(mkdir(path.c_str(), 0700) == 0); }
void make_file(const std::string &path, const char *text)
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
    const std::string app = std::string(mount) + "/PREGJNL";
    const std::string records = app + "/RECORDS";
    const std::string object = records + "/ONE.BIN";
    const std::string other = std::string(mount) + "/PHOTOS";
    const std::string keep = other + "/KEEP.JPG";
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

    assert(unlink(keep.c_str()) == 0);
    assert(rmdir(other.c_str()) == 0);
    assert(rmdir(mount) == 0);
    return 0;
}
