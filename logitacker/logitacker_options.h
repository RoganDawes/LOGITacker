#ifndef LOGITACKER_OPTIONS_H
#define LOGITACKER_OPTIONS_H


#include "logitacker.h"

typedef struct {
    bool pass_through_keyboard;
    bool pass_through_mouse;
    logitacker_discovery_on_new_address_t discovery_on_new_address_action; //not only state, persistent config
} logitacker_global_config_t;


extern logitacker_global_config_t g_logitacker_global_config;


#endif //LOGITACKER_OPTIONS_H
