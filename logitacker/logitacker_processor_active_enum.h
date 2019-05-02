#ifndef LOGITACKER_PROCESSOR_ACTIVE_ENUM_H
#define LOGITACKER_PROCESSOR_ACTIVE_ENUM_H

#include "logitacker_processor.h"
#include "app_timer.h"

logitacker_processor_t * new_processor_active_enum(uint8_t *rf_address, app_timer_id_t timer_next_action);

#endif //LOGITACKER_PROCESSOR_ACTIVE_ENUM_H
