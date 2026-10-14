#include <cassert>
#include <cstring>

#include "domain_repository_records.h"
#include "storage_crc32.h"

namespace {

bool same_date(const PregnancyDate &left, const PregnancyDate &right)
{
    return left.year == right.year && left.month == right.month &&
           left.day == right.day;
}

void test_keys()
{
    using namespace pregnancy_repository;
    using namespace sticky_storage;
    RepositoryKey pregnancy = {};
    RepositoryKey reminder = {};
    RepositoryKey checkup = {};
    RepositoryKey weight = {};
    RepositoryKey weight_preferences = {};
    RepositoryKey device_preferences = {};
    RepositoryKey network_authorization = {};
    assert(pregnancy_profile_key(pregnancy));
    assert(reminder_key(42U, reminder));
    assert(checkup_key(UINT32_MAX, checkup));
    assert(weight_key(7U, weight));
    assert(weight_preferences_key(weight_preferences));
    assert(device_preferences_key(device_preferences));
    assert(network_authorization_key(network_authorization));
    assert(pregnancy.repository == RepositoryKind::Pregnancy);
    assert(reminder.repository == RepositoryKind::Reminder);
    assert(checkup.repository == RepositoryKind::Checkup);
    assert(weight.repository == RepositoryKind::Weight);
    assert(weight_preferences.repository == RepositoryKind::Preference);
    assert(device_preferences.repository == RepositoryKind::Preference);
    assert(network_authorization.repository == RepositoryKind::Preference);
    uint32_t parsed_id = 0U;
    assert(numeric_record_id(reminder.record_id, parsed_id));
    assert(parsed_id == 42U);
    assert(!numeric_record_id("00000000000000000000000000000000", parsed_id));
    assert(std::strcmp(pregnancy.record_id,
                       "00000000000000000000000000000001") == 0);
    assert(std::strcmp(reminder.record_id,
                       "0000000000000000000000000000002a") == 0);
    assert(std::strcmp(checkup.record_id,
                       "000000000000000000000000ffffffff") == 0);
    assert(std::strcmp(weight_preferences.record_id,
                       "00000000000000000000000000000002") == 0);
    assert(std::strcmp(device_preferences.record_id,
                       "00000000000000000000000000000001") == 0);
    assert(std::strcmp(network_authorization.record_id,
                       "00000000000000000000000000000003") == 0);
    assert(!reminder_key(0U, reminder));
}

void test_kicks()
{
    using namespace pregnancy_repository;
    KickSession session = {};
    session.id = UINT32_MAX;
    session.date = {2026U, 10U, 10U};
    session.hour = 23U;
    session.minute = 59U;
    session.duration_minutes = 120U;  // Valid old firmware sessions remain migratable.
    session.count = 500U;
    session.period = KickPeriod::Evening;
    std::strcpy(session.note, "legacy record");
    EncodedPayload encoded = {};
    assert(encode_kick(session, encoded) == RecordCodecStatus::Ok);
    sticky_storage::RepositoryKey key = {};
    assert(kick_key(session.id, key));
    assert(key.repository == sticky_storage::RepositoryKind::Kicks);
    KickSession decoded = {};
    assert(decode_kick(encoded.bytes.data(), encoded.size, decoded) == RecordCodecStatus::Ok);
    assert(decoded.id == session.id && decoded.count == 500U);
    assert(decoded.duration_minutes == 120U && decoded.hour == 23U);
    assert(decoded.period == KickPeriod::Evening && same_date(decoded.date, session.date));
    assert(std::strcmp(decoded.note, session.note) == 0);
    for (size_t cut = 0U; cut < encoded.size; ++cut)
        assert(decode_kick(encoded.bytes.data(), cut, decoded) != RecordCodecStatus::Ok);
    for (size_t index = 0U; index < encoded.size; ++index) {
        auto broken = encoded;
        broken.bytes[index] ^= 1U;
        assert(decode_kick(broken.bytes.data(), broken.size, decoded) != RecordCodecStatus::Ok);
    }
    std::memset(session.note, 'x', sizeof(session.note));
    assert(encode_kick(session, encoded) == RecordCodecStatus::InvalidArgument);
}

void test_kick_modes()
{
    using namespace pregnancy_repository;
    KickSession ten = {};
    ten.id = 7U;
    ten.date = {2026U, 10U, 14U};
    ten.hour = 20U;
    ten.count = kKickCountToTenTarget;
    ten.period = KickPeriod::Evening;
    ten.mode = KickMode::CountToTen;
    ten.elapsed_seconds = 754U;
    ten.duration_minutes = 13U;
    EncodedPayload encoded = {};
    assert(encode_kick(ten, encoded) == RecordCodecStatus::Ok);
    KickSession decoded = {};
    assert(decode_kick(encoded.bytes.data(), encoded.size, decoded) == RecordCodecStatus::Ok);
    assert(decoded.mode == KickMode::CountToTen && decoded.elapsed_seconds == 754U);
    assert(decoded.feel == KickFeel::None && decoded.count == 10U);

    KickSession feel = {};
    feel.id = 8U;
    feel.date = ten.date;
    feel.mode = KickMode::Awareness;
    feel.feel = KickFeel::Weaker;
    assert(encode_kick(feel, encoded) == RecordCodecStatus::Ok);
    assert(decode_kick(encoded.bytes.data(), encoded.size, decoded) == RecordCodecStatus::Ok);
    assert(decoded.mode == KickMode::Awareness && decoded.feel == KickFeel::Weaker);

    // Invalid combinations never reach the card.
    feel.count = 1U;
    assert(encode_kick(feel, encoded) == RecordCodecStatus::InvalidArgument);
    feel.count = 0U;
    feel.feel = KickFeel::None;
    assert(encode_kick(feel, encoded) == RecordCodecStatus::InvalidArgument);
    ten.count = 11U;
    assert(encode_kick(ten, encoded) == RecordCodecStatus::InvalidArgument);
    ten.count = 10U;
    ten.feel = KickFeel::Usual;
    assert(encode_kick(ten, encoded) == RecordCodecStatus::InvalidArgument);
}

void test_kick_version_one_payload()
{
    using namespace pregnancy_repository;
    // Hand-built schema-1 body: no mode, feel or elapsed-seconds fields.
    uint8_t bytes[64] = {};
    size_t size = 0U;
    auto u8 = [&](uint8_t value) { bytes[size++] = value; };
    auto u16 = [&](uint16_t value) { u8(static_cast<uint8_t>(value)); u8(static_cast<uint8_t>(value >> 8U)); };
    auto u32 = [&](uint32_t value) { u16(static_cast<uint16_t>(value)); u16(static_cast<uint16_t>(value >> 16U)); };
    u32(0x314B434BU);
    u16(1U);
    u16(0U);
    u8(1U);
    u32(5U);
    u16(2026U); u8(10U); u8(9U);
    u8(9U); u8(30U);
    u16(45U);
    u16(12U);
    u8(static_cast<uint8_t>(KickPeriod::Morning));
    u8(0U);
    const uint16_t body = static_cast<uint16_t>(size - 8U);
    bytes[6] = static_cast<uint8_t>(body);
    bytes[7] = static_cast<uint8_t>(body >> 8U);
    u32(sticky_storage::crc32(bytes, size));
    KickSession decoded = {};
    assert(decode_kick(bytes, size, decoded) == RecordCodecStatus::Ok);
    assert(decoded.schema_version == kKickSessionSchemaVersion);
    assert(decoded.mode == KickMode::Timed && decoded.feel == KickFeel::None);
    assert(decoded.count == 12U && decoded.elapsed_seconds == 45U * 60U);
}

void test_pregnancy()
{
    using namespace pregnancy_repository;
    PregnancyProfile source = {};
    assert(pregnancy_profile_from_due_date({2027U, 3U, 18U}, source));
    EncodedPayload payload = {};
    assert(encode_pregnancy_profile(source, payload) == RecordCodecStatus::Ok);
    assert(payload.size == 22U);
    PregnancyProfile decoded = {};
    assert(decode_pregnancy_profile(payload.bytes.data(), payload.size, decoded) ==
           RecordCodecStatus::Ok);
    assert(decoded.schema_version == source.schema_version);
    assert(same_date(decoded.last_menstrual_period,
                     source.last_menstrual_period));
    assert(same_date(decoded.estimated_due_date, source.estimated_due_date));
    assert(decoded.use_due_date_as_primary);

    payload.bytes[payload.size - 1U] ^= 0x80U;
    assert(decode_pregnancy_profile(payload.bytes.data(), payload.size, decoded) ==
           RecordCodecStatus::Corrupt);
    assert(decode_pregnancy_profile(nullptr, 0U, decoded) ==
           RecordCodecStatus::InvalidArgument);
}

void test_reminder()
{
    using namespace pregnancy_repository;
    Reminder source = {};
    source.id = 19U;
    source.type = ReminderType::Supplement;
    source.date = {2026U, 10U, 2U};
    source.hour = 8U;
    source.minute = 35U;
    source.repeat_rule = RepeatRule::Daily;
    source.enabled = false;
    source.completed = true;
    std::strcpy(source.title, "Vitamin & water");
    std::strcpy(source.note, "after breakfast");
    EncodedPayload payload = {};
    assert(encode_reminder(source, payload) == RecordCodecStatus::Ok);
    Reminder decoded = {};
    assert(decode_reminder(payload.bytes.data(), payload.size, decoded) ==
           RecordCodecStatus::Ok);
    assert(decoded.id == source.id && decoded.type == source.type);
    assert(same_date(decoded.date, source.date));
    assert(decoded.hour == source.hour && decoded.minute == source.minute);
    assert(decoded.repeat_rule == source.repeat_rule);
    assert(!decoded.enabled && decoded.completed);
    assert(std::strcmp(decoded.title, source.title) == 0);
    assert(std::strcmp(decoded.note, source.note) == 0);

    Reminder unterminated = source;
    std::memset(unterminated.title, 'x', sizeof(unterminated.title));
    assert(encode_reminder(unterminated, payload) ==
           RecordCodecStatus::InvalidArgument);
}

void test_checkup()
{
    using namespace pregnancy_repository;
    Checkup source = {};
    source.id = 5U;
    source.date = {2026U, 11U, 9U};
    source.hour = 14U;
    source.minute = 20U;
    source.recommended_week = 24U;
    source.completed = true;
    source.reference_template = true;
    std::strcpy(source.title, "Routine visit");
    std::strcpy(source.location, "Clinic 2");
    std::strcpy(source.note, "bring records");
    EncodedPayload payload = {};
    assert(encode_checkup(source, payload) == RecordCodecStatus::Ok);
    Checkup decoded = {};
    assert(decode_checkup(payload.bytes.data(), payload.size, decoded) ==
           RecordCodecStatus::Ok);
    assert(decoded.id == source.id && same_date(decoded.date, source.date));
    assert(decoded.hour == source.hour && decoded.minute == source.minute);
    assert(decoded.recommended_week == source.recommended_week);
    assert(decoded.completed && decoded.reference_template);
    assert(std::strcmp(decoded.title, source.title) == 0);
    assert(std::strcmp(decoded.location, source.location) == 0);
    assert(std::strcmp(decoded.note, source.note) == 0);
}

void test_weight_and_preferences()
{
    using namespace pregnancy_repository;
    WeightRecord source = {};
    source.id = 31U;
    source.date = {2026U, 9U, 30U};
    source.weight_tenths_kg = 623U;
    std::strcpy(source.note, "morning");
    EncodedPayload payload = {};
    assert(encode_weight(source, payload) == RecordCodecStatus::Ok);
    WeightRecord decoded = {};
    assert(decode_weight(payload.bytes.data(), payload.size, decoded) ==
           RecordCodecStatus::Ok);
    assert(decoded.id == source.id && same_date(decoded.date, source.date));
    assert(decoded.weight_tenths_kg == source.weight_tenths_kg);
    assert(std::strcmp(decoded.note, source.note) == 0);

    WeightProfileSettings weight_settings = {};
    weight_settings.height_cm = 168U;
    weight_settings.baseline_tenths_kg = 584U;
    weight_settings.use_pounds = true;
    assert(encode_weight_preferences(weight_settings, payload) ==
           RecordCodecStatus::Ok);
    WeightProfileSettings decoded_weight_settings = {};
    assert(decode_weight_preferences(payload.bytes.data(), payload.size,
                                     decoded_weight_settings) ==
           RecordCodecStatus::Ok);
    assert(decoded_weight_settings.height_cm == weight_settings.height_cm);
    assert(decoded_weight_settings.baseline_tenths_kg ==
           weight_settings.baseline_tenths_kg);
    assert(decoded_weight_settings.use_pounds);

    weight_settings.height_cm = 99U;
    assert(encode_weight_preferences(weight_settings, payload) ==
           RecordCodecStatus::InvalidArgument);
}

void test_device_preferences_and_envelope_rejection()
{
    using namespace pregnancy_repository;
    StickyDeviceSettings source = {};
    source.language = UiLanguage::ChineseSimplified;
    source.last_app = StickyAppId::Weight;
    EncodedPayload payload = {};
    assert(encode_device_preferences(source, payload) == RecordCodecStatus::Ok);
    // Explicit wire representation: header + two-byte body + CRC32.
    assert(payload.size == 14U);
    assert(payload.bytes[0] == 'D' && payload.bytes[1] == 'F' &&
           payload.bytes[2] == 'P' && payload.bytes[3] == '1');
    assert(payload.bytes[4] == 1U && payload.bytes[5] == 0U);
    assert(payload.bytes[6] == 2U && payload.bytes[7] == 0U);
    assert(payload.bytes[8] == 1U && payload.bytes[9] == 7U);

    StickyDeviceSettings decoded = {};
    assert(decode_device_preferences(payload.bytes.data(), payload.size, decoded) ==
           RecordCodecStatus::Ok);
    assert(decoded.language == source.language && decoded.last_app == source.last_app);

    EncodedPayload newer = payload;
    newer.bytes[4] = 2U;
    assert(decode_device_preferences(newer.bytes.data(), newer.size, decoded) ==
           RecordCodecStatus::UnsupportedVersion);
    assert(decode_device_preferences(payload.bytes.data(), payload.size - 1U,
                                     decoded) == RecordCodecStatus::Corrupt);
    assert(decode_device_preferences(payload.bytes.data(), payload.size + 1U,
                                     decoded) == RecordCodecStatus::Corrupt);

    source.last_app = static_cast<StickyAppId>(99);
    assert(encode_device_preferences(source, payload) ==
           RecordCodecStatus::InvalidArgument);
}

void test_network_authorization()
{
    using namespace pregnancy_repository;
    NetworkAuthorizationSettings source = {};
    source.cloud_authorized = true;
    source.authorization_version = 4U;
    source.provider_config_version = 2U;
    std::strcpy(source.provider_id, "coze");
    std::strcpy(source.service_region, "cn");
    for (size_t index = 0U; index < sizeof(source.credentials.device_id); ++index)
        source.credentials.device_id[index] = static_cast<uint8_t>(index + 1U);
    for (size_t index = 0U; index < sizeof(source.credentials.nonce); ++index)
        source.credentials.nonce[index] = static_cast<uint8_t>(index + 10U);
    source.credentials.ciphertext_size = 5U;
    std::memcpy(source.credentials.ciphertext, "abcde", 5U);
    for (size_t index = 0U; index < sizeof(source.credentials.tag); ++index)
        source.credentials.tag[index] = static_cast<uint8_t>(index + 20U);
    assert(network_authorization_settings_valid(source));

    EncodedPayload payload = {};
    assert(encode_network_authorization(source, payload) ==
           RecordCodecStatus::Ok);
    NetworkAuthorizationSettings decoded = {};
    assert(decode_network_authorization(payload.bytes.data(), payload.size,
                                        decoded) == RecordCodecStatus::Ok);
    assert(decoded.cloud_authorized &&
           decoded.authorization_version == source.authorization_version);
    assert(std::strcmp(decoded.provider_id, "coze") == 0);
    assert(decoded.credentials.ciphertext_size == 5U);
    assert(std::memcmp(decoded.credentials.ciphertext, "abcde", 5U) == 0);

    NetworkAuthorizationSettings disabled = {};
    assert(network_authorization_settings_valid(disabled));
    assert(encode_network_authorization(disabled, payload) ==
           RecordCodecStatus::Ok);
    assert(decode_network_authorization(payload.bytes.data(), payload.size,
                                        decoded) == RecordCodecStatus::Ok);
    assert(!decoded.cloud_authorized);

    source.credentials.device_id[0] = 0U;
    std::memset(source.credentials.device_id, 0,
                sizeof(source.credentials.device_id));
    assert(encode_network_authorization(source, payload) ==
           RecordCodecStatus::InvalidArgument);
}

}  // namespace

int main()
{
    test_keys();
    test_kicks();
    test_kick_modes();
    test_kick_version_one_payload();
    test_pregnancy();
    test_reminder();
    test_checkup();
    test_weight_and_preferences();
    test_device_preferences_and_envelope_rejection();
    test_network_authorization();
    return 0;
}
