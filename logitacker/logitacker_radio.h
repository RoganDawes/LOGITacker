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

#define RADIO_DEFAULT_CHANNEL_HOP_INTERVAL_MS 30


typedef enum {
    RADIO_EVENT_NO_RX_TIMEOUT,
    RADIO_EVENT_CHANNEL_CHANGED,
    RADIO_EVENT_CHANNEL_CHANGED_FIRST_INDEX
} radio_evt_id_t;

typedef struct {
    radio_evt_id_t   evt_id;
    uint8_t          pipe;
    uint8_t          channel;
    uint8_t          channel_index;
} radio_evt_t;

typedef void (* radio_event_handler_t)(radio_evt_t const * p_event);


uint32_t logitacker_radio_init(nrf_esb_event_handler_t event_handler, radio_event_handler_t radio_event_handler);


uint32_t radio_start_channel_hopping(uint32_t interval, uint32_t start_delay_ms, bool disable_on_rx);
uint32_t radio_stop_channel_hopping();

uint32_t radio_enable_rx_timeout_event(uint32_t timeout_ms);
uint32_t radio_disable_rx_timeout_event();

uint32_t logitacker_radio_convert_promiscuous_frame_to_default_frame(nrf_esb_payload_t *p_out_payload, nrf_esb_payload_t const in_promiscuous_payload);

#endif