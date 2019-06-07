#ifndef LOGITACKER_OPTIONS_H
#define LOGITACKER_OPTIONS_H


#include "logitacker.h"
#include "logitacker_script_engine.h"

typedef struct {
    uint32_t boot_count;
} logitacker_options_stats_t;

typedef struct {
    bool pass_through_keyboard;
    bool pass_through_mouse;
    logitacker_discovery_on_new_address_t discovery_on_new_address_action; //not only state, persistent config
    logitacker_pairing_sniff_on_success_t pairing_sniff_on_success_action; //not only state, persistent config

    bool auto_store_plain_injectable;
    bool auto_store_sniffed_pairing_devices;

    logitacker_options_stats_t stats;

    logitacker_keyboard_map_lang_t injection_language;
    char default_script[LOGITACKER_SCRIPT_ENGINE_SCRIPT_NAME_MAX_LEN];
} logitacker_global_config_t;

const static logitacker_global_config_t LOGITACKER_OPTIONS_DEFAULTS = {
    .discovery_on_new_address_action = LOGITACKER_DISCOVERY_ON_NEW_ADDRESS_DO_NOTHING,
    .pairing_sniff_on_success_action = LOGITACKER_PAIRING_SNIFF_ON_SUCCESS_SWITCH_PASSIVE_ENUMERATION,
    .pass_through_mouse = false,
    .pass_through_keyboard = false,
    .auto_store_sniffed_pairing_devices = true,
    .auto_store_plain_injectable = true,
    .injection_language = LANGUAGE_LAYOUT_US, //default layout (if nothing set) is US
    .default_script = "",
    .stats = {
        .boot_count = 0,
    },
};


extern logitacker_global_config_t g_logitacker_global_config;

uint32_t logitacker_options_store_to_flash(void);
uint32_t logitacker_options_restore_from_flash(void);
void logitacker_options_print(nrf_cli_t const * p_cli);



#endif //LOGITACKER_OPTIONS_H
