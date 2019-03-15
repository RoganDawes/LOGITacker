#ifndef RADIO_H__
#define RADIO_H__

#include <stdint.h>
#include <stdbool.h>

#include "nrf_esb_illegalmod.h"


/*
Operation modes:

PTX:            Default operation, send frames, check for acks, collect ACK payloads
PRX_ACTIVE:     RX mode, transmit ACKS in response, transmit() usable to enqueue ACK payloads
PRX_PASSIVE:    Only RX, transmit() not allowed, no ACKs will be sent in response ro RX'ed frames
PROMISCOUS:     RX as much data as possible with invalid RF addresses, data could be checked to be ESB frame with validateESBFrame()
*/

typedef enum {
    PTX,          // Primary transmitter mode
    PRX_ACTIVE,   // Primary receiver mode (with tx of ACK payload).
    PRX_PASSIVE,  // Primary receiver mode (no ACKS == sniffing).
    PROMISCOUS,   // RX in pseudo promiscuous mode
} radio_rf_mode_t;

uint32_t radioSetMode(radio_rf_mode_t mode);
uint32_t radioSetChannel(radio_rf_mode_t mode);
uint32_t radioTransmit(uint8_t pipe, uint8_t payload_length, uint8_t *p_payload);

uint32_t validate_esb_payload(nrf_esb_payload_t * p_payload);

// helper
bool check_crc16(uint8_t * p_array, uint8_t len);
bool validate_esb_frame(uint8_t * p_array, uint8_t addrlen);
void array_shl(uint8_t *p_array, uint8_t len, uint8_t bits);

#endif