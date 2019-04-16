#ifndef LOGITACKER_H__
#define LOGITACKER_H__

#include <stdint.h>
#include "nrf_esb_illegalmod.h"


#define LOGITACKER_DISCOVERY_STAY_ON_CHANNEL_AFTER_RX_MS 500 // time in ms to stop channel hopping in discovery mode, once a valid ESB frame is received
#define LOGITACKER_DISCOVERY_CHANNEL_HOP_INTERVAL_MS 30 // channel hop interval in discovery mode

uint32_t logitacker_init();

#endif