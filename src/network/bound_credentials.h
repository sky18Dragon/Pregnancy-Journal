#pragma once

#include "esp_err.h"
#include "network_authorization.h"

// AES-256-GCM envelope bound to this device's locally generated root key and
// station MAC identity. Plaintext exists only in caller-owned RAM and must be
// cleared after use. The root key is device-local cryptographic material, not
// user data; it is the only NVS write in this module.
esp_err_t bound_credentials_seal(const NetworkCredentialPlaintext &plaintext,
                                 BoundCredentialEnvelope &envelope);
esp_err_t bound_credentials_unseal(const BoundCredentialEnvelope &envelope,
                                   NetworkCredentialPlaintext &plaintext);
esp_err_t bound_credentials_device_key_present(bool &present);

// Device reset action: forgets the device-local wrapping key. It never mounts
// or deletes MicroSD content. Existing card envelopes become intentionally
// unusable and require fresh network provisioning/authorization.
esp_err_t bound_credentials_reset_device_key();
