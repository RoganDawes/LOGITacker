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

typedef struct {
    uint8_t channel_list[100];
    uint32_t channel_list_length;
} radio_channel_set_t;

#define RADIO_DEFAULT_CHANNELS {                                                                    \
    .channel_list = { 5,8,11,14,17,20,23,26,29,32,35,38,41,44,47,50,53,56,59,62,65,68,71,74,77 },    \
    .channel_list_length = 25                                                                       \
}


uint32_t radioInit(nrf_esb_event_handler_t event_handler, radio_event_handler_t radio_event_handler);


uint32_t radio_start_channel_hopping(uint32_t interval, uint32_t start_delay_ms, bool disable_on_rx);
uint32_t radio_stop_channel_hopping();

uint32_t radio_enable_rx_timeout_event(uint32_t timeout_ms);
uint32_t radio_disable_rx_timeout_event();


#endif