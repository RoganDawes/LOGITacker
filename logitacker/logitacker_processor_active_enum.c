#include "nrf.h"
#include "logitacker.h"
#include "logitacker_processor_active_enum.h"
#include "logitacker_processor.h"
#include "helper.h"
#include "string.h"
#include "logitacker_devices.h"
#include "logitacker_unifying.h"
#include "logitacker_options.h"
#include "logitacker_flash.h"

#define NRF_LOG_MODULE_NAME LOGITACKER_PROCESSOR_ACTIVE_ENUM
#include "nrf_log.h"

NRF_LOG_MODULE_REGISTER();


#define PAIRING_REQ_MARKER_BYTE 0xee // byte used as device ID in pairing requests

// ToDo: Implement deinit and reset functions of processor interface

/*
 * ToDo: Due to forced pairing test, the dongle changes unused device slots to offer new RF addresses
 * (listens on new prefixes while dropping old ones). As the dongle couldn't listen on more than 7 prefixes a time
 * (00 for dongle address and 6 device addresses) the backing data struct for discovered neighbours limits the
 * maximum storable prefixes per base address to 7 (defined by LOGITACKER_DEVICE_DONGLE_MAX_PREFIX). This is an issue,
 * if prefixes change during neighbour discovery. To account for this, active enum should store all results to a temporary
 * data structure. In case a full prefix range (0x00 + 6 device prefixes) during neighbour discovery, the result could be
 * used to replace the current prefixes for the corresponding base address.
 *
 * CAUTION: It should be avoided to delete a prefix, which has a stored link encryption key (f.e. from sniff pairing),
 * as neighbour discovery could miss a valid prefix address, and thus accidentally delete the corresponding key along
 * with the prefix.
 *
 * For now, we keep the behavior of changing prefixes during neighbor discovery as "known bug". This could result in an
 * active scan, where not all discovered prefixes could be stored. Anyways, chances are high, that for a dongle vulnerable
 * to forced pairing, valid prefixes for this attack vector still get stored (as the vulnerability should apply to almost
 * all valid prefixes). In contrast, there could be rare cases, where a prefix vulnerable to plain keystroke injection
 * couldn't be stored.
 *
 * A fallback to allow re-enumeration of base addresses with to many prefixes stored, would be to delete the corresponding
 * device addresses manually (functionality has to be implemented).
 *
 */

typedef enum {
    ACTIVE_ENUM_PHASE_STARTED,   // try to reach dongle RF address for device
    ACTIVE_ENUM_PHASE_FINISHED,   // try to reach dongle RF address for device
    ACTIVE_ENUM_PHASE_RUNNING_TESTS   // tests if plain keystrokes could be injected (press CAPS, listen for LED reports)
} active_enumeration_phase_t;



typedef struct {
//    logitacker_mode_t * p_logitacker_mainstate;

    uint8_t inner_loop_count; // how many successfull transmissions to RF address of current prefix
    uint8_t led_count;
    uint8_t current_rf_address[5];

    uint8_t base_addr[4];
    uint8_t known_prefix;
    uint8_t next_prefix;

    uint8_t tx_delay_ms;

    bool receiver_in_range;
    active_enumeration_phase_t phase;
    app_timer_id_t timer_next_action;

    nrf_esb_payload_t tmp_tx_payload;
    nrf_esb_payload_t tmp_rx_payload;

    logitacker_devices_unifying_dongle_t * p_dongle;
} logitacker_processor_active_enum_ctx_t;

static logitacker_processor_t m_processor = {0};
static logitacker_processor_active_enum_ctx_t m_static_active_enum_ctx; //we reuse the same context, alternatively an malloc'ed ctx would allow separate instances

static char addr_str_buff[LOGITACKER_DEVICE_ADDR_STR_LEN] = {0};

static uint8_t rf_report_plain_keys_caps[] = { 0x00, 0xC1, 0x00, 0x39, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static uint8_t rf_report_plain_keys_release[] = { 0x00, 0xC1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static uint8_t rf_report_pairing_request1[] = { 0xee, 0x5F, 0x01, 0x99, 0x89, 0x9C, 0xCA, 0xB4, 0x08, 0x40, 0x4D, 0x04, 0x02, 0x01, 0x47, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79 };
static uint8_t rf_report_keep_alive[] = { 0x00, 0x40, 0x00, 0x08, 0x00 };

void processor_active_enum_init_func(logitacker_processor_t *p_processor);
void processor_active_enum_init_func_(logitacker_processor_active_enum_ctx_t *self);

void processor_active_enum_deinit_func(logitacker_processor_t *p_processor);
void processor_active_enum_deinit_func_(logitacker_processor_active_enum_ctx_t *self);

void processor_active_enum_esb_handler_func(logitacker_processor_t *p_processor, nrf_esb_evt_t *p_esb_evt);
void processor_active_enum_esb_handler_func_(logitacker_processor_active_enum_ctx_t *self, nrf_esb_evt_t *p_esb_event);

void processor_active_enum_timer_handler_func(logitacker_processor_t *p_processor, void *p_timer_ctx);
void processor_active_enum_timer_handler_func_(logitacker_processor_active_enum_ctx_t *self, void *p_timer_ctx);

void processor_active_enum_bsp_handler_func(logitacker_processor_t *p_processor, bsp_event_t event);
void processor_active_enum_bsp_handler_func_(logitacker_processor_active_enum_ctx_t *self, bsp_event_t event);


//void processor_active_enum_update_tx_payload(uint8_t iteration_count);
void processor_active_enum_update_tx_payload(logitacker_processor_active_enum_ctx_t *self);
void processor_active_enum_add_device_address_to_list(logitacker_processor_active_enum_ctx_t *self);
bool processor_active_enum_advance_to_next_addr_prefix(logitacker_processor_active_enum_ctx_t *self);



logitacker_processor_t * contruct_processor_active_enum_instance(logitacker_processor_active_enum_ctx_t *const active_enum_ctx) {
    m_processor.p_ctx = active_enum_ctx;
    m_processor.p_init_func = processor_active_enum_init_func;
    m_processor.p_deinit_func = processor_active_enum_deinit_func;
    m_processor.p_esb_handler = processor_active_enum_esb_handler_func;
    m_processor.p_timer_handler = processor_active_enum_timer_handler_func;
    m_processor.p_bsp_handler = processor_active_enum_bsp_handler_func;

    return &m_processor;
}

void processor_active_enum_bsp_handler_func(logitacker_processor_t *p_processor, bsp_event_t event) {
    processor_active_enum_bsp_handler_func_((logitacker_processor_active_enum_ctx_t *) p_processor->p_ctx, event);
}

void processor_active_enum_bsp_handler_func_(logitacker_processor_active_enum_ctx_t *self, bsp_event_t event) {

}

void processor_active_enum_init_func(logitacker_processor_t *p_processor) {
    processor_active_enum_init_func_((logitacker_processor_active_enum_ctx_t *) p_processor->p_ctx);
}

void processor_active_enum_init_func_(logitacker_processor_active_enum_ctx_t *self) {
//    *self->p_logitacker_mainstate = LOGITACKER_MODE_ACTIVE_ENUMERATION;
    self->tx_delay_ms = ACTIVE_ENUM_TX_DELAY_MS;

    //helper_addr_to_base_and_prefix(m_state_local.substate_active_enumeration.base_addr, &m_state_local.substate_active_enumeration.known_prefix, rf_address, LOGITACKER_DEVICE_ADDR_LEN);
    helper_addr_to_base_and_prefix(self->base_addr, &self->known_prefix, self->current_rf_address, LOGITACKER_DEVICE_ADDR_LEN);
    self->next_prefix = self->known_prefix-1;

    helper_addr_to_hex_str(addr_str_buff, LOGITACKER_DEVICE_ADDR_LEN, self->current_rf_address);
    NRF_LOG_INFO("Start active enumeration for address %s", addr_str_buff);

    radio_disable_rx_timeout_event(); // disable RX timeouts
    radio_stop_channel_hopping(); // disable channel hopping
    nrf_esb_stop_rx(); //stop rx in case running

    // set current address for pipe 1
    nrf_esb_enable_pipes(0x00); //disable all pipes
    nrf_esb_set_base_address_1(self->base_addr); // set base addr1
    nrf_esb_update_prefix(1, self->known_prefix); // set prefix and enable pipe 1


    // clear TX/RX payload buffers (just to be sure)
    memset(&self->tmp_tx_payload, 0, sizeof(self->tmp_tx_payload)); //unset TX payload
    memset(&self->tmp_rx_payload, 0, sizeof(self->tmp_rx_payload)); //unset RX payload

    self->p_dongle = NULL;

    // prepare test TX payload (first report will be a pairing request)
    self->inner_loop_count = 0;
    self->tmp_tx_payload.noack = false;
    processor_active_enum_update_tx_payload(self);

    // setup radio as PTX
    nrf_esb_set_mode(NRF_ESB_MODE_PTX);
    nrf_esb_enable_all_channel_tx_failover(true); //retransmit payloads on all channels if transmission fails
    nrf_esb_set_all_channel_tx_failover_loop_count(2); //iterate over channels two time before failing
    nrf_esb_set_retransmit_count(2);
    nrf_esb_set_retransmit_delay(250);
    nrf_esb_set_tx_power(NRF_ESB_TX_POWER_8DBM);

    self->phase = ACTIVE_ENUM_PHASE_STARTED;

    // write payload (autostart TX is enabled for PTX mode)
    nrf_esb_write_payload(&self->tmp_tx_payload);
}

void processor_active_enum_deinit_func(logitacker_processor_t *p_processor) {
    processor_active_enum_deinit_func_((logitacker_processor_active_enum_ctx_t *) p_processor->p_ctx);
}

void processor_active_enum_deinit_func_(logitacker_processor_active_enum_ctx_t *self) {
//    *self->p_logitacker_mainstate = LOGITACKER_MODE_IDLE;

    NRF_LOG_INFO("DEINIT active enumeration for address %s", addr_str_buff);

    radio_disable_rx_timeout_event(); // disable RX timeouts
    radio_stop_channel_hopping(); // disable channel hopping
    nrf_esb_stop_rx(); //stop rx in case running

    nrf_esb_set_mode(NRF_ESB_MODE_PRX); //should disable and end up in idle state

    // set current address for pipe 1
    nrf_esb_enable_pipes(0x00); //disable all pipes

    // reset inner loop count
    self->inner_loop_count = 0;
    self->known_prefix = 0x00; //unset prefix
    self->next_prefix = 0x00; //unset next prefix
    self->led_count = 0x00; //unset LED count
    self->phase = ACTIVE_ENUM_PHASE_FINISHED;
    memset(self->base_addr, 0, 4); //unset base address
    memset(self->current_rf_address, 0, 5); //unset RF address

    memset(&self->tmp_tx_payload, 0, sizeof(self->tmp_tx_payload)); //unset TX payload
    memset(&self->tmp_rx_payload, 0, sizeof(self->tmp_rx_payload)); //unset RX payload

    nrf_esb_enable_all_channel_tx_failover(false); // disable all channel failover
}

void processor_active_enum_timer_handler_func(logitacker_processor_t *p_processor, void *p_timer_ctx) {
    processor_active_enum_timer_handler_func_((logitacker_processor_active_enum_ctx_t *) p_processor->p_ctx, p_timer_ctx);
}

void processor_active_enum_timer_handler_func_(logitacker_processor_active_enum_ctx_t *self, void *p_timer_ctx) {
    // if timer is called, write (and auto transmit) current ESB payload
    self->phase = ACTIVE_ENUM_PHASE_RUNNING_TESTS;

    // write payload (autostart TX is enabled for PTX mode)
    //NRF_LOG_HEXDUMP_INFO(self->tmp_tx_payload.data, self->tmp_tx_payload.length);
    if (nrf_esb_write_payload(&self->tmp_tx_payload) != NRF_SUCCESS) {
        NRF_LOG_INFO("Error writing payload");
    }

}

void processor_active_enum_esb_handler_func(logitacker_processor_t *p_processor, nrf_esb_evt_t *p_esb_evt) {
    processor_active_enum_esb_handler_func_((logitacker_processor_active_enum_ctx_t *) p_processor->p_ctx, p_esb_evt);
}

void processor_active_enum_esb_handler_func_(logitacker_processor_active_enum_ctx_t *self, nrf_esb_evt_t *p_esb_event) {
    if (self->phase == ACTIVE_ENUM_PHASE_FINISHED) {
        NRF_LOG_WARNING("Active enumeration event, while active enumeration finished");
        goto ACTIVE_ENUM_FINISHED;
    }


    uint32_t channel_freq;
    nrf_esb_get_rf_frequency(&channel_freq);

    switch (p_esb_event->evt_id) {
        case NRF_ESB_EVENT_TX_FAILED:
        {
            // if very first transmission fails (for the prefix which should definitely be reachable) --> whole active enum failed
            NRF_LOG_DEBUG("ACTIVE ENUMERATION TX_FAILED channel: %d", channel_freq);
            if (self->phase == ACTIVE_ENUM_PHASE_STARTED) {
                self->receiver_in_range = false;
                self->phase = ACTIVE_ENUM_PHASE_FINISHED;
                NRF_LOG_INFO("Failed to reach receiver in first transmission, aborting active enumeration");
                goto ACTIVE_ENUM_FINISHED;
            }

            if (self->current_rf_address[4] == 0x00) { // note next prefix wasn't update,yet - thus it represents the prefix for which TX failed
                //if prefix 0x00 isn't reachable, it is unlikely that this is a Logitech dongle, thus we remove the device from the list and abort enumeration
                NRF_LOG_INFO("Address prefix 0x00 not reachable, either no Unifying device or dongle not in range");
//                NRF_LOG_INFO("Address prefix 0x00 not reachable, either no Unifying device or dongle not in range ... stop neighbor discovery");
/*
                logitacker_devices_unifying_dongle_t *p_dongle = NULL;
                logitacker_devices_get_dongle_by_device_addr(&p_dongle, self->current_rf_address);
                if (p_dongle != NULL) {
                    //logitacker_devices_del_dongle(p_dongle->base_addr);

                    //abort enumeration
                    self->phase = ACTIVE_ENUM_PHASE_FINISHED;
                    goto ACTIVE_ENUM_FINISHED;
                    //p_dongle->is_logitech = false;
                }
*/
            }

            // continue with next prefix, return if first prefix has been reached again
            if (processor_active_enum_advance_to_next_addr_prefix(self)) goto ACTIVE_ENUM_FINISHED;

            //re-transmit last frame (payload is still enqueued)
            nrf_esb_start_tx();


            break;
        }
        case NRF_ESB_EVENT_TX_SUCCESS_ACK_PAY:
        case NRF_ESB_EVENT_TX_SUCCESS:
        {
            NRF_LOG_DEBUG("ACTIVE ENUMERATION TX_SUCCESS channel: %d (loop %d)", channel_freq, self->inner_loop_count);

            // if first successful TX to this address, add to list
            if (self->inner_loop_count == 0) processor_active_enum_add_device_address_to_list(self);

            // update TX loop count
            self->inner_loop_count++;

            processor_active_enum_update_tx_payload(self);



            if (p_esb_event->evt_id == NRF_ESB_EVENT_TX_SUCCESS_ACK_PAY) {
                NRF_LOG_DEBUG("ACK_PAY channel (loop %d)", self->inner_loop_count);
                logitacker_devices_unifying_device_t * p_device = NULL;

                while (nrf_esb_read_rx_payload(&self->tmp_rx_payload) == NRF_SUCCESS) {
                    //Note: LED testing doesn't work on presenters like "R400", because no HID led output reports are sent
                    // test if LED report
                    uint8_t device_id = self->tmp_rx_payload.data[0];
                    uint8_t rf_report_type = self->tmp_rx_payload.data[1] & 0x1f;
                    if (rf_report_type == UNIFYING_RF_REPORT_LED) {
                        self->led_count++;
                        //device supports plain injection
                        NRF_LOG_INFO("ATTACK VECTOR: devices accepts plain keystroke injection (LED test succeeded)");
                        logitacker_devices_get_device(&p_device, self->current_rf_address);
                        if (p_device != NULL) {
                            p_device->vuln_plain_injection = true;
                            // set proper device capabilities and report types
                            p_device->report_types |= LOGITACKER_DEVICE_REPORT_TYPES_KEYBOARD;
                            p_device->caps &= ~LOGITACKER_DEVICE_CAPS_LINK_ENCRYPTION;
                            if (p_device->p_dongle != NULL) {
                                p_device->p_dongle->classification = DONGLE_CLASSIFICATION_IS_LOGITECH;
                            }

                            // if auto store is enabled, store to flash
                            if (g_logitacker_global_config.auto_store_plain_injectable) {
                                //check if already stored
                                logitacker_devices_unifying_device_t dummy_device;
                                if (logitacker_flash_get_device(&dummy_device, p_device->rf_address) != NRF_SUCCESS) {
                                    // not existing on flash create it
                                    if (logitacker_devices_store_ram_device_to_flash(p_device->rf_address) == NRF_SUCCESS) {
                                        NRF_LOG_INFO("device automatically stored to flash");
                                    }
                                } else {
                                    NRF_LOG_INFO("device already exists on flash");
                                }
                            }
                        }
                    } else if (rf_report_type == UNIFYING_RF_REPORT_PAIRING && device_id == PAIRING_REQ_MARKER_BYTE) { //data[0] holds byte used in request
                        //device supports plain injection
                        NRF_LOG_INFO("ATTACK VECTOR: forced pairing seems possible");
                        logitacker_devices_get_device(&p_device, self->current_rf_address);
                        if (p_device != NULL) {
                            p_device->vuln_forced_pairing = true;
                            logitacker_devices_unifying_dongle_t * p_dongle = p_device->p_dongle;
                            if (p_dongle != NULL) {
                                // dongle wpid is in response (byte 8,9)
                                memcpy(p_dongle->wpid, &self->tmp_rx_payload.data[9], 2);
                                if (p_dongle->wpid[0] == 0x88 && p_dongle->wpid[1] == 0x02) p_dongle->is_nordic = true;
                                if (p_dongle->wpid[0] == 0x88 && p_dongle->wpid[1] == 0x08) p_dongle->is_texas_instruments = true;
                                NRF_LOG_INFO("Dongle WPID is %.2X%.2X (TI: %s, Nordic: %s)", p_dongle->wpid[0], p_dongle->wpid[1], p_dongle->is_texas_instruments ? "yes" : "no", p_dongle->is_nordic ? "yes" : "no");
                            }

                        }

                    }

                    NRF_LOG_HEXDUMP_DEBUG(self->tmp_rx_payload.data, self->tmp_rx_payload.length);

                }
            }


            if (self->inner_loop_count < ACTIVE_ENUM_INNER_LOOP_MAX) {
                app_timer_start(self->timer_next_action, APP_TIMER_TICKS(self->tx_delay_ms), self->timer_next_action);
            } else {
                // we are done with this device

                // continue with next prefix, return if first prefix has been reached again
                if (processor_active_enum_advance_to_next_addr_prefix(self)) {
                    goto ACTIVE_ENUM_FINISHED;
                }

                // start next transmission
                if (nrf_esb_write_payload(&self->tmp_tx_payload) != NRF_SUCCESS) {
                    NRF_LOG_INFO("Error writing payload");
                }
            }

            break;
        }
        case NRF_ESB_EVENT_RX_RECEIVED:
        {
            NRF_LOG_INFO("ESB EVENT HANDLER ACTIVE ENUMERATION RX_RECEIVED ... !!shouldn't happen!!");
            break;
        }
    }

    ACTIVE_ENUM_FINISHED:
    if (self->phase == ACTIVE_ENUM_PHASE_FINISHED) {
        NRF_LOG_WARNING("Active enumeration finished, continue with passive enumeration");

        uint8_t rf_addr[5] = { 0 };
        helper_base_and_prefix_to_addr(rf_addr, self->base_addr, self->known_prefix, 5);
        //logitacker_enter_mode_passive_enum(rf_addr);
        logitacker_enter_mode_discovery();
        return;
    }

}




void processor_active_enum_add_device_address_to_list(logitacker_processor_active_enum_ctx_t *self) {
    logitacker_devices_unifying_device_t * p_device = NULL;

    // try to restore device from flash
    if (logitacker_devices_restore_device_from_flash(&p_device, self->current_rf_address) != NRF_SUCCESS) {
        // restore from flash failed, create in ram
        logitacker_devices_create_device(&p_device, self->current_rf_address);
    }


    if (p_device != NULL) {
        if (p_device->p_dongle != NULL) self->p_dongle = p_device->p_dongle;
        NRF_LOG_INFO("device address %s added/updated", addr_str_buff);
    } else {
        NRF_LOG_INFO("failed to add/update device address %s", addr_str_buff);
    }
}

// increments pipe 1 address prefix (neighbour discovery)
bool processor_active_enum_advance_to_next_addr_prefix(logitacker_processor_active_enum_ctx_t *self) {
    // all possible device_prefixes (neighbours) tested?
    if (self->next_prefix == self->known_prefix) {
        self->phase = ACTIVE_ENUM_PHASE_FINISHED;
        NRF_LOG_INFO("Tested all possible neighbours");
        if (self->p_dongle != NULL) self->p_dongle->active_enumeration_finished = true;
        return true;
    }

    nrf_esb_enable_pipes(0x00); //disable all pipes
    nrf_esb_update_prefix(1, self->next_prefix); // set prefix and enable pipe 1
    self->current_rf_address[4] = self->next_prefix;
    helper_addr_to_hex_str(addr_str_buff, LOGITACKER_DEVICE_ADDR_LEN, self->current_rf_address);

    NRF_LOG_INFO("Test next neighbour address %s", addr_str_buff);
    self->next_prefix--;
    self->inner_loop_count = 0; // reset TX_SUCCESS loop count
    self->led_count = 0; // reset RX LED report counter

    processor_active_enum_update_tx_payload(self);


    return false;
}

void processor_active_enum_update_tx_payload(logitacker_processor_active_enum_ctx_t *self) {

    uint8_t iter_mod_4 = self->inner_loop_count & 0x03;
/*
    char tmpstr[10];
    sprintf(tmpstr, "%d", self->inner_loop_count);
    NRF_LOG_INFO("inner loop count on TX update: %s", nrf_log_push(tmpstr));
*/
    if (self->inner_loop_count >= (ACTIVE_ENUM_INNER_LOOP_MAX - 4)) {
        // sequence: Pairing request phase 1, Keep-Alive, Keep-Alive, Keep-Alive
        switch (iter_mod_4) {
            case 0:
            {
                NRF_LOG_DEBUG("Update payload to PAIRING REQUEST");
                uint8_t len = sizeof(rf_report_pairing_request1);
                memcpy(self->tmp_tx_payload.data, rf_report_pairing_request1, len);
                self->tmp_tx_payload.length = len;
                break;
            }
            default:
                NRF_LOG_DEBUG("Update payload to KEEP ALIVE");
                memcpy(self->tmp_tx_payload.data, rf_report_keep_alive, sizeof(rf_report_keep_alive));
                self->tmp_tx_payload.length = sizeof(rf_report_keep_alive);
                break;
        }


    } else {
        // sequence: CAPS, Keep-Alive, Key release, keep-alive
        switch (iter_mod_4) {
            case 0:
            {
                NRF_LOG_DEBUG("Update payload to PLAIN KEY CAPS");
                memcpy(self->tmp_tx_payload.data, rf_report_plain_keys_caps, sizeof(rf_report_plain_keys_caps));
                self->tmp_tx_payload.length = sizeof(rf_report_plain_keys_caps);
                break;
            }
            case 2:
            {
                NRF_LOG_DEBUG("Update payload to PLAIN KEY RELEASE");
                memcpy(self->tmp_tx_payload.data, rf_report_plain_keys_release, sizeof(rf_report_plain_keys_release));
                self->tmp_tx_payload.length = sizeof(rf_report_plain_keys_release);
                break;
            }
            default:
                NRF_LOG_DEBUG("Update payload to KEEP ALIVE");
                memcpy(self->tmp_tx_payload.data, rf_report_keep_alive, sizeof(rf_report_keep_alive));
                self->tmp_tx_payload.length = sizeof(rf_report_keep_alive);
                break;
        }
    }

    self->tmp_tx_payload.pipe = 1;
    self->tmp_tx_payload.noack = false; // we need an ack

}

logitacker_processor_t * new_processor_active_enum(uint8_t *rf_address, app_timer_id_t timer_next_action) {
    //update checksums of RF payloads (only needed as they are not hardcoded, to allow changing payloads)
    logitacker_unifying_payload_update_checksum(rf_report_plain_keys_caps, sizeof(rf_report_plain_keys_caps));
    logitacker_unifying_payload_update_checksum(rf_report_plain_keys_release, sizeof(rf_report_plain_keys_release));
    rf_report_pairing_request1[0] = PAIRING_REQ_MARKER_BYTE;
    logitacker_unifying_payload_update_checksum(rf_report_pairing_request1, sizeof(rf_report_pairing_request1));
    logitacker_unifying_payload_update_checksum(rf_report_keep_alive, sizeof(rf_report_keep_alive));


    // initialize context (static in this case, has to use malloc for new instances)
    logitacker_processor_active_enum_ctx_t *const p_ctx = &m_static_active_enum_ctx;
    memset(p_ctx, 0, sizeof(*p_ctx)); //replace with malloc for dedicated instance

    //initialize member variables
    memcpy(p_ctx->current_rf_address, rf_address, 5);
    p_ctx->timer_next_action = timer_next_action;


    return contruct_processor_active_enum_instance(&m_static_active_enum_ctx);
}