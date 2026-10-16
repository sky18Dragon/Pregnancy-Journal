#pragma once

#include <cstdint>

// What a double-click on the top button does. Persisted values; append only.
// The default (value 0) is also what a record from older firmware decodes to.
// 双击顶键的目标。持久化值只追加不重排；0 为默认，旧固件写的记录也读作 0。
enum class StickyShortcutTarget : uint8_t {
    WeightRecordToday = 0,
    BabyWeek = 1,
    KickStart = 2,
    Off = 3,
};

constexpr uint8_t kStickyShortcutTargetCount = 4U;
