#pragma once

#include <cstddef>
#include <cstdint>

constexpr uint8_t kNetworkAuthorizationSchemaVersion = 1U;
constexpr uint8_t kBoundCredentialSchemaVersion = 1U;
constexpr size_t kCredentialDeviceIdBytes = 8U;
constexpr size_t kCredentialNonceBytes = 12U;
constexpr size_t kCredentialTagBytes = 16U;
constexpr size_t kCredentialCiphertextBytes = 256U;

struct BoundCredentialEnvelope {
    uint8_t schema_version = kBoundCredentialSchemaVersion;
    uint16_t key_version = 1U;
    uint8_t device_id[kCredentialDeviceIdBytes] = {};
    uint8_t nonce[kCredentialNonceBytes] = {};
    uint16_t ciphertext_size = 0U;
    uint8_t ciphertext[kCredentialCiphertextBytes] = {};
    uint8_t tag[kCredentialTagBytes] = {};
};

struct NetworkAuthorizationSettings {
    uint8_t schema_version = kNetworkAuthorizationSchemaVersion;
    bool cloud_authorized = false;
    uint32_t authorization_version = 0U;
    uint32_t provider_config_version = 0U;
    char provider_id[16] = {};
    char service_region[16] = {};
    BoundCredentialEnvelope credentials = {};
};

struct NetworkCredentialPlaintext {
    char ssid[33] = {};
    char password[65] = {};
    char gateway_device_token[129] = {};
};

bool bound_credential_envelope_valid(const BoundCredentialEnvelope &value);
bool network_authorization_settings_valid(
    const NetworkAuthorizationSettings &value);
bool network_credential_plaintext_valid(
    const NetworkCredentialPlaintext &value);
void network_credential_plaintext_clear(NetworkCredentialPlaintext &value);
