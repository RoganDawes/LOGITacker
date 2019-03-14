#ifndef STATE_H__
#define STATE_H__

#include <stdint.h>


/* A dummy structure to save in flash. */
typedef struct
{
    uint32_t boot_count;
    uint8_t  device_info_count;
} dongle_state_t;

void restoreStateFromFlash(dongle_state_t *state);

#endif
