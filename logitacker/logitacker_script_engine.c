#include "ringbuf.h"
#include "logitacker_options.h"
#include "fds.h"
#include "logitacker_script_engine.h"

#define NRF_LOG_MODULE_NAME LOGITACKER_SCRIPT_ENGINE
#include "nrf_log.h"
#include "logitacker_flash.h"

NRF_LOG_MODULE_REGISTER();



#define INJECT_MAX_TASK_DATA_SIZE 256

RINGBUF_DEF(m_ringbuf, LOGITACKER_SCRIPT_ENGINE_RINGBUF_BYTES);



/* start of FDS script storage relevant data */




typedef enum script_engine_state {
    SCRIPT_ENGINE_STATE_IDLE,
    SCRIPT_ENGINE_STATE_FDS_WRITE_RUNNING,
    SCRIPT_ENGINE_STATE_FDS_WRITE_FAILED,
    SCRIPT_ENGINE_STATE_FDS_WRITE_SUCCEEDED,
    SCRIPT_ENGINE_STATE_FDS_READ_RUNNING,
    SCRIPT_ENGINE_STATE_FDS_READ_FAILED,
    SCRIPT_ENGINE_STATE_FDS_READ_SUCCEEDED,
    SCRIPT_ENGINE_STATE_FDS_DELETE_RUNNING,
    SCRIPT_ENGINE_STATE_FDS_DELETE_SUCCEEDED,
    SCRIPT_ENGINE_STATE_FDS_DELETE_FAILED,

} script_engine_state_t;


typedef enum {
    FDS_OP_WRITE_SCRIPT_SUB_STATE_WRITE_TASK_HEADER,
    FDS_OP_WRITE_SCRIPT_SUB_STATE_WRITE_TASK_DATA,
    FDS_OP_WRITE_SCRIPT_SUB_STATE_WRITE_SCRIPT_INFO,
} fds_op_write_script_sub_state_t;


static uint16_t m_current_script_task_record_key;
static stored_script_fds_info_t m_current_fds_op_fds_script_info;
static inject_task_t m_current_fds_op_task = {0};
static uint8_t m_current_fds_op_task_data[LOGITACKER_SCRIPT_ENGINE_MAX_TASK_DATA_MAX_SIZE];
static fds_record_t m_current_fds_op_record;

//static logitacker_processor_inject_ctx_t *m_current_fds_op_proc_inj_ctx;
static fds_op_write_script_sub_state_t m_current_fds_op_write_script_sub_state;
/* end of FDS script storage relevant data */


//logitacker_keyboard_map_lang_t m_lang = LANGUAGE_LAYOUT_US; //default layout (if nothing set) is US
script_engine_state_t m_script_engine_state = SCRIPT_ENGINE_STATE_IDLE;

void script_engine_transfer_state(script_engine_state_t new_state) {
    if (new_state == m_script_engine_state) return; //no state change

    switch (new_state) {
        case SCRIPT_ENGINE_STATE_IDLE:
            // stop all actions, send notification to callback
            m_script_engine_state = new_state;
            break;
        case SCRIPT_ENGINE_STATE_FDS_WRITE_FAILED:
            // ToDo: callback notify
            m_script_engine_state = SCRIPT_ENGINE_STATE_IDLE; // transfer to IDLE
            break;
        case SCRIPT_ENGINE_STATE_FDS_WRITE_SUCCEEDED:
            // ToDo: callback notify
            m_script_engine_state = SCRIPT_ENGINE_STATE_IDLE; // transfer to IDLE
            break;
        case SCRIPT_ENGINE_STATE_FDS_READ_FAILED:
            // ToDo: callback notify
            m_script_engine_state = SCRIPT_ENGINE_STATE_IDLE; // transfer to IDLE
            break;
        case SCRIPT_ENGINE_STATE_FDS_READ_SUCCEEDED:
            // ToDo: callback notify
            m_script_engine_state = SCRIPT_ENGINE_STATE_IDLE; // transfer to IDLE
            break;
        case SCRIPT_ENGINE_STATE_FDS_DELETE_RUNNING:
            // ToDo: callback notify
            m_script_engine_state = SCRIPT_ENGINE_STATE_IDLE; // transfer to IDLE
            break;
        case SCRIPT_ENGINE_STATE_FDS_DELETE_SUCCEEDED:
            // ToDo: callback notify
            m_script_engine_state = SCRIPT_ENGINE_STATE_IDLE; // transfer to IDLE
            break;
        default:
            m_script_engine_state = new_state;
            break;
    }
}

bool logitacker_script_engine_read_next_task(inject_task_t *p_task, uint8_t *p_task_data_buf) {
    VERIFY_PARAM_NOT_NULL(p_task);
    VERIFY_PARAM_NOT_NULL(p_task_data_buf);
    if (ringbuf_available_peek(&m_ringbuf) == LOGITACKER_SCRIPT_ENGINE_RINGBUF_BYTES) {
        NRF_LOG_DEBUG("No more elements to peek in ring buffer");
        goto label_reset_peek; // reinit buffer to set read/write pointers to beginning
    }

    uint32_t err_code;

    // get copy of task header
    size_t read_len_task = sizeof(inject_task_t);
    err_code = ringbuf_peek_data(&m_ringbuf, (uint8_t *) p_task, &read_len_task);
    if (read_len_task != sizeof(inject_task_t)) {
        NRF_LOG_ERROR("pop_task wasn't able to read full task header");
        goto label_reset_peek; // flush whole buffer and return
    }
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_ERROR("pop_task wasn't able to read task header, error: %d", err_code);
        goto label_reset_peek; // flush whole buffer and return
    }

    // fix data pointer of popped task, to point to provided data array
    p_task->p_data_u8 = p_task_data_buf;

    //read back copy of task data
    size_t read_len_task_data = p_task->data_len;
    if (p_task->data_len > LOGITACKER_SCRIPT_ENGINE_MAX_TASK_DATA_MAX_SIZE) {
        NRF_LOG_ERROR("pop_task: task data exceeds max size (%d of %d maximum allowed)", read_len_task, LOGITACKER_SCRIPT_ENGINE_MAX_TASK_DATA_MAX_SIZE);
        goto label_reset_peek;
    }
    err_code = ringbuf_peek_data(&m_ringbuf, (uint8_t *) p_task_data_buf, &read_len_task_data);
    if (read_len_task_data != p_task->data_len) {
        NRF_LOG_ERROR("pop_task wasn't able to read full task data");
        goto label_reset_peek; // flush whole buffer and return
    }
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_ERROR("pop_task wasn't able to read task data, error: %d", err_code);
        goto label_reset_peek; // flush whole buffer and return
    }


    return true;

    label_reset_peek:
    p_task->data_len = 0;
    //p_task->delay_ms = 0;
    p_task->type = INJECT_TASK_TYPE_UNUSED;
    ringbuf_peek_rewind(&m_ringbuf);
    return false;


}


/*
bool free_task(inject_task_t task) {
    uint32_t err_res;
    NRF_LOG_DEBUG("Free task, len %d", task.data_len);

    NRF_LOG_DEBUG("ringbuf peek_rd_idx before free...");
    NRF_LOG_HEXDUMP_DEBUG(&m_ringbuf.p_cb->peek_rd_idx, 4);

    err_res = ringbuf_free(&m_ringbuf, task.data_len);
    if (err_res != NRF_SUCCESS) {
        NRF_LOG_INFO("ERROR FREEING RINGBUF %d", err_res)
    } else {
        NRF_LOG_INFO("SUCCESS FREEING RINGBUF %d", err_res)
    }

    NRF_LOG_DEBUG("ringbuf peek_rd_idx after free...");
    NRF_LOG_HEXDUMP_DEBUG(&m_ringbuf.p_cb->peek_rd_idx, 4);

    return err_res == NRF_SUCCESS;
}
*/

/*
bool flush_tasks() {
    NRF_LOG_WARNING("flushing task buffer");
    ringbuf_reset(&m_ringbuf);
    return true;
}
*/

uint32_t logitacker_script_engine_rewind() {
    return ringbuf_peek_rewind(&m_ringbuf);
}

uint32_t logitacker_script_engine_flush_tasks() {
    ringbuf_reset(&m_ringbuf);
    return NRF_SUCCESS;
}

uint32_t logitacker_script_engine_remove_last_task() {
    if (ringbuf_available_fetch(&m_ringbuf) == 0) {
        // ringbuf is empty
        NRF_LOG_INFO("no task to remove left");
        return NRF_ERROR_NOT_FOUND;
    }

    inject_task_t task = {0};
    uint8_t task_data[LOGITACKER_SCRIPT_ENGINE_MAX_TASK_DATA_MAX_SIZE];
    ringbuf_peek_rewind(&m_ringbuf);


    uint32_t old_read_index = m_ringbuf.p_cb->peek_rd_idx;
    bool not_last_task = true;
    while (not_last_task) {
        uint32_t last_rd_idx = old_read_index;
        old_read_index = m_ringbuf.p_cb->peek_rd_idx;
        not_last_task = logitacker_script_engine_read_next_task(&task, task_data);
        if (!not_last_task) {
            old_read_index = last_rd_idx;
            not_last_task = false; //if no more data left to read, the task of this iteration was the last one
        }
    }

    //old_read_index points to buffer pos before last task, update write index
    char tmp_str_idx[50];
    sprintf(tmp_str_idx, "Set write index %ld to %ld", m_ringbuf.p_cb->wr_idx, old_read_index);
    NRF_LOG_INFO("%s", nrf_log_push(tmp_str_idx));

    m_ringbuf.p_cb->wr_idx = old_read_index;
    ringbuf_peek_rewind(&m_ringbuf);

    return NRF_SUCCESS;
}

uint32_t logitacker_script_engine_append_task(inject_task_t task) {
    // push header
    size_t len_hdr = sizeof(inject_task_t);
    size_t len_data = task.data_len;
    size_t len = len_hdr + len_data;

    if (len_data > LOGITACKER_SCRIPT_ENGINE_MAX_TASK_DATA_MAX_SIZE) {
        NRF_LOG_ERROR("task data exceeds max size (%d of %d maximum allowed)", len_data, LOGITACKER_SCRIPT_ENGINE_MAX_TASK_DATA_MAX_SIZE);
        return NRF_ERROR_INVALID_LENGTH;
    }

    if (len > ringbuf_available_fetch(&m_ringbuf)) {
        NRF_LOG_ERROR("Not enough memory in ring buffer");
        return NRF_ERROR_NO_MEM;
    }

    uint32_t err_res;

    err_res = ringbuf_push_data(&m_ringbuf, (uint8_t *) &task, &len_hdr);
    if (len_hdr != sizeof(inject_task_t)) {
        NRF_LOG_ERROR("push_task hasn't written full header");
        return NRF_ERROR_INTERNAL; //this means memory leak, as it can't be read back
    }
    if (err_res != NRF_SUCCESS) {
        NRF_LOG_ERROR("push_task error writing task header: %d", err_res);
        return NRF_ERROR_INTERNAL;
    }

    NRF_LOG_DEBUG("Pushed task header:");
    NRF_LOG_HEXDUMP_DEBUG(&task, len_hdr);

    if (len_data > 0) { //f.e. delay task has no body
        NRF_LOG_DEBUG("ringbuf wr_idx before putting data.");
        NRF_LOG_HEXDUMP_DEBUG(&m_ringbuf.p_cb->wr_idx, 4);

        ringbuf_push_data(&m_ringbuf, task.p_data_u8, &len_data);
        if (len_data != task.data_len) {
            NRF_LOG_ERROR("push_task hasn't written full data");
            return NRF_ERROR_INTERNAL; //this means memory leak, as it can't be read back
        }

        NRF_LOG_DEBUG("ringbuf wr_idx after putting data.");
        NRF_LOG_HEXDUMP_DEBUG(&m_ringbuf.p_cb->wr_idx, 4);

    }

    return NRF_SUCCESS;

}

uint32_t logitacker_script_engine_append_task_press_combo(char * str_combo) {
    inject_task_t tmp_task = {0};
    tmp_task.data_len = strlen(str_combo) + 1; //include terminating 0x00
    tmp_task.p_data_c = str_combo;
    tmp_task.type = INJECT_TASK_TYPE_PRESS_KEYS;
//    tmp_task.lang = m_lang;
    //return push_task(tmp_task);

    return logitacker_script_engine_append_task(tmp_task);

}

uint32_t logitacker_script_engine_append_task_type_string(char * str) {
    inject_task_t tmp_task = {0};
    tmp_task.data_len = strlen(str)+1; //include terminating 0x00
    tmp_task.p_data_c = str;
    tmp_task.type = INJECT_TASK_TYPE_TYPE_STRING;
//    tmp_task.lang = m_lang;

    //return push_task(tmp_task);
    return logitacker_script_engine_append_task(tmp_task);
}

uint32_t logitacker_script_engine_append_task_type_altstring(char * str) {
    inject_task_t tmp_task = {0};
    tmp_task.data_len = strlen(str)+1; //include terminating 0x00
    tmp_task.p_data_c = str;
    tmp_task.type = INJECT_TASK_TYPE_TYPE_ALTSTRING;
//    tmp_task.lang = m_lang;

    //return push_task(tmp_task);
    return logitacker_script_engine_append_task(tmp_task);
}

uint32_t logitacker_script_engine_append_task_delay(uint32_t delay_ms) {
    inject_task_t tmp_task = {0};
    //tmp_task.delay_ms = delay_ms;
    tmp_task.data_len = 4; //include terminating 0x00
    tmp_task.p_data_u8 = (void*) &delay_ms;
    tmp_task.type = INJECT_TASK_TYPE_DELAY;

    //return push_task(tmp_task);
    return logitacker_script_engine_append_task(tmp_task);
}

void logitacker_script_engine_print_current_tasks(nrf_cli_t const * p_cli) {
    // ToDo: this uses peeking into ring buffer and thus it has to be prevented, that peeking is already running (f.e.
    // tasks are fetched while injection is performed)

    inject_task_t task = {0};
    uint8_t task_data[LOGITACKER_SCRIPT_ENGINE_MAX_TASK_DATA_MAX_SIZE];
    ringbuf_peek_rewind(&m_ringbuf);
    uint32_t task_num = 1;

    nrf_cli_fprintf(p_cli, NRF_CLI_VT100_COLOR_GREEN, "script start\r\n", task_num);
    while (logitacker_script_engine_read_next_task(&task, task_data)) {
        nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "%04d: ", task_num);
        switch (task.type) {
            case INJECT_TASK_TYPE_DELAY:
                nrf_cli_fprintf(p_cli, NRF_CLI_VT100_COLOR_YELLOW, "delay ");
                nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "%d\r\n", *task.p_data_u32);
                break;
            case INJECT_TASK_TYPE_TYPE_STRING:
                nrf_cli_fprintf(p_cli, NRF_CLI_VT100_COLOR_YELLOW, "string ");
                nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "%s\r\n", task_data);
                break;
            case INJECT_TASK_TYPE_TYPE_ALTSTRING:
                nrf_cli_fprintf(p_cli, NRF_CLI_VT100_COLOR_YELLOW, "altstring ");
                nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "%s\r\n", task_data);
                break;
            case INJECT_TASK_TYPE_PRESS_KEYS:
                nrf_cli_fprintf(p_cli, NRF_CLI_VT100_COLOR_YELLOW, "press ");
                nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "%s\r\n", task_data);
                break;
            default:
                nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "%04d: unknown task type %d\r\n", task_num, task.type);
                break;
        }
        task_num++;
    }
    nrf_cli_fprintf(p_cli, NRF_CLI_VT100_COLOR_GREEN, "script end\r\n", task_num);
    ringbuf_peek_rewind(&m_ringbuf);
}

































/*
 * SCRIPT FLASH STORAGE
 * - a script consists of several tasks and the respective task data
 * - each stored script is describe by a stored_script_fds_info_ struct (script_name, FDS_FILE_ID of tasks / tasks data, FDS_RECORD_ID of tasks/tasks data)
 *
 *
 */


//ToDo: For error cases during script storage, already written records have to be deleted again
void logitacker_script_engine_fds_event_handler(fds_evt_t const *p_evt) {
    if (m_script_engine_state == SCRIPT_ENGINE_STATE_IDLE) {
        NRF_LOG_DEBUG("FDS event handler for scripting: IDLE ... ignoring event");
        return;
    }


    if (m_script_engine_state == SCRIPT_ENGINE_STATE_FDS_WRITE_RUNNING && p_evt->id == FDS_EVT_WRITE) {
        // increment record_key to account for storage order problem
        // (https://devzone.nordicsemi.com/f/nordic-q-a/52297/fds-read-order-fds_record_find-doesn-t-reflect-write-order-fds_record_write-for-multiple-records-with-same-record_key-and-file_id-if-virtual-page-boundaries-are-crossed)
        m_current_script_task_record_key++;

        NRF_LOG_DEBUG("FDS_EVENT_WRITE");
        if (p_evt->result == NRF_SUCCESS)
        {
            switch (m_current_fds_op_write_script_sub_state) {
                case FDS_OP_WRITE_SCRIPT_SUB_STATE_WRITE_TASK_HEADER:
                {
                    NRF_LOG_DEBUG("Task header written successfully, writing next task data ...")
                    // start  writing task data

                    m_current_fds_op_write_script_sub_state = FDS_OP_WRITE_SCRIPT_SUB_STATE_WRITE_TASK_DATA; //indicate that next write operation contained task data
                    m_current_fds_op_record.file_id = m_current_fds_op_fds_script_info.script_tasks_file_id;
                    m_current_fds_op_record.key = m_current_script_task_record_key;
                    m_current_fds_op_record.data.length_words = (m_current_fds_op_task.data_len + 3) / 4; // this will write one word, even if task data length is zero (tasks of type delay)
                    m_current_fds_op_record.data.p_data = &m_current_fds_op_task_data;
                    if (fds_record_write(NULL, &m_current_fds_op_record) != NRF_SUCCESS) {
                        NRF_LOG_ERROR("failed to write task for script storage")
                        script_engine_transfer_state(SCRIPT_ENGINE_STATE_FDS_WRITE_FAILED);
                        return;
                    }
                    break;
                }
                case FDS_OP_WRITE_SCRIPT_SUB_STATE_WRITE_TASK_DATA:
                {
                    NRF_LOG_DEBUG("Task data written successfully, writing next task header ...");
                    // if here, next step would be to write the next task header, thus we try to fetch the next task
                    // load first task to tmp vars
                    if (!logitacker_script_engine_read_next_task(&m_current_fds_op_task, m_current_fds_op_task_data)) {
                        // script content written successfully, go on with script info

                        NRF_LOG_ERROR("No more tasks left to store, writing script info ...");
                        m_current_fds_op_write_script_sub_state = FDS_OP_WRITE_SCRIPT_SUB_STATE_WRITE_SCRIPT_INFO;
                        m_current_fds_op_record.file_id = LOGITACKER_FLASH_FILE_ID_STORED_SCRIPTS_INFO;
                        m_current_fds_op_record.key = LOGITACKER_FLASH_RECORD_KEY_STORED_SCRIPTS_INFO;
                        m_current_fds_op_record.data.length_words = (sizeof(stored_script_fds_info_t) + 3) / 4; // this will write one word, even if task data length is zero
                        m_current_fds_op_record.data.p_data = &m_current_fds_op_fds_script_info;
                        if (fds_record_write(NULL, &m_current_fds_op_record) != NRF_SUCCESS) {
                            NRF_LOG_ERROR("failed to write script info for script storage")
                            script_engine_transfer_state(SCRIPT_ENGINE_STATE_FDS_WRITE_FAILED);
                            return;
                        }
                        return;
                    }

                    // initiate storage of next task header
                    m_current_fds_op_write_script_sub_state = FDS_OP_WRITE_SCRIPT_SUB_STATE_WRITE_TASK_HEADER;
                    m_current_fds_op_record.file_id = m_current_fds_op_fds_script_info.script_tasks_file_id;
                    m_current_fds_op_record.key = m_current_script_task_record_key;
                    m_current_fds_op_record.data.length_words = (sizeof(inject_task_t) + 3) / 4;
                    m_current_fds_op_record.data.p_data = &m_current_fds_op_task;
                    if (fds_record_write(NULL, &m_current_fds_op_record) != NRF_SUCCESS) {
                        NRF_LOG_ERROR("failed to write task header for script storage")
                        script_engine_transfer_state(SCRIPT_ENGINE_STATE_FDS_WRITE_FAILED);
                        return;
                    }
                    break;
                }
                case FDS_OP_WRITE_SCRIPT_SUB_STATE_WRITE_SCRIPT_INFO:
                {
                    NRF_LOG_INFO("Script successfully written to flash ...");
                    script_engine_transfer_state(SCRIPT_ENGINE_STATE_FDS_WRITE_SUCCEEDED);
                    return;
                }
            }

        } else {
            // script storage failed
            NRF_LOG_ERROR("failed to store script to flash, FDS error result: %d", p_evt->result);
            script_engine_transfer_state(SCRIPT_ENGINE_STATE_FDS_WRITE_FAILED);
            return;

        }
    }
}

uint32_t find_stored_script_info_by_name(const char *script_name, stored_script_fds_info_t *p_stored_script_info) {
    VERIFY_PARAM_NOT_NULL(script_name);
    VERIFY_PARAM_NOT_NULL(p_stored_script_info);

    fds_find_token_t ftoken;
    memset(&ftoken, 0x00, sizeof(fds_find_token_t));
    fds_flash_record_t flash_record;
    fds_record_desc_t fds_record_desc;

    while(fds_record_find(LOGITACKER_FLASH_FILE_ID_STORED_SCRIPTS_INFO, LOGITACKER_FLASH_RECORD_KEY_STORED_SCRIPTS_INFO, &fds_record_desc, &ftoken) == NRF_SUCCESS) {
        if (fds_record_open(&fds_record_desc, &flash_record) != NRF_SUCCESS) {
            NRF_LOG_WARNING("Failed to open record");
            continue; // go on with next
        }

        stored_script_fds_info_t const * p_stored_tasks_fds_info_tmp = flash_record.p_data;
        // compare script_name
        if (strcmp(p_stored_tasks_fds_info_tmp->script_name, script_name) == 0) {
            // string found
            memcpy(p_stored_script_info, p_stored_tasks_fds_info_tmp, sizeof(stored_script_fds_info_t));
            fds_record_close(&fds_record_desc);
            return NRF_SUCCESS;
        }

        if (fds_record_close(&fds_record_desc) != NRF_SUCCESS) {
            NRF_LOG_WARNING("Failed to close record");
        }
    }

    return NRF_ERROR_NOT_FOUND;
}




bool logitacker_script_engine_store_current_script_to_flash(const char *script_name) {
    if (m_script_engine_state != SCRIPT_ENGINE_STATE_IDLE) {
        NRF_LOG_ERROR("can't store script to flash, script engine not in IDLE state: %d", m_script_engine_state);
        return false;
    }

    // indicate that a FDS write operation is ongoing
    script_engine_transfer_state(SCRIPT_ENGINE_STATE_FDS_WRITE_RUNNING);


    // an use first unused key)
    // 3) create stored_script_fds_info for next usable record_key

    // check if script name already exists (overwrite of existing scripts isn't allowed)
    stored_script_fds_info_t tmp_stfi;
    if (find_stored_script_info_by_name(script_name, &tmp_stfi) == NRF_SUCCESS) {
        NRF_LOG_ERROR("store_current_task_to_flash: script name already used");
        script_engine_transfer_state(SCRIPT_ENGINE_STATE_IDLE);
        return false;
    }

    // determine next available FDS file ID for script storage
    fds_record_desc_t fds_record_desc;
    fds_find_token_t ftoken;
    memset(&ftoken, 0x00, sizeof(fds_find_token_t));
    uint16_t last_used_id = LOGITACKER_FLASH_FIRST_FILE_ID_STORED_SCRIPT_TASKS-1;
    for (uint16_t test_id = LOGITACKER_FLASH_FIRST_FILE_ID_STORED_SCRIPT_TASKS; (fds_record_find(test_id, LOGITACKER_FLASH_RECORD_KEY_FIRST_STORED_SCRIPT_TASKS, &fds_record_desc, &ftoken) == NRF_SUCCESS) && (test_id < LOGITACKER_FLASH_MAXIMUM_FILE_ID_STORED_SCRIPT_TASKS); test_id++) {
        last_used_id = test_id; //update last used ID, as test ID is in use
    }
    uint16_t next_usable_id = last_used_id+1;
    if (next_usable_id > LOGITACKER_FLASH_MAXIMUM_FILE_ID_STORED_SCRIPT_TASKS) {
        // limit reached
        NRF_LOG_ERROR("limit of maximum storable scripts reached")
        script_engine_transfer_state(SCRIPT_ENGINE_STATE_IDLE);
        return false;
    }


    // fill stored_script_fds_info_t data struct (don't store yet)
    m_current_fds_op_fds_script_info.script_tasks_file_id = next_usable_id;
    m_current_script_task_record_key = LOGITACKER_FLASH_RECORD_KEY_FIRST_STORED_SCRIPT_TASKS;
    size_t slen = strlen(script_name);
    slen = slen > LOGITACKER_SCRIPT_ENGINE_SCRIPT_NAME_MAX_LEN ? LOGITACKER_SCRIPT_ENGINE_SCRIPT_NAME_MAX_LEN : slen;
    memset(m_current_fds_op_fds_script_info.script_name, 0, LOGITACKER_SCRIPT_ENGINE_SCRIPT_NAME_MAX_LEN);
    memcpy(m_current_fds_op_fds_script_info.script_name, script_name, slen);


    //ringbuf_peek_rewind(&m_ringbuf); // rewind ringbuf
    logitacker_script_engine_rewind();


    // load first task to tmp vars
    if (!logitacker_script_engine_read_next_task(&m_current_fds_op_task, m_current_fds_op_task_data)) {
        NRF_LOG_ERROR("Failed to load first task for script storage, can't store empty script")
        script_engine_transfer_state(SCRIPT_ENGINE_STATE_IDLE);
        return false;
    }

    // start first write operation
    m_current_fds_op_write_script_sub_state = FDS_OP_WRITE_SCRIPT_SUB_STATE_WRITE_TASK_HEADER;
    m_current_fds_op_record.file_id = m_current_fds_op_fds_script_info.script_tasks_file_id;
    m_current_fds_op_record.key = m_current_script_task_record_key;
    m_current_fds_op_record.data.length_words = (sizeof(inject_task_t) + 3) / 4;
    m_current_fds_op_record.data.p_data = &m_current_fds_op_task;
    if (fds_record_write(NULL, &m_current_fds_op_record) != NRF_SUCCESS) {
        NRF_LOG_ERROR("failed to write first task for script storage")
        script_engine_transfer_state(SCRIPT_ENGINE_STATE_FDS_WRITE_FAILED);
        return false;
    }

    return true;
}

/*
 * The assumption that `fds_record_find` returns records for NOT CHANGING file_id and record_key in the same
 * order they have been written IS WRONG.
 *
 * The documentation sub-section "storage format" of FDS states the following:
 *
 * ```
 * Record layout
 *
 * Records consist of a header (the record metadata) and the actual content. They are stored contiguously in flash in
 * the order they are written.
 * ```
 *
 * It turns out that this rule doesn't apply if virtual page boundaries are crossed.
 * The behavior seems to be the following:
 *
 * If a record should be written but a virtual page couldn't take it (because the virtual page has too few space left)
 * it is written to the next virtual page with enough remaining space. If a successive operation tries to write a
 * smaller record, which still fits into the virtual page which couldn't be used for the first write (the last record
 * was too large, but this one * fits) this record is written to the virtual page preceding the one used for the first
 * fds_record_write call. From fsstorage perspective, this means that these two records aren't layed out in write order,
 * because they are swapped.
 *
 * Now the `fds_record_find` method follows a simple logic, if multiple records with same file_id and record_id should
 * be returned. It steps through each virtual page (in order) and for each virtual page it steps through all records and
 * returns the next record (according to the find_token), which matches the filter criteria (file_id and record_key).
 * For the example above, the iteration would hit the record which was written in the second call to `fds_record_write`
 * FIRST, because it was stored in the virtual page preceding the virtual page of the first call to fds_record_write.
 *
 * This means if order of data is critical, different record_key (e.g. incrementing) or file_ids have to be used.
 * Records with same file_id and record_key have to be seen as "set" where read order (fds_record_find) isn't guaranteed
 * to reflect write order (successive calls to fds_record_write).
 *
 * Note 1: Increasing virtual page size for FDS could mitigate the problem, but it still occurs if page boundaries are
 * crossed for records with same file_id and record_key.
 *
 * Note 2: Not waiting for write operations to finish isn't part of the problem described here, as it was assured that
 * no successive `fds_record_write` calls have been enqueued, before the respective FDS_EVT_WRITE occurred.
 *
 */

bool logitacker_script_engine_load_script_from_flash(const char *script_name) {
    if (m_script_engine_state != SCRIPT_ENGINE_STATE_IDLE) {
        NRF_LOG_ERROR("can't load script from flash, script engine not in IDLE state: %d", m_script_engine_state);
        return false;
    }

    // indicate that a FDS write operation is ongoing
    script_engine_transfer_state(SCRIPT_ENGINE_STATE_FDS_READ_RUNNING);


    // an use first unused key)
    // 3) create stored_script_fds_info for next usable record_key

    // check if script name already exists (overwrite of existing scripts isn't allowed)
    if (find_stored_script_info_by_name(script_name, &m_current_fds_op_fds_script_info) != NRF_SUCCESS) {
        NRF_LOG_ERROR("logitacker_script_engine_load_script_from_flash: script name not found");
        script_engine_transfer_state(SCRIPT_ENGINE_STATE_FDS_READ_FAILED);
        return false;
    }

    NRF_LOG_INFO("script %s file_id %04x", script_name, m_current_fds_op_fds_script_info.script_tasks_file_id);

    // flush current tasks
    //flush_tasks();
    logitacker_script_engine_flush_tasks();

    // start iterating over tasks
    fds_find_token_t ftoken;
    memset(&ftoken, 0x00, sizeof(fds_find_token_t));
    fds_record_desc_t fds_record_desc;
    fds_flash_record_t fds_flash_record;
    m_current_script_task_record_key = LOGITACKER_FLASH_RECORD_KEY_FIRST_STORED_SCRIPT_TASKS;
    while (true) {
        uint32_t err = fds_record_find(m_current_fds_op_fds_script_info.script_tasks_file_id, m_current_script_task_record_key, &fds_record_desc, &ftoken);
        if (err != NRF_SUCCESS) {
            // should only fail with "RECORD_NOT_FOUND"
            NRF_LOG_DEBUG("error loading file_id %04x record_key %04x: %08x", err);
            break;
        }

        //NRF_LOG_INFO("HIT for record ID %04x", m_current_script_task_record_key);
        memset(&ftoken, 0x00, sizeof(fds_find_token_t)); // reset find token, next lookup is for different record_key
        m_current_script_task_record_key++;

        // open task header
        if (fds_record_open(&fds_record_desc, &fds_flash_record) != NRF_SUCCESS) {
            NRF_LOG_ERROR("logitacker_script_engine_load_script_from_flash: failed to read task header");
            script_engine_transfer_state(SCRIPT_ENGINE_STATE_FDS_READ_FAILED);
            return false;
        }

        memcpy(&m_current_fds_op_task, fds_flash_record.p_data, sizeof(inject_task_t));

        if (fds_record_close(&fds_record_desc) != NRF_SUCCESS) {
            NRF_LOG_ERROR("logitacker_script_engine_load_script_from_flash: failed to close task header record");
            script_engine_transfer_state(SCRIPT_ENGINE_STATE_FDS_READ_FAILED);
            return false;
        }

        NRF_LOG_DEBUG("task type: %d", m_current_fds_op_task.type);
        // try to load next record, which should be the task data
        if (fds_record_find(m_current_fds_op_fds_script_info.script_tasks_file_id, m_current_script_task_record_key, &fds_record_desc, &ftoken) != NRF_SUCCESS) {
            NRF_LOG_ERROR("logitacker_script_engine_load_script_from_flash: failed to read task data");
            script_engine_transfer_state(SCRIPT_ENGINE_STATE_FDS_READ_FAILED);
            return false;
        }
        m_current_script_task_record_key++;

        // open task data
        memset(&ftoken, 0x00, sizeof(fds_find_token_t)); // reset find token, next lookup is for different record_key
        if (fds_record_open(&fds_record_desc, &fds_flash_record) != NRF_SUCCESS) {
            NRF_LOG_ERROR("logitacker_script_engine_load_script_from_flash: failed to open task data");
            script_engine_transfer_state(SCRIPT_ENGINE_STATE_FDS_READ_FAILED);
            return false;
        }

        if (m_current_fds_op_task.data_len > 0) memcpy(&m_current_fds_op_task_data, fds_flash_record.p_data, m_current_fds_op_task.data_len);

        if (fds_record_close(&fds_record_desc) != NRF_SUCCESS) {
            NRF_LOG_ERROR("logitacker_script_engine_load_script_from_flash: failed to close task data record");
            script_engine_transfer_state(SCRIPT_ENGINE_STATE_FDS_READ_FAILED);
            return false;
        }

        m_current_fds_op_task.p_data_u8 = m_current_fds_op_task_data; // fix pointer of loaded task

        // ad task to current tasks
        //push_task(m_current_fds_op_task);
        logitacker_script_engine_append_task(m_current_fds_op_task);

    }

    // all tasks read if here
    script_engine_transfer_state(SCRIPT_ENGINE_STATE_FDS_READ_SUCCEEDED);
    return true;
}


void logitacker_script_engine_list_scripts_from_flash(nrf_cli_t const *p_cli) {

    fds_find_token_t ftoken;
    memset(&ftoken, 0x00, sizeof(fds_find_token_t));
    fds_flash_record_t flash_record;
    fds_record_desc_t fds_record_desc;

    uint32_t task_num = 1;
    while(fds_record_find(LOGITACKER_FLASH_FILE_ID_STORED_SCRIPTS_INFO, LOGITACKER_FLASH_RECORD_KEY_STORED_SCRIPTS_INFO, &fds_record_desc, &ftoken) == NRF_SUCCESS) {
        if (fds_record_open(&fds_record_desc, &flash_record) != NRF_SUCCESS) {
            NRF_LOG_WARNING("failed to open record");
            continue; // go on with next
        }

        stored_script_fds_info_t const * p_stored_tasks_fds_info_tmp = flash_record.p_data;


        nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "%04d script '%s'\r\n", task_num, p_stored_tasks_fds_info_tmp->script_name);

        if (fds_record_close(&fds_record_desc) != NRF_SUCCESS) {
            NRF_LOG_WARNING("failed to close record");
        }

        task_num++;
    }


}

void delete_taskdata_from_flash(uint16_t file_id) {
    fds_find_token_t ftoken;
    memset(&ftoken, 0x00, sizeof(fds_find_token_t));
    fds_record_desc_t fds_record_desc;

    while(fds_record_find_in_file(file_id, &fds_record_desc, &ftoken) == NRF_SUCCESS) {
        if (fds_record_delete(&fds_record_desc) != NRF_SUCCESS) {
            NRF_LOG_WARNING("failed to delete record");
            continue; // go on with next
        }
    }

}


bool logitacker_script_engine_delete_script_from_flash(const char *script_name) {
    //ToDo: avoid iterating over script info twice
    //ToDo: implement delete based on DFS_EVENT (same as store, continue on DFS_SUCCESS delete event)

    if (m_script_engine_state != SCRIPT_ENGINE_STATE_IDLE) {
        NRF_LOG_ERROR("can't delete script from flash, script engine not in IDLE state: %d", m_script_engine_state);
        return false;
    }

    // indicate that a FDS write operation is ongoing
    script_engine_transfer_state(SCRIPT_ENGINE_STATE_FDS_DELETE_RUNNING);

    // check if script name already exists
    if (find_stored_script_info_by_name(script_name, &m_current_fds_op_fds_script_info) != NRF_SUCCESS) {
        NRF_LOG_ERROR("logitacker_script_engine_delete_script_from_flash: script name not found");
        script_engine_transfer_state(SCRIPT_ENGINE_STATE_FDS_DELETE_FAILED);
        return false;
    }

    delete_taskdata_from_flash(m_current_fds_op_fds_script_info.script_tasks_file_id);


    // delete script info
    fds_find_token_t ftoken;
    memset(&ftoken, 0x00, sizeof(fds_find_token_t));
    fds_flash_record_t flash_record;
    fds_record_desc_t fds_record_desc;

    while(fds_record_find(LOGITACKER_FLASH_FILE_ID_STORED_SCRIPTS_INFO, LOGITACKER_FLASH_RECORD_KEY_STORED_SCRIPTS_INFO, &fds_record_desc, &ftoken) == NRF_SUCCESS) {
        if (fds_record_open(&fds_record_desc, &flash_record) != NRF_SUCCESS) {
            NRF_LOG_WARNING("Failed to open record");
            continue; // go on with next
        }

        stored_script_fds_info_t const * p_stored_tasks_fds_info_tmp = flash_record.p_data;
        // compare script_name
        if (strcmp(p_stored_tasks_fds_info_tmp->script_name, script_name) == 0) {
            // string found

            fds_record_close(&fds_record_desc);
            fds_record_delete(&fds_record_desc);
            break;
        }

        if (fds_record_close(&fds_record_desc) != NRF_SUCCESS) {
            NRF_LOG_WARNING("Failed to close record");
        }
    }


    // call garbage collector for FDS
    fds_gc();
    return true;
}

void logitacker_script_engine_set_language_layout(logitacker_keyboard_map_lang_t lang) {
    g_logitacker_global_config.injection_language = lang;
}

logitacker_keyboard_map_lang_t logitacker_script_engine_get_language_layout() {
    return g_logitacker_global_config.injection_language;
}