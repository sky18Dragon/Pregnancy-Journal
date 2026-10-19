#include "domain_repository_records.h"

#include <cstring>

#include "storage_crc32.h"

namespace pregnancy_repository {
namespace {

constexpr uint16_t kEnvelopeVersion = 1U;
constexpr size_t kHeaderBytes = 8U;
constexpr size_t kChecksumBytes = 4U;
constexpr uint32_t kPregnancyMagic = 0x31524750U;       // PGR1
constexpr uint32_t kKickMagic = 0x314B434BU;  // KCK1
constexpr uint32_t kWeightMagic = 0x31544757U;          // WGT1
constexpr uint32_t kWeightPreferencesMagic = 0x31504657U;  // WFP1
constexpr uint32_t kDevicePreferencesMagic = 0x31504644U;  // DFP1
constexpr uint32_t kPregnancySingletonId = 1U;
constexpr uint32_t kDevicePreferencesId = 1U;
constexpr uint32_t kWeightPreferencesId = 2U;

class Writer {
  public:
    Writer(uint8_t *bytes, size_t capacity) : bytes_(bytes), capacity_(capacity) {}

    bool u8(uint8_t value)
    {
        if (size_ >= capacity_) return false;
        bytes_[size_++] = value;
        return true;
    }

    bool u16(uint16_t value)
    {
        return u8(static_cast<uint8_t>(value)) &&
               u8(static_cast<uint8_t>(value >> 8U));
    }

    bool u32(uint32_t value)
    {
        return u16(static_cast<uint16_t>(value)) &&
               u16(static_cast<uint16_t>(value >> 16U));
    }

    bool string(const char *value, size_t capacity)
    {
        size_t length = 0U;
        while (length < capacity && value[length] != '\0') ++length;
        if (length == capacity || length > UINT8_MAX || !u8(length)) return false;
        for (size_t index = 0U; index < length; ++index) {
            if (!u8(static_cast<uint8_t>(value[index]))) return false;
        }
        return true;
    }

    size_t size() const { return size_; }

  private:
    uint8_t *bytes_ = nullptr;
    size_t capacity_ = 0U;
    size_t size_ = 0U;
};

class Reader {
  public:
    Reader(const uint8_t *bytes, size_t size) : bytes_(bytes), size_(size) {}

    bool u8(uint8_t &value)
    {
        if (offset_ >= size_) return false;
        value = bytes_[offset_++];
        return true;
    }

    bool u16(uint16_t &value)
    {
        uint8_t low = 0U;
        uint8_t high = 0U;
        if (!u8(low) || !u8(high)) return false;
        value = static_cast<uint16_t>(low) |
                static_cast<uint16_t>(static_cast<uint16_t>(high) << 8U);
        return true;
    }

    bool u32(uint32_t &value)
    {
        uint16_t low = 0U;
        uint16_t high = 0U;
        if (!u16(low) || !u16(high)) return false;
        value = static_cast<uint32_t>(low) |
                (static_cast<uint32_t>(high) << 16U);
        return true;
    }

    bool string(char *value, size_t capacity)
    {
        uint8_t length = 0U;
        if (!u8(length) || static_cast<size_t>(length) >= capacity ||
            remaining() < length) {
            return false;
        }
        std::memcpy(value, bytes_ + offset_, length);
        value[length] = '\0';
        offset_ += length;
        return true;
    }

    size_t remaining() const { return size_ - offset_; }

  private:
    const uint8_t *bytes_ = nullptr;
    size_t size_ = 0U;
    size_t offset_ = 0U;
};

bool key_from_id(sticky_storage::RepositoryKind kind, uint32_t id,
                 sticky_storage::RepositoryKey &key)
{
    if (id == 0U) return false;
    char text[sticky_storage::kRepositoryRecordIdCharacters + 1U] = {};
    constexpr char kHex[] = "0123456789abcdef";
    uint32_t remaining = id;
    for (size_t position = sticky_storage::kRepositoryRecordIdCharacters;
         position > 0U; --position) {
        text[position - 1U] = kHex[remaining & 0xFU];
        remaining >>= 4U;
    }
    return sticky_storage::repository_key_make(kind, text, key);
}

bool write_date(Writer &writer, const PregnancyDate &date)
{
    return writer.u16(date.year) && writer.u8(date.month) && writer.u8(date.day);
}

bool read_date(Reader &reader, PregnancyDate &date)
{
    return reader.u16(date.year) && reader.u8(date.month) && reader.u8(date.day);
}

template <typename BodyWriter>
RecordCodecStatus encode(uint32_t magic, BodyWriter write_body,
                         EncodedPayload &payload)
{
    payload = {};
    Writer writer(payload.bytes.data(), payload.bytes.size());
    if (!writer.u32(magic) || !writer.u16(kEnvelopeVersion) || !writer.u16(0U) ||
        !write_body(writer)) {
        payload = {};
        return RecordCodecStatus::BufferTooSmall;
    }
    const size_t body_size = writer.size() - kHeaderBytes;
    if (body_size > UINT16_MAX) {
        payload = {};
        return RecordCodecStatus::BufferTooSmall;
    }
    payload.bytes[6] = static_cast<uint8_t>(body_size);
    payload.bytes[7] = static_cast<uint8_t>(body_size >> 8U);
    const uint32_t checksum = sticky_storage::crc32(payload.bytes.data(), writer.size());
    if (!writer.u32(checksum)) {
        payload = {};
        return RecordCodecStatus::BufferTooSmall;
    }
    payload.size = writer.size();
    return RecordCodecStatus::Ok;
}

RecordCodecStatus open(const uint8_t *bytes, size_t size, uint32_t magic,
                       Reader &body)
{
    if (bytes == nullptr || size < kHeaderBytes + kChecksumBytes) {
        return RecordCodecStatus::InvalidArgument;
    }
    Reader header(bytes, kHeaderBytes);
    uint32_t actual_magic = 0U;
    uint16_t version = 0U;
    uint16_t body_size = 0U;
    if (!header.u32(actual_magic) || !header.u16(version) ||
        !header.u16(body_size)) {
        return RecordCodecStatus::Corrupt;
    }
    if (actual_magic != magic) return RecordCodecStatus::Corrupt;
    if (version != kEnvelopeVersion) return RecordCodecStatus::UnsupportedVersion;
    if (size != kHeaderBytes + body_size + kChecksumBytes) {
        return RecordCodecStatus::Corrupt;
    }
    Reader checksum_reader(bytes + size - kChecksumBytes, kChecksumBytes);
    uint32_t stored_checksum = 0U;
    if (!checksum_reader.u32(stored_checksum) ||
        stored_checksum != sticky_storage::crc32(bytes, size - kChecksumBytes)) {
        return RecordCodecStatus::Corrupt;
    }
    body = Reader(bytes + kHeaderBytes, body_size);
    return RecordCodecStatus::Ok;
}

bool weight_preferences_valid(const WeightProfileSettings &value)
{
    return value.height_cm >= 100U && value.height_cm <= 220U &&
           (value.baseline_tenths_kg == 0U ||
            (value.baseline_tenths_kg >= 300U &&
             value.baseline_tenths_kg <= 2000U));
}

bool device_preferences_valid(const StickyDeviceSettings &value)
{
    return (value.language == UiLanguage::English ||
            value.language == UiLanguage::ChineseSimplified) &&
           sticky_app_id_valid(value.last_app) &&
           static_cast<uint8_t>(value.shortcut_target) <=
               static_cast<uint8_t>(StickyShortcutTarget::Off) &&
           static_cast<uint8_t>(value.kick_mode_pref) <=
               static_cast<uint8_t>(StickyKickModePref::Awareness);
}

bool terminated(const char *value, size_t capacity)
{
    for (size_t index = 0U; index < capacity; ++index) {
        if (value[index] == '\0') return true;
    }
    return false;
}

}  // namespace

bool pregnancy_profile_key(sticky_storage::RepositoryKey &key)
{
    return key_from_id(sticky_storage::RepositoryKind::Pregnancy,
                       kPregnancySingletonId, key);
}

bool kick_key(uint32_t id, sticky_storage::RepositoryKey &key)
{
    return key_from_id(sticky_storage::RepositoryKind::Kicks, id, key);
}

bool weight_key(uint32_t id, sticky_storage::RepositoryKey &key)
{
    return key_from_id(sticky_storage::RepositoryKind::Weight, id, key);
}

bool weight_preferences_key(sticky_storage::RepositoryKey &key)
{
    return key_from_id(sticky_storage::RepositoryKind::Preference,
                       kWeightPreferencesId, key);
}

bool device_preferences_key(sticky_storage::RepositoryKey &key)
{
    return key_from_id(sticky_storage::RepositoryKind::Preference,
                       kDevicePreferencesId, key);
}

bool numeric_record_id(const char *record_id, uint32_t &id)
{
    id = 0U;
    if (record_id == nullptr) return false;
    for (size_t index = 0U; index < 24U; ++index) {
        if (record_id[index] != '0') return false;
    }
    for (size_t index = 24U; index < 32U; ++index) {
        const char value = record_id[index];
        uint8_t digit = 0U;
        if (value >= '0' && value <= '9') {
            digit = static_cast<uint8_t>(value - '0');
        } else if (value >= 'a' && value <= 'f') {
            digit = static_cast<uint8_t>(value - 'a' + 10U);
        } else {
            return false;
        }
        id = (id << 4U) | digit;
    }
    return record_id[32] == '\0' && id != 0U;
}

RecordCodecStatus encode_pregnancy_profile(const PregnancyProfile &value,
                                            EncodedPayload &payload)
{
    if (!pregnancy_profile_valid(value)) return RecordCodecStatus::InvalidArgument;
    return encode(kPregnancyMagic, [&](Writer &writer) {
        return writer.u8(value.schema_version) &&
               write_date(writer, value.last_menstrual_period) &&
               write_date(writer, value.estimated_due_date) &&
               writer.u8(value.use_due_date_as_primary ? 1U : 0U);
    }, payload);
}

RecordCodecStatus decode_pregnancy_profile(const uint8_t *bytes, size_t size,
                                            PregnancyProfile &value)
{
    Reader body(nullptr, 0U);
    const RecordCodecStatus status = open(bytes, size, kPregnancyMagic, body);
    if (status != RecordCodecStatus::Ok) return status;
    PregnancyProfile decoded = {};
    uint8_t primary = 0U;
    if (!body.u8(decoded.schema_version) ||
        !read_date(body, decoded.last_menstrual_period) ||
        !read_date(body, decoded.estimated_due_date) || !body.u8(primary) ||
        body.remaining() != 0U || primary > 1U) {
        return RecordCodecStatus::Corrupt;
    }
    decoded.use_due_date_as_primary = primary != 0U;
    if (decoded.schema_version != kPregnancyProfileSchemaVersion) {
        return RecordCodecStatus::UnsupportedVersion;
    }
    if (!pregnancy_profile_valid(decoded)) return RecordCodecStatus::Corrupt;
    value = decoded;
    return RecordCodecStatus::Ok;
}

RecordCodecStatus encode_kick(const KickSession &value, EncodedPayload &payload)
{
    if (!kick_session_valid(value) || !terminated(value.note, sizeof(value.note))) {
        return RecordCodecStatus::InvalidArgument;
    }
    return encode(kKickMagic, [&](Writer &writer) {
        return writer.u8(value.schema_version) && writer.u32(value.id) &&
               write_date(writer, value.date) && writer.u8(value.hour) &&
               writer.u8(value.minute) && writer.u16(value.duration_minutes) &&
               writer.u16(value.count) && writer.u8(static_cast<uint8_t>(value.period)) &&
               writer.string(value.note, sizeof(value.note)) &&
               writer.u8(static_cast<uint8_t>(value.mode)) &&
               writer.u8(static_cast<uint8_t>(value.feel)) &&
               writer.u16(value.elapsed_seconds);
    }, payload);
}

RecordCodecStatus decode_kick(const uint8_t *bytes, size_t size, KickSession &value)
{
    Reader body(nullptr, 0U);
    const RecordCodecStatus status = open(bytes, size, kKickMagic, body);
    if (status != RecordCodecStatus::Ok) return status;
    KickSession decoded = {};
    uint8_t period = 0U;
    if (!body.u8(decoded.schema_version) || !body.u32(decoded.id) ||
        !read_date(body, decoded.date) || !body.u8(decoded.hour) ||
        !body.u8(decoded.minute) || !body.u16(decoded.duration_minutes) ||
        !body.u16(decoded.count) || !body.u8(period) ||
        !body.string(decoded.note, sizeof(decoded.note))) {
        return RecordCodecStatus::Corrupt;
    }
    decoded.period = static_cast<KickPeriod>(period);
    if (decoded.schema_version == kKickSessionLegacySchemaVersion) {
        // Version 1 payloads predate modes: read them as Timed sessions.
        if (body.remaining() != 0U) return RecordCodecStatus::Corrupt;
        decoded.mode = KickMode::Timed;
        decoded.feel = KickFeel::None;
        decoded.elapsed_seconds =
            static_cast<uint16_t>(decoded.duration_minutes * 60U);
    } else if (decoded.schema_version == kKickSessionSchemaVersion) {
        uint8_t mode = 0U;
        uint8_t feel = 0U;
        if (!body.u8(mode) || !body.u8(feel) ||
            !body.u16(decoded.elapsed_seconds) || body.remaining() != 0U) {
            return RecordCodecStatus::Corrupt;
        }
        decoded.mode = static_cast<KickMode>(mode);
        decoded.feel = static_cast<KickFeel>(feel);
    } else {
        return RecordCodecStatus::UnsupportedVersion;
    }
    decoded.schema_version = kKickSessionSchemaVersion;
    if (!kick_session_valid(decoded)) return RecordCodecStatus::Corrupt;
    value = decoded;
    return RecordCodecStatus::Ok;
}

RecordCodecStatus encode_weight(const WeightRecord &value,
                                EncodedPayload &payload)
{
    if (!weight_record_valid(value) || !terminated(value.note, sizeof(value.note))) {
        return RecordCodecStatus::InvalidArgument;
    }
    return encode(kWeightMagic, [&](Writer &writer) {
        return writer.u8(value.schema_version) && writer.u32(value.id) &&
               write_date(writer, value.date) &&
               writer.u16(value.weight_tenths_kg) &&
               writer.string(value.note, sizeof(value.note));
    }, payload);
}

RecordCodecStatus decode_weight(const uint8_t *bytes, size_t size,
                                WeightRecord &value)
{
    Reader body(nullptr, 0U);
    const RecordCodecStatus status = open(bytes, size, kWeightMagic, body);
    if (status != RecordCodecStatus::Ok) return status;
    WeightRecord decoded = {};
    if (!body.u8(decoded.schema_version) || !body.u32(decoded.id) ||
        !read_date(body, decoded.date) ||
        !body.u16(decoded.weight_tenths_kg) ||
        !body.string(decoded.note, sizeof(decoded.note)) ||
        body.remaining() != 0U) {
        return RecordCodecStatus::Corrupt;
    }
    if (decoded.schema_version != kWeightRecordSchemaVersion) {
        return RecordCodecStatus::UnsupportedVersion;
    }
    if (!weight_record_valid(decoded)) return RecordCodecStatus::Corrupt;
    value = decoded;
    return RecordCodecStatus::Ok;
}

RecordCodecStatus encode_weight_preferences(
    const WeightProfileSettings &value, EncodedPayload &payload)
{
    if (!weight_preferences_valid(value)) return RecordCodecStatus::InvalidArgument;
    return encode(kWeightPreferencesMagic, [&](Writer &writer) {
        return writer.u16(value.height_cm) &&
               writer.u16(value.baseline_tenths_kg) &&
               writer.u8(value.use_pounds ? 1U : 0U);
    }, payload);
}

RecordCodecStatus decode_weight_preferences(
    const uint8_t *bytes, size_t size, WeightProfileSettings &value)
{
    Reader body(nullptr, 0U);
    const RecordCodecStatus status = open(bytes, size, kWeightPreferencesMagic, body);
    if (status != RecordCodecStatus::Ok) return status;
    WeightProfileSettings decoded = {};
    uint8_t pounds = 0U;
    if (!body.u16(decoded.height_cm) ||
        !body.u16(decoded.baseline_tenths_kg) || !body.u8(pounds) ||
        pounds > 1U || body.remaining() != 0U) {
        return RecordCodecStatus::Corrupt;
    }
    decoded.use_pounds = pounds != 0U;
    if (!weight_preferences_valid(decoded)) return RecordCodecStatus::Corrupt;
    value = decoded;
    return RecordCodecStatus::Ok;
}

RecordCodecStatus encode_device_preferences(
    const StickyDeviceSettings &value, EncodedPayload &payload)
{
    if (!device_preferences_valid(value)) return RecordCodecStatus::InvalidArgument;
    return encode(kDevicePreferencesMagic, [&](Writer &writer) {
        return writer.u8(static_cast<uint8_t>(value.language)) &&
               writer.u8(static_cast<uint8_t>(value.last_app)) &&
               writer.u8(static_cast<uint8_t>(value.shortcut_target)) &&
               writer.u8(static_cast<uint8_t>(value.kick_mode_pref));
    }, payload);
}

RecordCodecStatus decode_device_preferences(
    const uint8_t *bytes, size_t size, StickyDeviceSettings &value)
{
    Reader body(nullptr, 0U);
    const RecordCodecStatus status = open(bytes, size, kDevicePreferencesMagic, body);
    if (status != RecordCodecStatus::Ok) return status;
    StickyDeviceSettings decoded = {};
    uint8_t language = 0U;
    uint8_t app = 0U;
    if (!body.u8(language) || !body.u8(app)) {
        return RecordCodecStatus::Corrupt;
    }
    // Records written by older firmware stop after these two bytes; the newer
    // fields then keep their defaults.
    uint8_t shortcut = static_cast<uint8_t>(decoded.shortcut_target);
    uint8_t kick_mode = static_cast<uint8_t>(decoded.kick_mode_pref);
    if (body.remaining() != 0U &&
        (!body.u8(shortcut) || !body.u8(kick_mode))) {
        return RecordCodecStatus::Corrupt;
    }
    if (body.remaining() != 0U) {
        return RecordCodecStatus::Corrupt;
    }
    decoded.language = static_cast<UiLanguage>(language);
    decoded.last_app = static_cast<StickyAppId>(app);
    decoded.shortcut_target = static_cast<StickyShortcutTarget>(shortcut);
    decoded.kick_mode_pref = static_cast<StickyKickModePref>(kick_mode);
    if (!device_preferences_valid(decoded)) return RecordCodecStatus::Corrupt;
    value = decoded;
    return RecordCodecStatus::Ok;
}

}  // namespace pregnancy_repository
