#ifndef LOGITACKER_OPTIONS_H
#define LOGITACKER_OPTIONS_H


#include "logitacker.h"

typedef struct {
    uint32_t boot_count;
} logitacker_options_stats_t;

typedef struct {
    bool pass_through_keyboard;
    bool pass_through_mouse;
    logitacker_discovery_on_new_address_t discovery_on_new_address_action; //not only state, persistent config

    logitacker_options_stats_t stats;
} logitacker_global_config_t;

const static logitacker_global_config_t LOGITACKER_OPTIONS_DEFAULTS = {
    .discovery_on_new_address_action = LOGITACKER_DISCOVERY_ON_NEW_ADDRESS_DO_NOTHING,
    .pass_through_mouse = false,
    .pass_through_keyboard = false,
    .stats = {
        .boot_count = 0,
    },
};


extern logitacker_global_config_t g_logitacker_global_config;

uint32_t logitacker_options_store_to_flash(void);
uint32_t logitacker_options_restore_from_flash(void);
void logitacker_options_print_stats();
void logitacker_options_print();



#endif //LOGITACKER_OPTIONS_H
