#include "logitacker_processor_inject.h"

#include "nrf.h"
#include "logitacker.h"
#include "logitacker_processor.h"
#include "helper.h"
#include "string.h"
#include "logitacker_devices.h"
#include "logitacker_tx_payload_provider.h"
#include "unifying.h"
#include "nrf_ringbuf.h"

#define NRF_LOG_MODULE_NAME LOGITACKER_PROCESSOR_INJECT
#include "nrf_log.h"
#include "logitacker_tx_pay_provider_string_to_keys.h"
#include "logitacker_tx_payload_provider_press_to_keys.h"

NRF_LOG_MODULE_REGISTER();

#define INJECT_TX_DELAY_MS 8 //delay in ms between successful transmits
#define INJECT_RETRANSMIT_BEFORE_FAIL 10
#define INJECT_RINGBUF_BYTES (1<<15) //Very large 32KB ring buffer

NRF_RINGBUF_DEF(m_test_ringbuf, INJECT_RINGBUF_BYTES);

typedef enum inject_task_type inject_task_type_t;
typedef struct inject_task inject_task_t;
uint32_t ringbuf_available();
bool push_task_delay(uint32_t delay_ms);
bool push_task_string(logitacker_keyboard_map_lang_t lang, char * str);
bool push_task(inject_task_t task);
bool pop_task(inject_task_t * p_task);
bool free_task(inject_task_t task);
bool flush_tasks();

// ToDo: change to initialized -> idle -> working -> idle -> not_initialized (SUCCESS/FAIL states aren't needed, proper events could be fired while processing)
typedef enum {
    INJECT_STATE_IDLE,
    INJECT_STATE_WORKING,
    INJECT_STATE_SUCCEEDED,
    INJECT_STATE_FAILED,
    INJECT_STATE_NOT_INITIALIZED,
} inject_state_t;

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
    bool finished; // ToDo: not needed, only one task at a time -> covered by inject_state_t

    union {
        uint8_t* p_data_u8;
        char* p_data_c;
    };
} inject_task_t;

typedef struct {
    logitacker_mainstate_t * p_logitacker_mainstate;

    uint8_t current_rf_address[5];

    uint8_t base_addr[4];
    uint8_t prefix;

    uint8_t tx_delay_ms;

    bool receiver_in_range;
    app_timer_id_t timer_next_action;

    inject_state_t state;

    nrf_esb_payload_t tmp_tx_payload;
//    nrf_esb_payload_t tmp_rx_payload;

    int retransmit_counter;

    inject_task_t current_task;

    logitacker_devices_unifying_device_t * p_device;
    logitacker_tx_payload_provider_t * p_payload_provider;
} logitacker_processor_inject_ctx_t;


void processor_inject_init_func(logitacker_processor_t *p_processor);
void processor_inject_init_func_(logitacker_processor_inject_ctx_t *self);

void processor_inject_deinit_func(logitacker_processor_t *p_processor);
void processor_inject_deinit_func_(logitacker_processor_inject_ctx_t *self);

void processor_inject_esb_handler_func(logitacker_processor_t *p_processor, nrf_esb_evt_t *p_esb_evt);
void processor_inject_esb_handler_func_(logitacker_processor_inject_ctx_t *self, nrf_esb_evt_t *p_esb_event);

void processor_inject_timer_handler_func(logitacker_processor_t *p_processor, void *p_timer_ctx);
void processor_inject_timer_handler_func_(logitacker_processor_inject_ctx_t *self, void *p_timer_ctx);

void processor_inject_bsp_handler_func(logitacker_processor_t *p_processor, bsp_event_t event);
void processor_inject_bsp_handler_func_(logitacker_processor_inject_ctx_t *self, bsp_event_t event);

void transfer_state(logitacker_processor_inject_ctx_t *self, inject_state_t new_state);
void logitacker_processor_inject_run_next_task(logitacker_processor_inject_ctx_t *self);

static logitacker_processor_t m_processor = {0};
static logitacker_processor_inject_ctx_t m_static_inject_ctx; //we reuse the same context, alternatively an malloc'ed ctx would allow separate instances

static char addr_str_buff[LOGITACKER_DEVICE_ADDR_STR_LEN] = {0};
static uint8_t tmp_addr[LOGITACKER_DEVICE_ADDR_LEN] = {0};
static logitacker_devices_unifying_device_t tmp_device = {0};
static bool m_ringbuf_initialized;

uint32_t ringbuf_available() {
    return m_test_ringbuf.bufsize_mask + 1 - (m_test_ringbuf.p_cb->wr_idx -  m_test_ringbuf.p_cb->rd_idx);
}

bool push_task(inject_task_t task) {
    // push header
    size_t len_hdr = sizeof(task);
    size_t len_data = task.data_len;
    size_t len = len_hdr + len_data;

    if (len > ringbuf_available()) {
        NRF_LOG_ERROR("Not enough memory in ring buffer");
        return false;
    }

    nrf_ringbuf_cpy_put(&m_test_ringbuf, (uint8_t*) &task, &len_hdr);
    if (len_hdr != sizeof(task)) {
        NRF_LOG_ERROR("push_task hasn't written full header");
        return false; //this means memory leak, as it can't be read back
    }


    if (len_data > 0) { //f.e. delay task has no body
        nrf_ringbuf_cpy_put(&m_test_ringbuf, task.p_data_u8, &len_data);
        if (len_data != task.data_len) {
            NRF_LOG_ERROR("push_task hasn't written full data");
            return false; //this means memory leak, as it can't be read back
        }
    }

    return true;
}

bool push_task_string(logitacker_keyboard_map_lang_t lang, char * str) {
    inject_task_t tmp_task = {0};
    tmp_task.data_len = strlen(str)+1; //include terminating 0x00
    tmp_task.p_data_c = str;
    tmp_task.type = INJECT_TASK_TYPE_TYPE_STRING;
    tmp_task.lang = lang;
    return push_task(tmp_task);
}

bool push_task_delay(uint32_t delay_ms) {
    inject_task_t tmp_task = {0};
    tmp_task.delay_ms = delay_ms;
    tmp_task.data_len = 0; //include terminating 0x00
    tmp_task.p_data_c = NULL;
    tmp_task.type = INJECT_TASK_TYPE_DELAY;
    return push_task(tmp_task);

}

bool push_task_press(logitacker_keyboard_map_lang_t lang, char * str_combo) {
    inject_task_t tmp_task = {0};
    tmp_task.data_len = sizeof(str_combo) + 1; //include terminating 0x00
    tmp_task.p_data_c = str_combo;
    tmp_task.type = INJECT_TASK_TYPE_PRESS_KEYS;
    tmp_task.lang = lang;
    return push_task(tmp_task);
}

bool pop_task(inject_task_t * p_task) {
    if (ringbuf_available() == INJECT_RINGBUF_BYTES) {
        NRF_LOG_ERROR("No more elements to pop in ring buffer");
        return false;
    }


    size_t read_len = sizeof(inject_task_t);
    nrf_ringbuf_cpy_get(&m_test_ringbuf, (uint8_t *) p_task, &read_len);
    if (read_len != sizeof(inject_task_t)) {
        NRF_LOG_ERROR("pop_task wasn't able to read full header");
        return false; //this means memory leak, as we can't use the data
    }
    // bugfix: tmp_rd_idx doesn't follow rd_idx if nrf_ringbuf_get and nrf_ring_buf_cpy_get are used alternating
    m_test_ringbuf.p_cb->tmp_rd_idx = m_test_ringbuf.p_cb->rd_idx;


    read_len = p_task->data_len;
    uint32_t err_code = nrf_ringbuf_get(&m_test_ringbuf, &(p_task->p_data_u8), &read_len, true);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_ERROR("pop_task wasn't able to get a pointer to the body data");
        return false; //this means memory leak, as we can't use the data
    }

    return true;
}

bool free_task(inject_task_t task) {
    return nrf_ringbuf_free(&m_test_ringbuf, task.data_len) == NRF_SUCCESS;
}

bool flush_tasks() {
    nrf_ringbuf_init(&m_test_ringbuf);
    return true;
}

logitacker_processor_t * contruct_processor_inject_instance(logitacker_processor_inject_ctx_t *const inject_ctx) {
    m_processor.p_ctx = inject_ctx;
    m_processor.p_init_func = processor_inject_init_func;
    m_processor.p_deinit_func = processor_inject_deinit_func;
    m_processor.p_esb_handler = processor_inject_esb_handler_func;
    m_processor.p_timer_handler = processor_inject_timer_handler_func;
    m_processor.p_bsp_handler = processor_inject_bsp_handler_func;

    return &m_processor;
}

void processor_inject_init_func(logitacker_processor_t *p_processor) {
    processor_inject_init_func_((logitacker_processor_inject_ctx_t *) p_processor->p_ctx);
}

void processor_inject_deinit_func(logitacker_processor_t *p_processor) {
    processor_inject_deinit_func_((logitacker_processor_inject_ctx_t *) p_processor->p_ctx);
}

void processor_inject_esb_handler_func(logitacker_processor_t *p_processor, nrf_esb_evt_t *p_esb_evt) {
    processor_inject_esb_handler_func_((logitacker_processor_inject_ctx_t *) p_processor->p_ctx, p_esb_evt);
}

void processor_inject_timer_handler_func(logitacker_processor_t *p_processor, void *p_timer_ctx) {
    processor_inject_timer_handler_func_((logitacker_processor_inject_ctx_t *) p_processor->p_ctx, p_timer_ctx);
}

void processor_inject_bsp_handler_func(logitacker_processor_t *p_processor, bsp_event_t event) {
    processor_inject_bsp_handler_func_((logitacker_processor_inject_ctx_t *) p_processor->p_ctx, event);
}

void processor_inject_bsp_handler_func_(logitacker_processor_inject_ctx_t *self, bsp_event_t event) {
    // do nothing
}


void processor_inject_init_func_(logitacker_processor_inject_ctx_t *self) {
    *self->p_logitacker_mainstate = LOGITACKER_MAINSTATE_INJECT;
    self->tx_delay_ms = INJECT_TX_DELAY_MS;

    helper_addr_to_base_and_prefix(self->base_addr, &self->prefix, self->current_rf_address, LOGITACKER_DEVICE_ADDR_LEN);

    helper_addr_to_hex_str(addr_str_buff, LOGITACKER_DEVICE_ADDR_LEN, self->current_rf_address);
    NRF_LOG_INFO("Initializing injection mode for %s", addr_str_buff);

    radio_disable_rx_timeout_event(); // disable RX timeouts
    radio_stop_channel_hopping(); // disable channel hopping
    nrf_esb_stop_rx(); //stop rx in case running

    // set current address for pipe 1
    nrf_esb_enable_pipes(0x00); //disable all pipes
    nrf_esb_set_base_address_0(self->base_addr); // set base addr 0
    nrf_esb_update_prefix(0, self->prefix); // set prefix and enable pipe 0
    nrf_esb_enable_pipes(0x01); //enable pipe 0


    // clear TX/RX payload buffers (just to be sure)
    memset(&self->tmp_tx_payload, 0, sizeof(self->tmp_tx_payload)); //unset TX payload
//    memset(&self->tmp_rx_payload, 0, sizeof(self->tmp_rx_payload)); //unset RX payload

    // prepare test TX payload (first report will be a pairing request)
    self->tmp_tx_payload.noack = false;
    self->state = INJECT_STATE_IDLE;
    self->current_task.finished = true; // set initial task to finished

    self->retransmit_counter = 0;

    // setup radio as PTX
    nrf_esb_set_mode(NRF_ESB_MODE_PTX);
    nrf_esb_enable_all_channel_tx_failover(true); //retransmit payloads on all channels if transmission fails
    nrf_esb_set_all_channel_tx_failover_loop_count(2); //iterate over channels two time before failing
    nrf_esb_set_retransmit_count(1);
    nrf_esb_set_retransmit_delay(250);
    nrf_esb_set_tx_power(NRF_ESB_TX_POWER_8DBM);
}

void processor_inject_deinit_func_(logitacker_processor_inject_ctx_t *self) {
    *self->p_logitacker_mainstate = LOGITACKER_MAINSTATE_IDLE;

    NRF_LOG_INFO("Stop injection mode for address %s", addr_str_buff);

    radio_disable_rx_timeout_event(); // disable RX timeouts
    radio_stop_channel_hopping(); // disable channel hopping
    nrf_esb_stop_rx(); //stop rx in case running

    nrf_esb_set_mode(NRF_ESB_MODE_PROMISCOUS); //should disable and end up in idle state

    // set current address for pipe 1
    nrf_esb_enable_pipes(0x00); //disable all pipes

    // reset inner loop count
    self->prefix = 0x00; //unset prefix
    memset(self->base_addr, 0, 4); //unset base address
    memset(self->current_rf_address, 0, 5); //unset RF address

    memset(&self->tmp_tx_payload, 0, sizeof(self->tmp_tx_payload)); //unset TX payload
//    memset(&self->tmp_rx_payload, 0, sizeof(self->tmp_rx_payload)); //unset RX payload

    self->state = INJECT_STATE_NOT_INITIALIZED;
    self->retransmit_counter = 0;

    flush_tasks();
    nrf_esb_enable_all_channel_tx_failover(false); // disable all channel failover
}

void processor_inject_timer_handler_func_(logitacker_processor_inject_ctx_t *self, void *p_timer_ctx) {
    if (self->state == INJECT_STATE_WORKING) {
        switch (self->current_task.type) {
            case INJECT_TASK_TYPE_DELAY:
            {
                NRF_LOG_INFO("DELAY end reached");
                self->current_task.finished = true;
                transfer_state(self, INJECT_STATE_IDLE);
                free_task(self->current_task);
                break;
            }
            case INJECT_TASK_TYPE_PRESS_KEYS:
            case INJECT_TASK_TYPE_TYPE_STRING:
            {
                // if timer is called, write (and auto transmit) current ESB payload
                unifying_payload_update_checksum(self->tmp_tx_payload.data, self->tmp_tx_payload.length);

                if (nrf_esb_write_payload(&self->tmp_tx_payload) != NRF_SUCCESS) {
                    NRF_LOG_INFO("Error writing payload");
                } else {
                    nrf_esb_convert_pipe_to_address(self->tmp_tx_payload.pipe, tmp_addr);
                    helper_addr_to_hex_str(addr_str_buff, 5, tmp_addr);
                    NRF_LOG_INFO("TX'ed to %s", nrf_log_push(addr_str_buff));
                }
                break;
            }
            default:
                NRF_LOG_WARNING("timer event fired for unhandled TASK TYPE: %d", self->current_task.type);
                break;
        }
    }

}

// runs next task if state is transferred back to idle
void transfer_state(logitacker_processor_inject_ctx_t *self, inject_state_t new_state) {
    if (new_state == self->state) return; //no state change

    bool reset_payload_provider = false;
    bool run_next_task = false;
    switch (new_state) {
        case INJECT_STATE_IDLE:
            // stop all actions, send notification to callback
            app_timer_stop(self->timer_next_action);
            self->retransmit_counter = 0;
            self->state = new_state;
            reset_payload_provider = true;
            run_next_task = true;
            break;
        case INJECT_STATE_SUCCEEDED:
            // ToDo: callback notify
            app_timer_stop(self->timer_next_action);
            self->retransmit_counter = 0;
            reset_payload_provider = true;
            run_next_task = true;
            self->state = new_state;
            break;
        case INJECT_STATE_FAILED:
            // ToDo: callback notify
            app_timer_stop(self->timer_next_action);
            self->retransmit_counter = 0;
            reset_payload_provider = true;
            run_next_task = true;
            self->state = new_state;
            break;
        default:
            self->state = new_state;
            break;
    }

    if (reset_payload_provider) {
        if (self->p_payload_provider != NULL) (*self->p_payload_provider->p_reset)(self->p_payload_provider);
    }


    if (run_next_task) {
        self->current_task.finished = true;
        free_task(self->current_task);
        logitacker_processor_inject_run_next_task(self);
    }
}

void processor_inject_esb_handler_func_(logitacker_processor_inject_ctx_t *self, nrf_esb_evt_t *p_esb_event) {

/*
    uint32_t channel_freq;
    nrf_esb_get_rf_frequency(&channel_freq);
*/
    if (self->retransmit_counter >= INJECT_RETRANSMIT_BEFORE_FAIL) {
        NRF_LOG_WARNING("Too many retransmissions")
        self->state = INJECT_STATE_FAILED;
    }

    if (self->state == INJECT_STATE_FAILED) {
        NRF_LOG_WARNING("Injection failed, switching mode to discovery");
        transfer_state(self,INJECT_STATE_IDLE);
        logitacker_enter_mode_discovery();
        return;
    }

    switch (p_esb_event->evt_id) {
        case NRF_ESB_EVENT_TX_FAILED:
        {
            //re-transmit last frame (payload still enqued)
            nrf_esb_start_tx();
            self->retransmit_counter++;
            break;
        }
        case NRF_ESB_EVENT_TX_SUCCESS_ACK_PAY:
            nrf_esb_flush_rx(); //ignore inbound payloads
            // fall through
        case NRF_ESB_EVENT_TX_SUCCESS:
        {
            NRF_LOG_INFO("TX_SUCCESS");
            self->retransmit_counter = 0;

            if (self->p_payload_provider == NULL) {
                transfer_state(self, INJECT_STATE_IDLE);
                return;
            }

            // fetch next payload
            if ((*self->p_payload_provider->p_get_next)(self->p_payload_provider, &self->tmp_tx_payload)) {
                //next payload retrieved
                NRF_LOG_INFO("New payload retrieved from TX_payload_provider");
                // schedule payload transmission
                app_timer_start(self->timer_next_action, APP_TIMER_TICKS(self->tx_delay_ms), NULL);

            } else {
                // no more payloads, we succeeded
                transfer_state(self, INJECT_STATE_SUCCEEDED);
            }

            // schedule next payload

            break;
        }
        case NRF_ESB_EVENT_RX_RECEIVED:
        {
            NRF_LOG_ERROR("ESB EVENT HANDLER PAIR DEVICE RX_RECEIVED ... !!shouldn't happen!!");
            break;
        }
    }


    if (self->state == INJECT_STATE_SUCCEEDED) {
        NRF_LOG_WARNING("Injection succeeded, switching mode to discovery");
        logitacker_enter_mode_discovery();
        return;
    }

    return;
}

void logitacker_processor_inject_process_task_string(logitacker_processor_inject_ctx_t *self) {
    NRF_LOG_INFO("process string injection: %s", self->current_task.p_data_c);
    self->p_payload_provider = new_payload_provider_string(self->p_device, self->current_task.lang, self->current_task.p_data_c);
    //while ((*p_pay_provider->p_get_next)(p_pay_provider, &tmp_pay)) {};

    //fetch first payload
    if (!(*self->p_payload_provider->p_get_next)(self->p_payload_provider, &self->tmp_tx_payload)) {
        //failed to fetch first payload
        NRF_LOG_WARNING("failed to fetch initial RF report from payload provider");
        return;
    }

    self->state = INJECT_STATE_WORKING;

    //start injection
    app_timer_start(self->timer_next_action, APP_TIMER_TICKS(self->tx_delay_ms), NULL);
}

void logitacker_processor_inject_process_task_press(logitacker_processor_inject_ctx_t *self) {
    NRF_LOG_INFO("process key-combo injection: %s", self->current_task.p_data_c);
    self->p_payload_provider = new_payload_provider_press(self->p_device, self->current_task.lang, self->current_task.p_data_c);
    //while ((*p_pay_provider->p_get_next)(p_pay_provider, &tmp_pay)) {};

    //fetch first payload
    if (!(*self->p_payload_provider->p_get_next)(self->p_payload_provider, &self->tmp_tx_payload)) {
        //failed to fetch first payload
        NRF_LOG_WARNING("failed to fetch initial RF report from payload provider");
        return;
    }

    self->state = INJECT_STATE_WORKING;

    //start injection
    app_timer_start(self->timer_next_action, APP_TIMER_TICKS(self->tx_delay_ms), NULL);
}

void logitacker_processor_inject_process_task_delay(logitacker_processor_inject_ctx_t *self) {
    NRF_LOG_INFO("process delay injection: %d milliseconds", self->current_task.delay_ms);
    self->p_payload_provider = NULL;

    self->state = INJECT_STATE_WORKING;

    //start injection
    if (self->current_task.delay_ms == 0) self->current_task.delay_ms = 1; //avoid zero delay, alternatively the timer handler could be called directly
    app_timer_start(self->timer_next_action, APP_TIMER_TICKS(self->current_task.delay_ms), NULL);
}


void logitacker_processor_inject_run_next_task(logitacker_processor_inject_ctx_t *self) {
    if (self == NULL) {
        NRF_LOG_ERROR("logitacker processor inject context is NULL");
        return;
    }

    // pop task
    if (!self->current_task.finished) {
        NRF_LOG_ERROR("current task not finished");
        return;
    }
    if (!pop_task(&self->current_task)) {
        NRF_LOG_INFO("No more tasks scheduled");
        self->state = INJECT_STATE_IDLE;
        logitacker_enter_mode_discovery();
        return;
    }

    switch (self->current_task.type) {
        case INJECT_TASK_TYPE_PRESS_KEYS:
            logitacker_processor_inject_process_task_press(self);
            break;
        case INJECT_TASK_TYPE_TYPE_STRING:
            logitacker_processor_inject_process_task_string(self);
            break;
        case INJECT_TASK_TYPE_DELAY:
            logitacker_processor_inject_process_task_delay(self);
            break;
        default:
            NRF_LOG_ERROR("unhandled task type %d, dropped ...", self->current_task.type);
            self->current_task.finished = true;
            free_task(self->current_task);

            break;
    }
}

void logitacker_processor_inject_string(logitacker_processor_t * p_processor_inject, logitacker_keyboard_map_lang_t lang, char * str) {
    if (p_processor_inject == NULL) {
        NRF_LOG_ERROR("logitacker processor is NULL");
        return;
    }

    logitacker_processor_inject_ctx_t * self = (logitacker_processor_inject_ctx_t *) p_processor_inject->p_ctx;
    if (self == NULL) {
        NRF_LOG_ERROR("logitacker processor inject context is NULL");
        return;
    }

    push_task_string(lang, str);

    if (self->state == INJECT_STATE_IDLE) {
        logitacker_processor_inject_run_next_task(self);
    }
}

void logitacker_processor_inject_press(logitacker_processor_t * p_processor_inject, logitacker_keyboard_map_lang_t lang, char * combo_str) {
    if (p_processor_inject == NULL) {
        NRF_LOG_ERROR("logitacker processor is NULL");
        return;
    }

    logitacker_processor_inject_ctx_t * self = (logitacker_processor_inject_ctx_t *) p_processor_inject->p_ctx;
    if (self == NULL) {
        NRF_LOG_ERROR("logitacker processor inject context is NULL");
        return;
    }


    push_task_press(lang, combo_str);


    if (self->state == INJECT_STATE_IDLE) {
        logitacker_processor_inject_run_next_task(self);
    }
}

void logitacker_processor_inject_delay(logitacker_processor_t * p_processor_inject, uint32_t delay_ms) {
    if (p_processor_inject == NULL) {
        NRF_LOG_ERROR("logitacker processor is NULL");
        return;
    }

    logitacker_processor_inject_ctx_t * self = (logitacker_processor_inject_ctx_t *) p_processor_inject->p_ctx;
    if (self == NULL) {
        NRF_LOG_ERROR("logitacker processor inject context is NULL");
        return;
    }

    push_task_delay(delay_ms);

    if (self->state == INJECT_STATE_IDLE) {
        logitacker_processor_inject_run_next_task(self);
    }

}

logitacker_processor_t * new_processor_inject(uint8_t const *target_rf_address, app_timer_id_t timer_next_action) {
    if (!m_ringbuf_initialized) {
        nrf_ringbuf_init(&m_test_ringbuf);
        m_ringbuf_initialized = true;
    }

    // initialize context (static in this case, has to use malloc for new instances)
    logitacker_processor_inject_ctx_t *const p_ctx = &m_static_inject_ctx;
    memset(p_ctx, 0, sizeof(*p_ctx)); //replace with malloc for dedicated instance

    //initialize member variables
    memcpy(p_ctx->current_rf_address, target_rf_address, 5);
    p_ctx->timer_next_action = timer_next_action;


    p_ctx->p_device = NULL;
    logitacker_devices_get_device(&p_ctx->p_device, p_ctx->current_rf_address);
    if (p_ctx->p_device == NULL) {
        NRF_LOG_WARNING("device not found, creating capabilities");
        //tmp_device.is_encrypted = false;
        memcpy(tmp_device.rf_address, p_ctx->current_rf_address, 5);
        p_ctx->p_device = &tmp_device;
    }


    return contruct_processor_inject_instance(&m_static_inject_ctx);
}
