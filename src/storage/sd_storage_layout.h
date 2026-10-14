#pragma once

#include <cstddef>

namespace sticky_storage {

// FAT long-file-name support is intentionally not required. Keep every
// physical component inside this conservative uppercase ASCII 8.3 subset.
inline constexpr char kApplicationDirectory[] = "PREGJNL";
inline constexpr char kManifestDirectory[] = "MANIFEST";
inline constexpr char kManifestSlotAFile[] = "MNF000A.JSN";
inline constexpr char kManifestSlotBFile[] = "MNF000B.JSN";
inline constexpr char kJournalDirectory[] = "JOURNAL";
inline constexpr char kJournalFirstSegmentFile[] = "TXN00001.LOG";
inline constexpr char kRecordsDirectory[] = "RECORDS";
inline constexpr char kAudioDirectory[] = "AUDIO";
inline constexpr char kTasksDirectory[] = "TASKS";
inline constexpr char kIndexDirectory[] = "INDEX";
inline constexpr char kTemporaryDirectory[] = "TMP";

inline constexpr char kApplicationRootPath[] = "/sdcard/PREGJNL";
inline constexpr char kManifestDirectoryPath[] =
    "/sdcard/PREGJNL/MANIFEST";
inline constexpr char kManifestSlotAPath[] =
    "/sdcard/PREGJNL/MANIFEST/MNF000A.JSN";
inline constexpr char kManifestSlotBPath[] =
    "/sdcard/PREGJNL/MANIFEST/MNF000B.JSN";
inline constexpr char kJournalDirectoryPath[] = "/sdcard/PREGJNL/JOURNAL";
inline constexpr char kJournalFirstSegmentPath[] =
    "/sdcard/PREGJNL/JOURNAL/TXN00001.LOG";
inline constexpr char kRecordsDirectoryPath[] = "/sdcard/PREGJNL/RECORDS";
inline constexpr char kAudioDirectoryPath[] = "/sdcard/PREGJNL/AUDIO";
inline constexpr char kTasksDirectoryPath[] = "/sdcard/PREGJNL/TASKS";
inline constexpr char kIndexDirectoryPath[] = "/sdcard/PREGJNL/INDEX";
inline constexpr char kTemporaryDirectoryPath[] = "/sdcard/PREGJNL/TMP";

constexpr bool fat83_character(char value)
{
    return (value >= 'A' && value <= 'Z') ||
           (value >= '0' && value <= '9') || value == '_';
}

constexpr bool fat83_name_valid(const char *name)
{
    if (name == nullptr || name[0] == '\0') {
        return false;
    }
    size_t base_length = 0U;
    size_t extension_length = 0U;
    bool extension = false;
    for (size_t index = 0U; name[index] != '\0'; ++index) {
        if (name[index] == '.') {
            if (extension || base_length == 0U) {
                return false;
            }
            extension = true;
            continue;
        }
        if (!fat83_character(name[index])) {
            return false;
        }
        if (extension) {
            if (++extension_length > 3U) {
                return false;
            }
        } else if (++base_length > 8U) {
            return false;
        }
    }
    return base_length > 0U && (!extension || extension_length > 0U);
}

static_assert(fat83_name_valid(kApplicationDirectory));
static_assert(fat83_name_valid(kManifestDirectory));
static_assert(fat83_name_valid(kManifestSlotAFile));
static_assert(fat83_name_valid(kManifestSlotBFile));
static_assert(fat83_name_valid(kJournalDirectory));
static_assert(fat83_name_valid(kJournalFirstSegmentFile));
static_assert(fat83_name_valid(kRecordsDirectory));
static_assert(fat83_name_valid(kAudioDirectory));
static_assert(fat83_name_valid(kTasksDirectory));
static_assert(fat83_name_valid(kIndexDirectory));
static_assert(fat83_name_valid(kTemporaryDirectory));

}  // namespace sticky_storage
