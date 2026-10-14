#include "kick_storage_record.h"

#include <cstddef>
#include <cstring>

KickSession kick_session_from_v1(const KickSessionV1 &legacy) {
  KickSession session = {};
  session.id = legacy.id;
  session.date = legacy.date;
  session.hour = legacy.hour;
  session.minute = legacy.minute;
  session.duration_minutes = legacy.duration_minutes;
  session.count = legacy.count;
  session.period = legacy.period;
  std::memcpy(session.note, legacy.note, sizeof(session.note));
  session.mode = KickMode::Timed;
  session.elapsed_seconds =
      static_cast<uint16_t>(legacy.duration_minutes * 60U);
  return session;
}

bool kick_storage_record_valid(const KickStorageRecord &record) {
  const auto *bytes = reinterpret_cast<const uint8_t *>(&record);
  uint32_t hash = 2166136261U;
  for (size_t i = 0U; i < offsetof(KickStorageRecord, checksum); ++i) {
    hash ^= bytes[i];
    hash *= 16777619U;
  }
  return record.magic == 0x4B494B31U && record.version == 1U &&
         record.size == sizeof(KickSessionV1) &&
         record.item.schema_version == kKickSessionLegacySchemaVersion &&
         kick_session_valid(kick_session_from_v1(record.item)) &&
         record.checksum == hash;
}
