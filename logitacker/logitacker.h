#ifndef LOGITACKER_H__
#define LOGITACKER_H__

#ifdef __cplusplus
extern "C" {
#endif


#include "stdint.h"
#include "nrf_cli.h"
#include "nrf_esb_illegalmod.h"
#include "logitacker_keyboard_map.h"

#define VERSION_STRING "v0.1.1-beta"

//#define PAIRING_REQ_MARKER_BYTE 0xee // byte used as device ID in pairing requests
#define ACTIVE_ENUM_INNER_LOOP_MAX 20 //how many CAPS presses / key releases get send
#define ACTIVE_ENUM_TX_DELAY_MS 2 //delay in ms between successful transmits in active enum mode (below 8 ms re-tx delay of real devices, to allow collecting ack payloads before device does)



#define LOGITACKER_DISCOVERY_STAY_ON_CHANNEL_AFTER_RX_MS 500 // time in ms to stop channel hopping in discovery mode, once a valid ESB frame is received
#define LOGITACKER_DISCOVERY_CHANNEL_HOP_INTERVAL_MS 30 // channel hop interval in discovery mode

#define LOGITACKER_PASSIVE_ENUM_STAY_ON_CHANNEL_AFTER_RX_MS 1300 // time in ms to stop channel hopping in passive mode, once a valid ESB frame is received
#define LOGITACKER_PASSIVE_ENUM_CHANNEL_HOP_INTERVAL_MS 30 // channel hop interval in passive enum mode

#define LOGITACKER_SNIFF_PAIR_STAY_ON_CHANNEL_AFTER_RX_MS 1300
#define LOGITACKER_SNIFF_PAIR_CHANNEL_HOP_INTERVAL_MS 10


typedef enum {
    LOGITACKER_MODE_DISCOVERY,   // radio in promiscuous mode, logs devices
    LOGITACKER_MODE_ACTIVE_ENUMERATION,   // radio in PTX mode, actively collecting dongle info
    LOGITACKER_MODE_PASSIVE_ENUMERATION,   // radio in SNIFF mode, collecting device frames to determin caps
    LOGITACKER_MODE_SNIFF_PAIRING,
    LOGITACKER_MODE_PAIR_DEVICE,
    LOGITACKER_MODE_INJECT,
    LOGITACKER_MODE_IDLE
} logitacker_mode_t;

char g_logitacker_cli_name[32];

uint32_t logitacker_init();

void logitacker_enter_mode_discovery();


void logitacker_enter_mode_passive_enum(uint8_t *rf_address);

void logitacker_enter_mode_active_enum(uint8_t *rf_address);

void logitacker_enter_mode_pair_device(uint8_t const *rf_address);

void logitacker_enter_mode_pair_sniff();

void logitacker_enter_mode_injection(uint8_t const *rf_address);

void logitacker_injection_start_execution(bool execute);

#ifdef __cplusplus
}
#endif



#endif