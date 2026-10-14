#include "network_authorization.h"

#include <cstring>

namespace {

bool terminated(const char *value, size_t capacity)
{
    return value != nullptr && std::memchr(value, '\0', capacity) != nullptr;
}

bool nonzero(const uint8_t *value, size_t size)
{
    uint8_t combined = 0U;
    for (size_t index = 0U; index < size; ++index) combined |= value[index];
    return combined != 0U;
}

void secure_clear(void *value, size_t size)
{
    volatile uint8_t *bytes = static_cast<volatile uint8_t *>(value);
    while (size-- > 0U) *bytes++ = 0U;
}

}  // namespace

bool bound_credential_envelope_valid(const BoundCredentialEnvelope &value)
{
    return value.schema_version == kBoundCredentialSchemaVersion &&
           value.key_version != 0U && value.ciphertext_size > 0U &&
           value.ciphertext_size <= kCredentialCiphertextBytes &&
           nonzero(value.device_id, sizeof(value.device_id)) &&
           nonzero(value.nonce, sizeof(value.nonce)) &&
           nonzero(value.tag, sizeof(value.tag));
}

bool network_authorization_settings_valid(
    const NetworkAuthorizationSettings &value)
{
    if (value.schema_version != kNetworkAuthorizationSchemaVersion ||
        !terminated(value.provider_id, sizeof(value.provider_id)) ||
        !terminated(value.service_region, sizeof(value.service_region))) {
        return false;
    }
    if (!value.cloud_authorized) {
        return value.authorization_version == 0U &&
               value.provider_config_version == 0U &&
               value.provider_id[0] == '\0' && value.service_region[0] == '\0' &&
               value.credentials.ciphertext_size == 0U;
    }
    return value.authorization_version != 0U &&
           value.provider_config_version != 0U &&
           value.provider_id[0] != '\0' && value.service_region[0] != '\0' &&
           bound_credential_envelope_valid(value.credentials);
}

bool network_credential_plaintext_valid(
    const NetworkCredentialPlaintext &value)
{
    return terminated(value.ssid, sizeof(value.ssid)) &&
           terminated(value.password, sizeof(value.password)) &&
           terminated(value.gateway_device_token,
                      sizeof(value.gateway_device_token)) &&
           value.ssid[0] != '\0' && value.gateway_device_token[0] != '\0';
}

void network_credential_plaintext_clear(NetworkCredentialPlaintext &value)
{
    secure_clear(&value, sizeof(value));
}
