#ifndef LOGITACKER_UNIFYING_CRYPTO_H
#define LOGITACKER_UNIFYING_CRYPTO_H

#include <nrf_esb_illegalmod.h>
#include "stdint.h"

uint32_t logitacker_unifying_crypto_aes_ecb_encrypt(uint8_t *cipher, uint8_t * key, uint8_t * plain);
uint32_t logitacker_unifying_crypto_calculate_frame_key(uint8_t * result, uint8_t * device_key, uint8_t * counter);
uint32_t logitacker_unifying_crypto_decrypt_encrypted_keyboard_frame(uint8_t * result, uint8_t * device_key, nrf_esb_payload_t * rf_frame);

#endif //LOGITACKER_UNIFYING_CRYPTO_H
