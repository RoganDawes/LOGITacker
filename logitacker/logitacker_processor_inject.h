#ifndef LOGITACKER_PROCESSOR_INJECT_H
#define LOGITACKER_PROCESSOR_INJECT_H

#include "logitacker_processor.h"
#include "app_timer.h"
#include "logitacker_pairing_parser.h"
#include "logitacker_keyboard_map.h"

logitacker_processor_t * new_processor_inject(uint8_t const *target_rf_address, app_timer_id_t timer_next_action);

void logitacker_processor_inject_string(logitacker_processor_t * p_processor_inject, logitacker_keyboard_map_lang_t lang, char * str);
void logitacker_processor_inject_delay(logitacker_processor_t * p_processor_inject, uint32_t delay_ms);
void logitacker_processor_inject_press(logitacker_processor_t * p_processor_inject, logitacker_keyboard_map_lang_t lang, char * combo_str);

void logitacker_processor_inject_start_execution(logitacker_processor_t *p_processor_inject, bool execute);
void logitacker_processor_inject_clear_tasks(logitacker_processor_t *p_processor_inject);
void logitacker_processor_inject_list_tasks(logitacker_processor_t *p_processor_inject, nrf_cli_t const * p_cli);


#endif //LOGITACKER_PROCESSOR_INJECT_H
