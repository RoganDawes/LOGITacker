#ifndef LOGITACKER_H__
#define LOGITACKER_H__

#ifdef __cplusplus
extern "C" {
#endif


#include <stdint.h>
#include "nrf_esb_illegalmod.h"
#include "logitacker_keyboard_map.h"


//#define PAIRING_REQ_MARKER_BYTE 0xee // byte used as device ID in pairing requests
#define ACTIVE_ENUM_INNER_LOOP_MAX 20 //how many CAPS presses / key releases get send
#define ACTIVE_ENUM_TX_DELAY_MS 2 //delay in ms between successful transmits in active enum mode (below 8 ms re-tx delay of real devices, to allow collecting ack payloads before device does)



#define LOGITACKER_DISCOVERY_STAY_ON_CHANNEL_AFTER_RX_MS 100 // time in ms to stop channel hopping in discovery mode, once a valid ESB frame is received
#define LOGITACKER_DISCOVERY_CHANNEL_HOP_INTERVAL_MS 30 // channel hop interval in discovery mode

#define LOGITACKER_PASSIVE_ENUM_STAY_ON_CHANNEL_AFTER_RX_MS 1300 // time in ms to stop channel hopping in passive mode, once a valid ESB frame is received
#define LOGITACKER_PASSIVE_ENUM_CHANNEL_HOP_INTERVAL_MS 30 // channel hop interval in passive enum mode

#define LOGITACKER_SNIFF_PAIR_STAY_ON_CHANNEL_AFTER_RX_MS 1300
#define LOGITACKER_SNIFF_PAIR_CHANNEL_HOP_INTERVAL_MS 10

typedef enum {
    LOGITACKER_DISCOVERY_ON_NEW_ADDRESS_DO_NOTHING,   // continues in discovery mode, when new address has been found
    LOGITACKER_DISCOVERY_ON_NEW_ADDRESS_SWITCH_PASSIVE_ENUMERATION,   // continues in passive enumeration mode when address found
    LOGITACKER_DISCOVERY_ON_NEW_ADDRESS_SWITCH_ACTIVE_ENUMERATION   // continues in active enumeration mode when address found
} logitacker_discovery_on_new_address_t;

typedef enum {
    LOGITACKER_PAIRING_SNIFF_ON_SUCCESS_CONTINUE,   // continues in discovery mode, when new address has been found
    LOGITACKER_PAIRING_SNIFF_ON_SUCCESS_SWITCH_PASSIVE_ENUMERATION,   // continues in passive enumeration mode when address found
    LOGITACKER_PAIRING_SNIFF_ON_SUCCESS_SWITCH_ACTIVE_ENUMERATION,   // continues in active enumeration mode when address found
    LOGITACKER_PAIRING_SNIFF_ON_SUCCESS_SWITCH_DISCOVERY
} logitacker_pairing_sniff_on_success_t;

typedef enum {
    LOGITACKER_MAINSTATE_DISCOVERY,   // radio in promiscuous mode, logs devices
    LOGITACKER_MAINSTATE_ACTIVE_ENUMERATION,   // radio in PTX mode, actively collecting dongle info
    LOGITACKER_MAINSTATE_PASSIVE_ENUMERATION,   // radio in SNIFF mode, collecting device frames to determin caps
    LOGITACKER_MAINSTATE_SNIFF_PAIRING,
    LOGITACKER_MAINSTATE_PAIR_DEVICE,
    LOGITACKER_MAINSTATE_INJECT,
    LOGITACKER_MAINSTATE_IDLE
} logitacker_mainstate_t;


uint32_t logitacker_init();

void logitacker_enter_mode_discovery();

void logitacker_discovery_mode_set_on_new_address_action(logitacker_discovery_on_new_address_t on_new_address_action);

void logitacker_enter_mode_passive_enum(uint8_t *rf_address);

void logitacker_enter_mode_active_enum(uint8_t *rf_address);

void logitacker_enter_mode_pair_device(uint8_t const *rf_address);

void logitacker_enter_mode_pairing_sniff();

void logitacker_enter_mode_injection(uint8_t const *rf_address);
void logitacker_injection_string(logitacker_keyboarmap_lang_t language_layout, char const * const str);


#ifdef __cplusplus
}
#endif



#endif