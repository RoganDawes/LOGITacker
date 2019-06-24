#include "helper.h"
#include "crc16.h"
#include "app_util_platform.h"
#include "nrf_log.h"


void helper_array_shl(uint8_t *p_array, uint8_t len, uint8_t bits) {
    if (bits == 0) return;
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

void helper_array_shl_cpy(uint8_t * out_array, uint8_t * in_array, uint8_t len, uint8_t bits) {
    if (bits == 0) {
        memcpy(out_array, in_array, len);
        return;
    }

    if ((bits & 0x08) == 0) {
        uint8_t byteoff = bits / 8;
        memcpy(out_array, &in_array[byteoff], len-byteoff);
        return;
    }

    if (len == 1) {
        out_array[0] = in_array[0] << bits;
        return;
    }

    uint8_t remainder_shift = (8-bits);
    for (uint8_t i=0; i<(len-1); i++) {
        out_array[i] = in_array[i] << bits | in_array[i+1] >> remainder_shift;
    }
    out_array[len-1] = in_array[len-1] << bits;
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
    out_addr[in_addr_len-1] = in_prefix;

}


void helper_addr_to_hex_str(char * p_result, uint8_t len, uint8_t const * const p_addr)
{
    ASSERT(p_result);
    ASSERT(p_addr);

    if (len > 5) len = 5;

    char buffer[4] = {0};

    // ToDo: This line is bullshit, at leat the first byte should be 0x00 to make strcat work
    memset(p_result, 0, len);

    for (uint8_t i = 0; i < len; ++i)
    {
        sprintf(buffer, "%.2X", p_addr[i]);
        strcat(p_result, buffer);

        if (i < (len - 1)) strcat(p_result, ":");
    }
}

uint32_t helper_hex_str_to_addr(uint8_t * p_result_addr, uint8_t len, char const * const addr_str) {
    ASSERT(p_result_addr);
    ASSERT(addr_str);

    int tmp;
    for (int i=0; i<len; i++) {
        if (sscanf(&addr_str[i*3], "%02x", &tmp) != 1) return NRF_ERROR_INVALID_PARAM;
        p_result_addr[i] = (uint8_t) tmp;
    }

    NRF_LOG_INFO("parsed addr len %d:", len);
    NRF_LOG_HEXDUMP_INFO(p_result_addr, len);
    return NRF_SUCCESS;
}

uint32_t helper_hex_str_to_bytes(uint8_t * p_result, uint8_t len, char const * const hex_str) {
    ASSERT(p_result);
    ASSERT(hex_str);

    int tmp;
    for (int i=0; i<len; i++) {
        if (sscanf(&hex_str[i*2], "%02x", &tmp) != 1) return NRF_ERROR_INVALID_PARAM;
        p_result[i] = (uint8_t) tmp;
        //NRF_LOG_INFO("tmp: %02x", tmp);
    }

    NRF_LOG_INFO("parsed len %d:", len);
    NRF_LOG_HEXDUMP_INFO(p_result, len);
    return NRF_SUCCESS;
}

// Return the next DELIM-delimited token from *STRINGP terminating it with a '\0', and update *STRINGP to point past it.

char *helper_strsep (char **stringp, const char *delim) {
    char *token_start, *token_end;
    token_start = *stringp;
    if (token_start == NULL)
        return NULL;
    /* Find the end of the token.  */
    token_end = token_start + strcspn (token_start, delim);
    if (*token_end)
    {
        // Terminate the token and set *STRINGP past NUL character.
        *token_end++ = '\0';
        *stringp = token_end;
    }
    else
        // No more delimiters; this is the last token.
        *stringp = NULL;
    return token_start;
}
