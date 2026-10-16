#pragma once

#include <cstddef>
#include <cstdint>

#include "sticky_app_id.h"
#include "ui_language.h"

// What a double-click on the top button does. Persisted values; append only.
// The default (value 0) is also what a record from older firmware decodes to.
// 双击顶键的目标。持久化值只追加不重排；0 为默认，旧固件写的记录也读作 0。
enum class StickyShortcutTarget : uint8_t {
    WeightRecordToday = 0,
    BabyWeek = 1,
    KickStart = 2,
    Off = 3,
};

// The kick record method the user picked last. Persisted values; append only.
// Not KickMode: that enum starts at 0, which here means "never chosen".
// 胎动记录方式的上次选择。持久化值只追加；0 表示从未选择，所以不能直接存 KickMode。
enum class StickyKickModePref : uint8_t {
    Unset = 0,
    Timed = 1,
    CountToTen = 2,
    Awareness = 3,
};

struct StickyDeviceSettings {
    UiLanguage language = UiLanguage::English;
    StickyAppId last_app = StickyAppId::Pregnancy;
    StickyShortcutTarget shortcut_target = StickyShortcutTarget::WeightRecordToday;
    StickyKickModePref kick_mode_pref = StickyKickModePref::Unset;
};

// Frozen layout of the old Flash (NVS) record. Its size and checksum are part
// of the stored format and of legacy migration: never add fields here.
// 旧 Flash(NVS) 记录的冻结布局，大小和校验都属于存储格式，不得加字段。
struct StickyLegacyDeviceSettings {
    UiLanguage language = UiLanguage::English;
    StickyAppId last_app = StickyAppId::Pregnancy;
};

struct StickyPersistentRecord {
    uint32_t magic = 0U;
    uint16_t version = 0U;
    uint16_t payload_size = 0U;
    uint32_t sequence = 0U;
    StickyLegacyDeviceSettings settings = {};
    uint32_t checksum = 0U;
};

constexpr uint32_t kStickyPersistentMagic = 0x53434657U;
constexpr uint16_t kStickyPersistentVersion = 1U;

uint32_t sticky_persistent_checksum(const StickyPersistentRecord &record);
StickyPersistentRecord sticky_persistent_make(
    const StickyDeviceSettings &settings, uint32_t sequence);
bool sticky_persistent_valid(const StickyPersistentRecord &record);
// The device settings a legacy record carries; newer fields take their defaults.
StickyDeviceSettings sticky_persistent_settings(const StickyPersistentRecord &record);
bool sticky_persistent_select(const StickyPersistentRecord *first,
                              const StickyPersistentRecord *second,
                              StickyPersistentRecord &selected);
