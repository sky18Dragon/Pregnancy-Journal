#pragma once

#include <cstddef>
#include <cstdint>

#include "sticky_app_id.h"
#include "ui_language.h"

struct StickyDeviceSettings {
    UiLanguage language = UiLanguage::English;
    StickyAppId last_app = StickyAppId::Pregnancy;
};

struct StickyPersistentRecord {
    uint32_t magic = 0U;
    uint16_t version = 0U;
    uint16_t payload_size = 0U;
    uint32_t sequence = 0U;
    StickyDeviceSettings settings = {};
    uint32_t checksum = 0U;
};

constexpr uint32_t kStickyPersistentMagic = 0x53434657U;
constexpr uint16_t kStickyPersistentVersion = 1U;

uint32_t sticky_persistent_checksum(const StickyPersistentRecord &record);
StickyPersistentRecord sticky_persistent_make(
    const StickyDeviceSettings &settings, uint32_t sequence);
bool sticky_persistent_valid(const StickyPersistentRecord &record);
bool sticky_persistent_select(const StickyPersistentRecord *first,
                              const StickyPersistentRecord *second,
                              StickyPersistentRecord &selected);
