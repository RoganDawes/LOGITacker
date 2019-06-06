#ifndef LOGITACKER_PROCESSOR_INJECT_H
#define LOGITACKER_PROCESSOR_INJECT_H

#include "logitacker_processor.h"
#include "app_timer.h"

logitacker_processor_t * new_processor_inject(uint8_t const *target_rf_address, app_timer_id_t timer_next_action);

void logitacker_processor_inject_start_execution(logitacker_processor_t *p_processor_inject, bool execute);



#endif //LOGITACKER_PROCESSOR_INJECT_H
