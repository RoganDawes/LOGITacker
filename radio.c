#include <string.h> //memcpy, memset
#include <stddef.h> //NULL

#include "radio.h"
#include "crc16.h"
#include "nrf_error.h"

//#include "bsp.h"

bool check_crc16(uint8_t * p_array, uint8_t len) {
    uint16_t crc = crc16_compute(p_array, (uint32_t) len, NULL);

    if (crc == 0x0000) {
        return true;
    }

    return false;
}

bool validate_esb_frame(uint8_t * p_array, uint8_t addrlen) {
    uint8_t framelen = p_array[addrlen] >> 2;
    if (framelen > 32) {
        return false; // early out if ESB frame has a length > 32, this only accounts for "old style" ESB which is bound to 32 byte max payload length
    }
    uint8_t crclen = addrlen + 1 + framelen + 2; //+1 for PCF (we ignore one bit), +2 for crc16

    return check_crc16(p_array, crclen);
}

void array_shl(uint8_t *p_array, uint8_t len, uint8_t bits) {
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

#define VALIDATION_SHIFT_BUF_SIZE 64
uint32_t validate_esb_payload(nrf_esb_payload_t * p_payload) {
    uint8_t assumed_addrlen = 5; //Validation has to take RF address length of raw frames into account, thus we assume the given length in byte
    uint8_t tmpData[VALIDATION_SHIFT_BUF_SIZE];
    uint8_t tmpDataLen = p_payload->length;
    static bool crcmatch = false;

    for (uint8_t i=0; i< tmpDataLen; i++) {
        tmpData[i] = p_payload -> data[i];
    }
    /*
    report[0] = rx_payload.pipe;
    report[1] = (uint8_t) ch;
    report[2] = rx_payload.length;
    */

    // if processing takes too long RF frames are discarded
    // the shift value in the following for loop controls how often a received
    // frame is shifted for CRC check. If this value is too large, frames are dropped,
    // if it is too low, chance for detecting valid frames decreases.
    // The validate_esb_frame function has an early out, if determined ESB frame length
    // exceeds 32 byte, which avoids unnecessary CRC16 calculations.
    crcmatch = false;
    for (uint8_t shift=0; shift<32; shift++) {
        if (validate_esb_frame(tmpData, assumed_addrlen)) {
            crcmatch = true;
            break;
        }
        array_shl(tmpData, tmpDataLen, 1);
    }

    if (crcmatch) {
        //wipe out old rx data
        memset(p_payload->data, 0, p_payload->length);

        uint8_t esb_len = tmpData[assumed_addrlen] >> 2; //extract length bits from the assumed Packet Control Field, which starts right behind the RF address

        //correct RF frame length if CRC match
        p_payload->length = assumed_addrlen + 1 + esb_len + 2; //final payload (5 byte address, ESB payload with dynamic length, higher 8 PCF bits, 2 byte CRC)

        //byte allign payload (throw away no_ack bit of PCF, keep the other 8 bits)
        array_shl(&tmpData[assumed_addrlen+1], esb_len, 1); //shift left all bytes behind the PCF field by one bit (we loose the lowest PCF bit, which is "no ack", and thus not of interest)

        /*
        //zero out rest of report
        memset(&tmpData[p_payload->length], 0, VALIDATION_SHIFT_BUF_SIZE - p_payload->length);
        */
        memcpy(&p_payload->data[2], tmpData, p_payload->length);
        p_payload->length += 2;
        p_payload->data[0] = p_payload->pipe; //encode rx pipe
        p_payload->data[1] = esb_len; //encode real ESB payload length

        return NRF_SUCCESS;
    } else {
        return NRF_ERROR_INVALID_DATA;
    }
}