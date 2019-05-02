#ifndef LOGITACKER_PROCESSOR_ACTIVE_ENUM_H
#define LOGITACKER_PROCESSOR_ACTIVE_ENUM_H

#include <stdbool.h>
#include "nrf.h"
#include "logitacker.h"
#include "logitacker_processor.h"
#include "app_timer.h"

typedef enum {
    ACTIVE_ENUM_PHASE_STARTED,   // try to reach dongle RF address for device
    ACTIVE_ENUM_PHASE_FINISHED,   // try to reach dongle RF address for device
    ACTIVE_ENUM_PHASE_PING_DONGLE,   // try to reach dongle RF address for device
    ACTIVE_ENUM_PHASE_DISCOVER_NEIGHBOURS,   // !! should be own substate !!
    ACTIVE_ENUM_PHASE_TEST_PLAIN_KEYSTROKE_INJECTION   // tests if plain keystrokes could be injected (press CAPS, listen for LED reports)
} active_enumeration_phase_t;




logitacker_processor_t * new_processor_active_enum(uint8_t *rf_address, app_timer_id_t timer_next_action);

#endif //LOGITACKER_PROCESSOR_ACTIVE_ENUM_H
