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

#define RADIO_MAX_CHANNEL_COUNT 100
#define RADIO_MIN_CHANNEL       5
#define RADIO_MAX_CHANNEL       77


typedef enum {
    RADIO_MODE_DISABLED,
    RADIO_MODE_PTX,          // Primary transmitter mode
    RADIO_MODE_PRX_ACTIVE,   // Primary receiver mode (with tx of ACK payload).
    RADIO_MODE_PRX_PASSIVE,  // Primary receiver mode (no ACKS == sniffing).
    RADIO_MODE_PROMISCOUS,   // RX in pseudo promiscuous mode
} radio_rf_mode_t;

typedef struct {
    uint8_t channel_list[100];
    uint32_t channel_list_length;
} radio_channel_set_t;

#define RADIO_DEFAULT_CHANNELS {                                                                    \
    .channel_list = { 5,8,11,14,17,20,23,26,29,32,35,38,41,44,47,50,53,56,59,62,65,68,71,74,77 },    \
    .channel_list_length = 25                                                                       \
}


uint32_t radioInit(nrf_esb_event_handler_t event_handler);
uint32_t radioSetMode(radio_rf_mode_t mode);
radio_rf_mode_t radioGetMode();

uint32_t radioSetAddressLength(uint8_t length);
uint32_t radioSetBaseAddress0(uint8_t const * p_addr);
uint32_t radioSetBaseAddress1(uint8_t const * p_addr);
uint32_t radioSetPrefixes(uint8_t const * p_prefixes, uint8_t num_pipes);
uint32_t radioEnablePipes(uint8_t enable_mask);
uint32_t radioUpdatePrefix(uint8_t pipe, uint8_t prefix);
uint32_t radioSetRfChannel(uint32_t channel);
uint32_t radioGetRfChannel(uint32_t * p_channel);
uint32_t radioNextRfChannel();
uint32_t radioSetRfChannelIndex(uint8_t channel_idx);
uint32_t radioGetRfChannelIndex(uint8_t *channel_index_result);



uint32_t radioPipeNumToRFAddress(uint8_t pipeNum, uint8_t *p_dst);
/*
uint32_t radioSetTxPower(nrf_esb_tx_power_t tx_output_power);
uint32_t nrf_esb_set_retransmit_delay(uint16_t delay);
uint32_t nrf_esb_set_retransmit_count(uint16_t count);
uint32_t nrf_esb_set_bitrate(nrf_esb_bitrate_t bitrate);
uint32_t nrf_esb_reuse_pid(uint8_t pipe);
*/



bool radioTransmit(nrf_esb_payload_t *p_tx_payload, bool blockTillResult);
bool radioTransmitCollectAck(nrf_esb_payload_t *p_tx_payload, bool blockTillResult, bool *ack_payload_received, nrf_esb_payload_t *ack_payload);
void radioTransmitDelayed(nrf_esb_payload_t *p_tx_payload, uint32_t delay_ms);

uint32_t validate_esb_payload(nrf_esb_payload_t * p_payload);

// helper
bool check_crc16(uint8_t * p_array, uint8_t len);
bool validate_esb_frame(uint8_t * p_array, uint8_t addrlen);
void array_shl(uint8_t *p_array, uint8_t len, uint8_t bits);

#endif