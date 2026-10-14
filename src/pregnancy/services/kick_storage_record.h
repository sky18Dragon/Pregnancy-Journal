#pragma once

#include "kick.h"

// Frozen schema-1 session layout used by the legacy ESP32 NVS blobs.
struct KickSessionV1 {
  uint8_t schema_version;
  uint32_t id;
  PregnancyDate date;
  uint8_t hour;
  uint8_t minute;
  uint16_t duration_minutes;
  uint16_t count;
  KickPeriod period;
  char note[40];
};

// Exact legacy ESP32 NVS layout, read only. New records use the SD codec.
struct KickStorageRecord {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  KickSessionV1 item;
  uint32_t checksum;
};
static_assert(sizeof(KickSessionV1) == 60U);
static_assert(sizeof(KickStorageRecord) == 72U);
bool kick_storage_record_valid(const KickStorageRecord &record);
// Converts a validated legacy record into the current Timed session form.
KickSession kick_session_from_v1(const KickSessionV1 &legacy);
