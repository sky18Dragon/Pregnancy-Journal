#include "bound_credentials.h"

#include <cstring>

#include "esp_mac.h"
#include "esp_random.h"
#include "mbedtls/gcm.h"
#include "mbedtls/sha256.h"
#include "nvs.h"

namespace {

constexpr char kNamespace[] = "sticky_keys";
constexpr char kRootKeyName[] = "net_root";
constexpr size_t kRootKeyBytes = 32U;
constexpr size_t kPlaintextBytes = 3U + 32U + 64U + 128U;

void secure_clear(void *value, size_t size)
{
    volatile uint8_t *bytes = static_cast<volatile uint8_t *>(value);
    while (size-- > 0U) *bytes++ = 0U;
}

esp_err_t load_or_create_root(uint8_t root[kRootKeyBytes])
{
    nvs_handle_t handle = 0;
    esp_err_t result = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (result != ESP_OK) return result;
    size_t size = kRootKeyBytes;
    result = nvs_get_blob(handle, kRootKeyName, root, &size);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        esp_fill_random(root, kRootKeyBytes);
        result = nvs_set_blob(handle, kRootKeyName, root, kRootKeyBytes);
        if (result == ESP_OK) result = nvs_commit(handle);
    } else if (result == ESP_OK && size != kRootKeyBytes) {
        result = ESP_ERR_INVALID_SIZE;
    }
    nvs_close(handle);
    return result;
}

esp_err_t load_root(uint8_t root[kRootKeyBytes])
{
    nvs_handle_t handle = 0;
    esp_err_t result = nvs_open(kNamespace, NVS_READONLY, &handle);
    if (result != ESP_OK) return result;
    size_t size = kRootKeyBytes;
    result = nvs_get_blob(handle, kRootKeyName, root, &size);
    if (result == ESP_OK && size != kRootKeyBytes)
        result = ESP_ERR_INVALID_SIZE;
    nvs_close(handle);
    return result;
}

esp_err_t current_device_id(uint8_t output[kCredentialDeviceIdBytes])
{
    uint8_t mac[6] = {};
    esp_err_t result = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (result != ESP_OK) return result;
    uint8_t digest[32] = {};
    result = mbedtls_sha256(mac, sizeof(mac), digest, 0) == 0
                 ? ESP_OK
                 : ESP_FAIL;
    if (result == ESP_OK)
        std::memcpy(output, digest, kCredentialDeviceIdBytes);
    secure_clear(digest, sizeof(digest));
    return result;
}

size_t encode_plaintext(const NetworkCredentialPlaintext &value,
                        uint8_t output[kPlaintextBytes])
{
    const size_t ssid = std::strlen(value.ssid);
    const size_t password = std::strlen(value.password);
    const size_t token = std::strlen(value.gateway_device_token);
    if (ssid > 32U || password > 64U || token > 128U) return 0U;
    size_t offset = 0U;
    output[offset++] = static_cast<uint8_t>(ssid);
    std::memcpy(output + offset, value.ssid, ssid);
    offset += ssid;
    output[offset++] = static_cast<uint8_t>(password);
    std::memcpy(output + offset, value.password, password);
    offset += password;
    output[offset++] = static_cast<uint8_t>(token);
    std::memcpy(output + offset, value.gateway_device_token, token);
    return offset + token;
}

bool decode_plaintext(const uint8_t *bytes, size_t size,
                      NetworkCredentialPlaintext &value)
{
    value = {};
    size_t offset = 0U;
    auto read = [&](char *output, size_t capacity) {
        if (offset >= size) return false;
        const size_t length = bytes[offset++];
        if (length >= capacity || length > size - offset) return false;
        std::memcpy(output, bytes + offset, length);
        output[length] = '\0';
        offset += length;
        return true;
    };
    return read(value.ssid, sizeof(value.ssid)) &&
           read(value.password, sizeof(value.password)) &&
           read(value.gateway_device_token,
                sizeof(value.gateway_device_token)) &&
           offset == size && network_credential_plaintext_valid(value);
}

void make_aad(const BoundCredentialEnvelope &envelope, uint8_t output[11])
{
    output[0] = envelope.schema_version;
    output[1] = static_cast<uint8_t>(envelope.key_version);
    output[2] = static_cast<uint8_t>(envelope.key_version >> 8U);
    std::memcpy(output + 3U, envelope.device_id, kCredentialDeviceIdBytes);
}

}  // namespace

esp_err_t bound_credentials_seal(const NetworkCredentialPlaintext &plaintext,
                                 BoundCredentialEnvelope &envelope)
{
    envelope = {};
    if (!network_credential_plaintext_valid(plaintext))
        return ESP_ERR_INVALID_ARG;
    uint8_t root[kRootKeyBytes] = {};
    uint8_t clear[kPlaintextBytes] = {};
    esp_err_t result = load_or_create_root(root);
    if (result != ESP_OK) goto done;
    result = current_device_id(envelope.device_id);
    if (result != ESP_OK) goto done;
    esp_fill_random(envelope.nonce, sizeof(envelope.nonce));
    envelope.schema_version = kBoundCredentialSchemaVersion;
    envelope.key_version = 1U;
    envelope.ciphertext_size =
        static_cast<uint16_t>(encode_plaintext(plaintext, clear));
    if (envelope.ciphertext_size == 0U) {
        result = ESP_ERR_INVALID_ARG;
        goto done;
    }
    {
        uint8_t aad[11] = {};
        make_aad(envelope, aad);
        mbedtls_gcm_context context;
        mbedtls_gcm_init(&context);
        const int key_result = mbedtls_gcm_setkey(
            &context, MBEDTLS_CIPHER_ID_AES, root, kRootKeyBytes * 8U);
        const int crypt_result = key_result == 0
            ? mbedtls_gcm_crypt_and_tag(
                  &context, MBEDTLS_GCM_ENCRYPT,
                  envelope.ciphertext_size, envelope.nonce,
                  sizeof(envelope.nonce), aad, sizeof(aad), clear,
                  envelope.ciphertext, sizeof(envelope.tag), envelope.tag)
            : key_result;
        mbedtls_gcm_free(&context);
        secure_clear(aad, sizeof(aad));
        result = crypt_result == 0 ? ESP_OK : ESP_FAIL;
    }
done:
    secure_clear(clear, sizeof(clear));
    secure_clear(root, sizeof(root));
    if (result != ESP_OK) envelope = {};
    return result;
}

esp_err_t bound_credentials_unseal(const BoundCredentialEnvelope &envelope,
                                   NetworkCredentialPlaintext &plaintext)
{
    plaintext = {};
    if (!bound_credential_envelope_valid(envelope))
        return ESP_ERR_INVALID_ARG;
    uint8_t root[kRootKeyBytes] = {};
    uint8_t clear[kCredentialCiphertextBytes] = {};
    uint8_t device_id[kCredentialDeviceIdBytes] = {};
    esp_err_t result = current_device_id(device_id);
    if (result != ESP_OK ||
        std::memcmp(device_id, envelope.device_id, sizeof(device_id)) != 0) {
        result = ESP_ERR_INVALID_STATE;
        goto done;
    }
    result = load_root(root);
    if (result != ESP_OK) goto done;
    {
        uint8_t aad[11] = {};
        make_aad(envelope, aad);
        mbedtls_gcm_context context;
        mbedtls_gcm_init(&context);
        const int key_result = mbedtls_gcm_setkey(
            &context, MBEDTLS_CIPHER_ID_AES, root, kRootKeyBytes * 8U);
        const int crypt_result = key_result == 0
            ? mbedtls_gcm_auth_decrypt(
                  &context, envelope.ciphertext_size, envelope.nonce,
                  sizeof(envelope.nonce), aad, sizeof(aad), envelope.tag,
                  sizeof(envelope.tag), envelope.ciphertext, clear)
            : key_result;
        mbedtls_gcm_free(&context);
        secure_clear(aad, sizeof(aad));
        if (crypt_result != 0 ||
            !decode_plaintext(clear, envelope.ciphertext_size, plaintext)) {
            result = ESP_ERR_INVALID_CRC;
        } else {
            result = ESP_OK;
        }
    }
done:
    secure_clear(device_id, sizeof(device_id));
    secure_clear(clear, sizeof(clear));
    secure_clear(root, sizeof(root));
    if (result != ESP_OK) network_credential_plaintext_clear(plaintext);
    return result;
}

esp_err_t bound_credentials_device_key_present(bool &present)
{
    present = false;
    nvs_handle_t handle = 0;
    esp_err_t result = nvs_open(kNamespace, NVS_READONLY, &handle);
    if (result == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (result != ESP_OK) return result;
    size_t size = 0U;
    result = nvs_get_blob(handle, kRootKeyName, nullptr, &size);
    nvs_close(handle);
    if (result == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (result != ESP_OK) return result;
    if (size != kRootKeyBytes) return ESP_ERR_INVALID_SIZE;
    present = true;
    return ESP_OK;
}

esp_err_t bound_credentials_reset_device_key()
{
    bool present = false;
    esp_err_t result = bound_credentials_device_key_present(present);
    if (result != ESP_OK || !present) return result;
    nvs_handle_t handle = 0;
    result = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (result != ESP_OK) return result;
    result = nvs_erase_key(handle, kRootKeyName);
    if (result == ESP_ERR_NVS_NOT_FOUND) result = ESP_OK;
    if (result == ESP_OK) result = nvs_commit(handle);
    nvs_close(handle);
    return result;
}
