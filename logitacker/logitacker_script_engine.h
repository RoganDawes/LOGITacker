#include "nrf_cli.h"
#include "stdbool.h"
#include "stdint.h"
#include "logitacker_keyboard_map.h"

#ifndef LOGITACKER_SCRIPT_ENGINE_H
#define LOGITACKER_SCRIPT_ENGINE_H


#define INJECT_MAX_TASK_DATA_SIZE 256 // maximum size of a single script task
#define INJECT_RINGBUF_BYTES (1<<15) // overall size of ringbuffer for storable script tasks (has to be power of 2)

typedef enum inject_task_type {
    INJECT_TASK_TYPE_TYPE_STRING, //type out UTF-8 String
    INJECT_TASK_TYPE_PRESS_KEYS,  // parse UTF-8 string for valid key combos and generate reports pressing those keys
    INJECT_TASK_TYPE_DELAY,
} inject_task_type_t;

typedef struct inject_task {
    inject_task_type_t type;
    size_t data_len;
    logitacker_keyboard_map_lang_t lang;
    uint32_t delay_ms;

    union {
        uint8_t* p_data_u8;
        char* p_data_c;
    };
} inject_task_t;

bool logitacker_script_engine_store_current_script_to_flash(const char *script_name);
bool logitacker_script_engine_load_script_from_flash(const char *script_name);
void logitacker_script_engine_list_scripts_from_flash(nrf_cli_t const *p_cli);
bool logitacker_script_engine_delete_script_from_flash(const char *script_name);


bool logitacker_script_engine_read_next_task(inject_task_t *p_task, uint8_t *p_task_data_buf);
uint32_t logitacker_script_engine_rewind();
uint32_t logitacker_script_engine_flush_tasks();
uint32_t logitacker_script_engine_remove_last_task();
uint32_t logitacker_script_engine_append_task(inject_task_t task);
uint32_t logitacker_script_engine_append_task_press_combo(char * str_combo);
uint32_t logitacker_script_engine_append_task_type_string(char * str);
uint32_t logitacker_script_engine_append_task_delay(uint32_t delay_ms);

void logitacker_script_engine_print_current_tasks(nrf_cli_t const * p_cli);

void logitacker_script_engine_fds_event_handler(fds_evt_t const *p_evt);

#endif //LOGITACKER_SCRIPT_ENGINE_H
