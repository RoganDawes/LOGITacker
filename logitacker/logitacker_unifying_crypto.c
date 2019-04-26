#include "logitacker_unifying_crypto.h"
#include "sdk_common.h"
#include "nrf_crypto.h"
#include "nrf_crypto_error.h"

#define NRF_LOG_MODULE_NAME LOGITACKER_UNIFYING_CRYPTO
#include "nrf_log.h"

NRF_LOG_MODULE_REGISTER();



nrf_crypto_aes_info_t const * p_ecb_info = &g_nrf_crypto_aes_ecb_128_info;
static nrf_crypto_aes_context_t * p_ecb_encr_ctx = NULL;


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
        NRF_LOG_INFO("cipher (%d bytes):", len_out);
        NRF_LOG_HEXDUMP_INFO(cipher, 16);
    }

    return ret_val;
}