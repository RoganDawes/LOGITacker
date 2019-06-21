#include "helper.h"
#include "logitacker_processor_discover.h"

#include "logitacker_processor_pair_sniff.h"
#include "logitacker.h"
#include "logitacker_devices.h"
#include "logitacker_options.h"
#include "logitacker_bsp.h"

#define NRF_LOG_MODULE_NAME LOGITACKER_PROCESSOR_DISCOVER
#include "nrf_log.h"
#include "logitacker_script_engine.h"
#include "logitacker_usb.h"

NRF_LOG_MODULE_REGISTER();


typedef struct {
    nrf_esb_payload_t tmp_rx_payload;
} logitacker_processor_discover_ctx_t;

static logitacker_processor_t m_processor = {0};
static logitacker_processor_discover_ctx_t m_static_discover_ctx; //we reuse the same context, alternatively an malloc'ed ctx would allow separate instances
static char addr_str_buff[LOGITACKER_DEVICE_ADDR_STR_LEN] = {0};

void processor_discover_init_func(logitacker_processor_t *p_processor);
void processor_discover_init_func_(logitacker_processor_discover_ctx_t *self);

void processor_discover_deinit_func(logitacker_processor_t *p_processor);
void processor_discover_deinit_func_(logitacker_processor_discover_ctx_t *self);

void processor_discover_esb_handler_func(logitacker_processor_t *p_processor, nrf_esb_evt_t *p_esb_evt);
void processor_discover_esb_handler_func_(logitacker_processor_discover_ctx_t *self, nrf_esb_evt_t *p_esb_event);

void processor_discover_radio_handler_func(logitacker_processor_t *p_processor, radio_evt_t const *p_event);
void processor_discover_radio_handler_func_(logitacker_processor_discover_ctx_t *self, radio_evt_t const *p_event);

void processor_discover_bsp_handler_func(logitacker_processor_t *p_processor, bsp_event_t event);
void processor_discover_bsp_handler_func_(logitacker_processor_discover_ctx_t *self, bsp_event_t event);

void discovery_process_rx(logitacker_processor_discover_ctx_t *self) {
    nrf_esb_payload_t * p_rx_payload = &self->tmp_rx_payload;

    while (nrf_esb_read_rx_payload(p_rx_payload) == NRF_SUCCESS) {
        if (p_rx_payload->validated_promiscuous_frame) {
            uint8_t len = p_rx_payload->length;
            uint8_t ch_idx = p_rx_payload->rx_channel_index;
            uint8_t ch = p_rx_payload->rx_channel;
            uint8_t addr[5];
            memcpy(addr, &p_rx_payload->data[2], 5);

            if (g_logitacker_global_config.discover_pass_through_hidraw) {
                logitacker_usb_write_hidraw_input_report_rf_frame(LOGITACKER_MODE_DISCOVERY, addr, p_rx_payload);
            }

            uint8_t prefix;
            uint8_t base[4];
            helper_addr_to_base_and_prefix(base, &prefix, addr, 5); //convert device addr to base+prefix and update device

            helper_addr_to_hex_str(addr_str_buff, LOGITACKER_DEVICE_ADDR_LEN, addr);
            NRF_LOG_INFO("DISCOVERY: received valid ESB frame (addr %s, len: %d, ch idx %d, raw ch %d, rssi %d)", addr_str_buff, len, ch_idx, ch, p_rx_payload->rssi);

            NRF_LOG_HEXDUMP_DEBUG(p_rx_payload->data, p_rx_payload->length);

            logitacker_devices_unifying_device_t *p_device = NULL;

            // check if deiscovered device already exists in RAM
            if (logitacker_devices_get_device(&p_device, addr) != NRF_SUCCESS) {
                // try to restore device from flash
                if (logitacker_devices_restore_device_from_flash(&p_device, addr) != NRF_SUCCESS) {
                    // restore from flash failed, create in ram
                    logitacker_devices_create_device(&p_device, addr);
                }

            }

            // update device counters
            //bool isLogitech = false;
            if (p_device != NULL) {
                // convert promisuous mode frame to default ESB frame
                nrf_esb_payload_t promiscuous_rx_payload;
                memcpy(&promiscuous_rx_payload, p_rx_payload, sizeof(nrf_esb_payload_t)); // create copy of promiscuous payload
                logitacker_radio_convert_promiscuous_frame_to_default_frame(p_rx_payload, promiscuous_rx_payload); // convert to default payload (no RF address in payload data etc.)

                // classify device (determin if it is Logitech)
                logitacker_devices_device_update_classification(p_device, *p_rx_payload);
                if (p_device->p_dongle != NULL) {
                    if (p_device->p_dongle->classification == DONGLE_CLASSIFICATION_IS_LOGITECH) {
                        NRF_LOG_INFO("discovered device is Logitech")
                        switch (g_logitacker_global_config.discovery_on_new_address) {
                            case OPTION_DISCOVERY_ON_NEW_ADDRESS_CONTINUE:
                                break;
                            case OPTION_DISCOVERY_ON_NEW_ADDRESS_SWITCH_ACTIVE_ENUMERATION:
                                if (!p_device->p_dongle->active_enumeration_finished) {
                                    logitacker_enter_mode_active_enum(addr);
                                } else {
                                    NRF_LOG_INFO("active enumeration for device already performed, continue deiscovery")
                                }

                                break;
                            case OPTION_DISCOVERY_ON_NEW_ADDRESS_SWITCH_PASSIVE_ENUMERATION:
                                logitacker_enter_mode_passive_enum(addr);
                                break;
                            case OPTION_DISCOVERY_ON_NEW_ADDRESS_SWITCH_AUTO_INJECTION:
                                if (p_device->executed_auto_inject_count < g_logitacker_global_config.max_auto_injects_per_device) {
                                    p_device->executed_auto_inject_count++;
                                    logitacker_enter_mode_injection(addr);
                                    logitacker_injection_start_execution(true);
                                } else {
                                    NRF_LOG_INFO("maximum number of autoinjects reached for this device, continue discover mode")
                                }
                                //logitacker_script_engine_append_task_type_string(LOGITACKER_AUTO_INJECTION_PAYLOAD);
                                break;
                            default:
                                // do nothing, stay in discovery
                                break;
                        }
                    } else if (p_device->p_dongle->classification == DONGLE_CLASSIFICATION_IS_NOT_LOGITECH) {
                        NRF_LOG_INFO("Discovered device doesn't seem to be Logitech, removing from list again...");
                        NRF_LOG_HEXDUMP_INFO(p_rx_payload->data, p_rx_payload->length);
                        if (p_device != NULL) logitacker_devices_del_device(p_device->rf_address);
                    } else {
                        NRF_LOG_INFO("discovered device not classified, yet. Likely because RX frame was empty ... removing device from list")
                        if (p_device != NULL) logitacker_devices_del_device(p_device->rf_address);
                    }
                }
            }

        } else {
            NRF_LOG_WARNING("invalid promiscuous frame in discover mode, shouldn't happen because of filtering");
        }

    }

}


void processor_discover_init_func(logitacker_processor_t *p_processor) {
    processor_discover_init_func_((logitacker_processor_discover_ctx_t *) p_processor->p_ctx);
}

void processor_discover_deinit_func(logitacker_processor_t *p_processor){
    processor_discover_deinit_func_((logitacker_processor_discover_ctx_t *) p_processor->p_ctx);
}

void processor_discover_esb_handler_func(logitacker_processor_t *p_processor, nrf_esb_evt_t *p_esb_evt){
    processor_discover_esb_handler_func_((logitacker_processor_discover_ctx_t *) p_processor->p_ctx, p_esb_evt);
}

void processor_discover_radio_handler_func(logitacker_processor_t *p_processor, radio_evt_t const *p_event) {
    processor_discover_radio_handler_func_((logitacker_processor_discover_ctx_t *) p_processor->p_ctx, p_event);
}

void processor_discover_bsp_handler_func(logitacker_processor_t *p_processor, bsp_event_t event) {
    processor_discover_bsp_handler_func_((logitacker_processor_discover_ctx_t *) p_processor->p_ctx, event);
}

void processor_discover_init_func_(logitacker_processor_discover_ctx_t *self) {
    radio_disable_rx_timeout_event(); // disable RX timeouts
    radio_stop_channel_hopping(); // disable channel hopping
    nrf_esb_stop_rx(); //stop rx in case running

    bsp_board_leds_off(); //disable all LEDs

    //set radio to promiscuous mode and start RX
    nrf_esb_set_mode(NRF_ESB_MODE_PROMISCOUS); //use promiscuous mode
    nrf_esb_update_channel_frequency_table_unifying_reduced();
    nrf_esb_start_rx(); //start rx
    radio_enable_rx_timeout_event(LOGITACKER_DISCOVERY_STAY_ON_CHANNEL_AFTER_RX_MS); //set RX timeout, the eventhandler starts channel hopping once this timeout is reached

}

void processor_discover_deinit_func_(logitacker_processor_discover_ctx_t *self) {
    radio_disable_rx_timeout_event();
    radio_stop_channel_hopping();
    nrf_esb_stop_rx();

    //*self->p_logitacker_mainstate = LOGITACKER_MODE_IDLE;

    nrf_esb_set_mode(NRF_ESB_MODE_PTX);
}

void processor_discover_esb_handler_func_(logitacker_processor_discover_ctx_t *self, nrf_esb_evt_t *p_esb_event) {
    switch (p_esb_event->evt_id) {
        case NRF_ESB_EVENT_RX_RECEIVED:
            NRF_LOG_DEBUG("ESB EVENT HANDLER discover RX_RECEIVED");
            discovery_process_rx(self);
            break;
        default:
            break;
    }

}

void processor_discover_radio_handler_func_(logitacker_processor_discover_ctx_t *self, radio_evt_t const *p_event) {
    //helper_log_priority("UNIFYING_event_handler");
    switch (p_event->evt_id)
    {
        case RADIO_EVENT_NO_RX_TIMEOUT:
        {
            NRF_LOG_INFO("discover: no RX on current channel for %d ms ... restart channel hopping ...", LOGITACKER_DISCOVERY_STAY_ON_CHANNEL_AFTER_RX_MS);
            radio_start_channel_hopping(LOGITACKER_DISCOVERY_CHANNEL_HOP_INTERVAL_MS, 0, true); //start channel hopping directly (0ms delay) with 30ms hop interval, automatically stop hopping on RX
            break;
        }
        case RADIO_EVENT_CHANNEL_CHANGED_FIRST_INDEX:
        {
            NRF_LOG_DEBUG("discover MODE channel hop reached first channel");
            bsp_board_led_invert(LED_B); // toggle scan LED everytime we jumped through all channels
            break;
        }
        default:
            break;
    }

}

void processor_discover_bsp_handler_func_(logitacker_processor_discover_ctx_t *self, bsp_event_t event) {
    NRF_LOG_INFO("Discovery BSP event: %d", (unsigned int) event);
}


logitacker_processor_t * contruct_processor_discover_instance(logitacker_processor_discover_ctx_t *const discover_ctx) {
    m_processor.p_ctx = discover_ctx;

    m_processor.p_init_func = processor_discover_init_func;
    m_processor.p_deinit_func = processor_discover_deinit_func;
    m_processor.p_esb_handler = processor_discover_esb_handler_func;
//    m_processor.p_timer_handler = processor_active_enum_timer_handler_func;
    m_processor.p_bsp_handler = processor_discover_bsp_handler_func;
    m_processor.p_radio_handler = processor_discover_radio_handler_func;

    return &m_processor;
}


logitacker_processor_t * new_processor_discover() {
    // initialize context (static in this case, has to use malloc for new instances)
    logitacker_processor_discover_ctx_t *const p_ctx = &m_static_discover_ctx;
    memset(p_ctx, 0, sizeof(*p_ctx)); //replace with malloc for dedicated instance



    return contruct_processor_discover_instance(&m_static_discover_ctx);
}

