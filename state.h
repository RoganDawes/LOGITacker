#ifndef STATE_H__
#define STATE_H__

#include <stdint.h>
#include "radio.h"


// dongle state for persistent flash storage
typedef struct
{
    uint32_t boot_count;
    uint8_t  device_info_count;
} dongle_state_t;

// runtime dongle state
typedef struct
{
    //RF address
    //Channel
    radio_rf_mode_t  rf_mode; //RF mode 

} dongle_state_volatile_t;

void restoreStateFromFlash(dongle_state_t *state);

#endif
