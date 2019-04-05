#include "helper.h"
#include "crc16.h"

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