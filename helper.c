#include "helper.h"
#include "crc16.h"
#include "app_util_platform.h"
#include "nrf_log.h"


void helper_array_shl(uint8_t *p_array, uint8_t len, uint8_t bits) {
    if (len == 1) {
        p_array[0] = p_array[0] << bits;
        return;
    }
    
    for (uint8_t i=0; i<(len-1); i++) {
        p_array[i] = p_array[i] << bits | p_array[i+1] >> (8-bits);
    }
    p_array[len-1] = p_array[len-1] << bits;
    return;
}

bool helper_array_check_crc16(uint8_t * p_array, uint8_t len) {
    uint16_t crc = crc16_compute(p_array, (uint32_t) len, NULL);

    if (crc == 0x0000) {
        return true;
    }

    return false;
}

void helper_log_priority(char* source) {
    if (current_int_priority_get() == APP_IRQ_PRIORITY_THREAD) {
        NRF_LOG_INFO("%s: Running in Thread/main mode", source);
    } else {
        NRF_LOG_INFO("%s: Running in Interrupt mode", source);
    } 
}

void helper_addr_to_base_and_prefix(uint8_t *out_base_addr, uint8_t *out_prefix, uint8_t const *in_addr, uint8_t in_addr_len) {
    int pos = 0;
    for (int i=in_addr_len-2; i >= 0; i--) {
        out_base_addr[pos++] = in_addr[i];
    }
    *out_prefix = in_addr[in_addr_len-1];
    
}

void helper_base_and_prefix_to_addr(uint8_t *out_addr, uint8_t const *in_base_addr, uint8_t in_prefix, uint8_t in_addr_len) {
    int pos = 0;
    for (int i=in_addr_len-2; i >= 0; i--) {
        out_addr[i] = in_base_addr[pos++];
    }
    out_addr[in_addr_len-1] = in_base_addr[pos++];

}


void helper_addr_to_hex_str(char * p_result, uint8_t len, uint8_t const * const p_addr)
{
    ASSERT(p_result);
    ASSERT(p_addr);

    if (len > 5) len = 5;

    char buffer[4] = {0};

    memset(p_result, 0, len);

    for (uint8_t i = 0; i < len; ++i)
    {
        sprintf(buffer, "%.2X", p_addr[i]);
        strcat(p_result, buffer);

        if (i < (len - 1)) strcat(p_result, ":");
    }
}


