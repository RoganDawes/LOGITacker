#include "logitacker_unifying_crypto.h"
#include "sdk_common.h"
#include "nrf_crypto.h"
#include "nrf_crypto_error.h"

#define NRF_LOG_MODULE_NAME LOGITACKER_UNIFYING_CRYPTO
#include "nrf_log.h"

NRF_LOG_MODULE_REGISTER();


static uint8_t little_known_secret[16] = { 0x04, 0x14, 0x1d, 0x1f, 0x27, 0x28, 0x0d, 0xde, 0xad, 0xbe, 0xef, 0x0a, 0x0d, 0x13, 0x26, 0x0e };

nrf_crypto_aes_info_t const * p_ecb_info = &g_nrf_crypto_aes_ecb_128_info;
static nrf_crypto_aes_context_t * p_ecb_encr_ctx = NULL;

void update_little_known_secret_counter(uint8_t const * const counter) {
    memcpy(&little_known_secret[7], counter, 4);
}

uint32_t logitacker_unifying_crypto_aes_ecb_encrypt(uint8_t *cipher, uint8_t * key, uint8_t * plain) {
    size_t len_in = 16;
    size_t len_out =16;
    ret_code_t ret_val;

    ret_val = nrf_crypto_aes_crypt(p_ecb_encr_ctx,
                                   p_ecb_info,
                                   NRF_CRYPTO_ENCRYPT,
                                   key,
                                   NULL,
                                   plain,
                                   len_in,
                                   cipher,
                                   &len_out);
    if (ret_val != NRF_SUCCESS) {
        NRF_LOG_WARNING("error nrf_crypto_aes_crypt: %d", ret_val);
    } else {
        NRF_LOG_DEBUG("cipher (%d bytes):", len_out);
        NRF_LOG_HEXDUMP_DEBUG(cipher, 16);
    }

    return ret_val;
}

uint32_t logitacker_unifying_crypto_calculate_frame_key(uint8_t * result, uint8_t * device_key, uint8_t * counter) {
    update_little_known_secret_counter(counter);
    return logitacker_unifying_crypto_aes_ecb_encrypt(result, device_key, little_known_secret);
}

uint32_t logitacker_unifying_crypto_decrypt_encrypted_keyboard_frame(uint8_t * result, uint8_t * device_key, nrf_esb_payload_t * rf_frame) {
    // validate frame
    VERIFY_PARAM_NOT_NULL(result);
    VERIFY_PARAM_NOT_NULL(device_key);
    VERIFY_PARAM_NOT_NULL(rf_frame);
    VERIFY_TRUE(rf_frame->length == 22, NRF_ERROR_INVALID_DATA);
    VERIFY_TRUE((rf_frame->data[1] & 0x1f) == 0x13, NRF_ERROR_INVALID_DATA); //encrypted keyboard frame

    ret_code_t ret_val;


    // extract counter
    uint8_t counter[4] = { 0 };
    memcpy(counter, &rf_frame->data[10], 4);

    //generate frame key
    uint8_t frame_key[16] = { 0 };
    ret_val = logitacker_unifying_crypto_calculate_frame_key(frame_key, device_key, counter);
    VERIFY_SUCCESS(ret_val);

    //copy cipher part to result
    memcpy(result, &rf_frame->data[2], 8);

    // xor decrypt with relevant part of AES key (yes, only half of the key - generated from high-entropy input data - is used)
    for (int i=0; i<8; i++) {
        result[i] ^= frame_key[i];
    }

    return NRF_SUCCESS;
}