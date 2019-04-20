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


#define NRF_LOG_MODULE_NAME LOGITACKER
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

APP_TIMER_DEF(m_timer_next_tx_action);

typedef enum {
    LOGITACKER_SUBEVENT_TIMER,
    LOGITACKER_SUBEVENT_ESB_TX_SUCCESS,
    LOGITACKER_SUBEVENT_ESB_TX_FAILED,
    LOGITACKER_SUBEVENT_ESB_RX_RECEIVED,
    LOGITACKER_SUBEVENT_ESB_TX_SUCCESS_ACK_PAY,
} logitacker_subevent_t;


typedef enum {
    LOGITACKER_DEVICE_DISCOVERY,   // radio in promiscuous mode, logs devices
    LOGITACKER_DEVICE_ACTIVE_ENUMERATION,   // radio in PTX mode, actively collecting dongle info
    LOGITACKER_DEVICE_PASSIVE_ENUMERATION   // radio in SNIFF mode, collecting device frames to determin caps
} logitacker_mainstate_t;


typedef struct {
    logitacker_discovery_on_new_address_t on_new_address_action; //not only state, persistent config
} logitacker_substate_discovery_t;

typedef enum {
    LOGITACKER_ACTIVE_ENUM_PHASE_STARTED,   // try to reach dongle RF address for device
    LOGITACKER_ACTIVE_ENUM_PHASE_FINISHED,   // try to reach dongle RF address for device
    LOGITACKER_ACTIVE_ENUM_PHASE_PING_DONGLE,   // try to reach dongle RF address for device
    LOGITACKER_ACTIVE_ENUM_PHASE_DISCOVER_NEIGHBOURS,   // !! should be own substate !!
    LOGITACKER_ACTIVE_ENUM_PHASE_TEST_PLAIN_KEYSTROKE_INJECTION   // tests if plain keystrokes could be injected (press CAPS, listen for LED reports)
} logitacker_active_enumeration_phase_t;

typedef struct {
    uint8_t base_addr[4];
    uint8_t known_prefix;
    uint8_t next_prefix;
    uint8_t prefixes_under_test[8];
    uint8_t current_channel_index;

    uint8_t tx_delay_ms;

    bool receiver_in_range;
    logitacker_active_enumeration_phase_t phase;
} logitacker_substate_active_enumeration_t;

typedef struct {
    uint8_t base_addr[4];
    uint8_t known_prefix;
    uint8_t current_channel_index;

    logitacker_device_t devices[NRF_ESB_PIPE_COUNT];
} logitacker_substate_passive_enumeration_t;


typedef struct {
    bool initialized;
    logitacker_mainstate_t   mainstate;
    logitacker_substate_discovery_t substate_discovery;
    logitacker_substate_passive_enumeration_t substate_passive_enumeration;
    logitacker_substate_active_enumeration_t substate_active_enumeration;

    bsp_event_callback_t current_bsp_event_handler;
    nrf_esb_event_handler_t current_esb_event_handler;
    radio_event_handler_t current_radio_event_handler;
    app_timer_timeout_handler_t current_timer_event_handler;
} logitacker_state_t;


logitacker_state_t m_state_local;

static uint8_t key_report_caps[] = { 0x00, 0xC1, 0x00, 0x39, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static uint8_t key_report_release[] = { 0x00, 0xC1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

static char addr_str_buff[LOGITACKER_DEVICE_ADDR_STR_LEN] = {0};
static nrf_esb_payload_t tmp_payload = {0};
static nrf_esb_payload_t tmp_tx_payload = {0};


void timer_next_tx_action_handler_main(void* p_context) {
    if (m_state_local.current_timer_event_handler != NULL) m_state_local.current_timer_event_handler(p_context);
}


void esb_event_handler_main(nrf_esb_evt_t * p_event) {
    switch (p_event->evt_id)
    {
        case NRF_ESB_EVENT_TX_SUCCESS:
            NRF_LOG_DEBUG("ESB EVENT HANDLER MAIN TX_SUCCESS");
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
    
    if (m_state_local.current_esb_event_handler != 0) m_state_local.current_esb_event_handler(p_event);
}


void radio_process_rx_discovery_mode() {
    static nrf_esb_payload_t rx_payload;
    static nrf_esb_payload_t * p_rx_payload = &rx_payload;
    while (nrf_esb_read_rx_payload(p_rx_payload) == NRF_SUCCESS) {
        if (p_rx_payload->validated_promiscuous_frame) {
            uint8_t len = p_rx_payload->length;
            uint8_t ch_idx = p_rx_payload->rx_channel_index;
            uint8_t ch = p_rx_payload->rx_channel;
            uint8_t addr[5];
            memcpy(addr, &p_rx_payload->data[2], 5);

            helper_addr_to_hex_str(addr_str_buff, LOGITACKER_DEVICE_ADDR_LEN, addr);
            NRF_LOG_INFO("DISCOVERY: received valid ESB frame (addr %s, len: %d, ch idx %d, raw ch %d)", addr_str_buff, len, ch_idx, ch);
           
            NRF_LOG_HEXDUMP_DEBUG(p_rx_payload->data, p_rx_payload->length);

            // retrieve device entry if existent
            logitacker_device_t *p_device = logitacker_device_list_get_by_addr(addr);
            if (p_device == NULL) {
                // device doesn't exist, add to list
                logitacker_device_t new_device = {0};
                helper_addr_to_base_and_prefix(new_device.base_addr, &new_device.addr_prefix, addr, 5); //convert device addr to base+prefix and update device
                p_device = logitacker_device_list_add(new_device); // add device to device list
                if (p_device != NULL) {
                    NRF_LOG_INFO("DISCOVERY: added new device entry for %s", addr_str_buff);
                } else {
                    NRF_LOG_WARNING("DISCOVERY: failed to add new device entry for %s! device list full ?", addr_str_buff);
                }
            } else {
                // existing device
                NRF_LOG_INFO("DISCOVERY: device %s already known", addr_str_buff);
            }

            // update device counters
            if (p_device != NULL) {
                logitacker_radio_convert_promiscuous_frame_to_default_frame(&tmp_payload, rx_payload);
                logitacker_device_update_counters_from_frame(p_device, tmp_payload);
            }

            switch (m_state_local.substate_discovery.on_new_address_action) {
                case LOGITACKER_DISCOVERY_ON_NEW_ADDRESS_DO_NOTHING:
                    break;
                case LOGITACKER_DISCOVERY_ON_NEW_ADDRESS_SWITCH_ACTIVE_ENUMERATION:
                    logitacker_enter_state_active_enumeration(addr);
                    break;
                case LOGITACKER_DISCOVERY_ON_NEW_ADDRESS_SWITCH_PASSIVE_ENUMERATION:
                    logitacker_enter_state_passive_enumeration(addr);
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

void esb_event_handler_discovery(nrf_esb_evt_t * p_event) {
    switch (p_event->evt_id) {
        case NRF_ESB_EVENT_RX_RECEIVED:
            NRF_LOG_DEBUG("ESB EVENT HANDLER DISCOVERY RX_RECEIVED");
            radio_process_rx_discovery_mode(); //replace by processing of frames
            break;
        default:
            break;
    }
    
}

void radio_process_rx_passive_enum_mode() {
    static nrf_esb_payload_t rx_payload;
    static nrf_esb_payload_t * p_rx_payload = &rx_payload;
    while (nrf_esb_read_rx_payload(p_rx_payload) == NRF_SUCCESS) {
            uint8_t len = p_rx_payload->length;
            uint8_t unifying_report_type;
            bool unifying_is_keep_alive;
            unifying_frame_classify(rx_payload, &unifying_report_type, &unifying_is_keep_alive);

            logitacker_device_t *p_device = &m_state_local.substate_passive_enumeration.devices[rx_payload.pipe]; //pointer to correct device meta data

            logitacker_device_update_counters_from_frame(p_device, rx_payload); //update device data

            if (!(unifying_report_type == 0x00 && unifying_is_keep_alive) && len != 0) { //ignore keep-alive only frames and empty frames
                uint8_t ch_idx = p_rx_payload->rx_channel_index;
                uint8_t ch = p_rx_payload->rx_channel;
                uint8_t addr[5];
                nrf_esb_convert_pipe_to_address(p_rx_payload->pipe, addr);
                helper_addr_to_hex_str(addr_str_buff, LOGITACKER_DEVICE_ADDR_LEN, addr);
                NRF_LOG_INFO("frame RX in passive enumeration mode (addr %s, len: %d, ch idx %d, raw ch %d)", addr_str_buff, len, ch_idx, ch);
                unifying_frame_classify_log(rx_payload);
                NRF_LOG_HEXDUMP_INFO(p_rx_payload->data, p_rx_payload->length);
            }
    }

}

void esb_event_handler_passive_enum(nrf_esb_evt_t * p_event) {
    switch (p_event->evt_id) {
        case NRF_ESB_EVENT_RX_RECEIVED:
            NRF_LOG_DEBUG("ESB EVENT HANDLER PASSIVE ENUMERATION RX_RECEIVED");
            radio_process_rx_passive_enum_mode();
            break;
        default:
            break;
    }
    
}

#define ACTIVE_ENUM_INNER_LOOP_MAX 4 //how many CAPS presses / key releases get send
uint8_t m_active_enum_inner_loop_count = 0;
bool m_active_enum_key_press = false; // indicates if next active enum TX payload is a key down (CAPS) or key release
uint8_t m_active_enum_led_count = 0;
static uint8_t m_active_enum_current_address[5];
static logitacker_device_t m_active_enum_tmp_device;

// increments pipe 1 address prefix (neighbour discovery)
bool active_enumeration_set_next_prefix(logitacker_substate_active_enumeration_t * p_state) {
    // all possible prefixes (neighbours) tested? 
    if (p_state->next_prefix == p_state->known_prefix) {
        p_state->phase = LOGITACKER_ACTIVE_ENUM_PHASE_FINISHED;
        NRF_LOG_INFO("Tested all possible neighbours");
        return true;
    }

    // update temp device
    m_active_enum_tmp_device.addr_prefix = p_state->next_prefix;

    nrf_esb_enable_pipes(0x00); //disable all pipes
    nrf_esb_update_prefix(1, p_state->next_prefix); // set prefix and enable pipe 1
    m_active_enum_current_address[4] = p_state->next_prefix;
    helper_addr_to_hex_str(addr_str_buff, LOGITACKER_DEVICE_ADDR_LEN, m_active_enum_current_address);
    //NRF_LOG_INFO("Testing next device address prefix %.2x", p_state->next_prefix);
    NRF_LOG_INFO("Test next neighbour address %s", addr_str_buff);
    p_state->next_prefix++;
    m_active_enum_inner_loop_count = 0;
    m_active_enum_led_count = 0;


    return false;
}

void active_enumeration_subevent_process(logitacker_subevent_t se_type, void *p_subevent) {
    logitacker_substate_active_enumeration_t * p_state = &m_state_local.substate_active_enumeration;

    if (p_state->phase == LOGITACKER_ACTIVE_ENUM_PHASE_FINISHED) {
        NRF_LOG_WARNING("Active enumeration event, while active enumeration finished");
        return;
    }


    uint32_t channel_freq;
    nrf_esb_get_rf_frequency(&channel_freq);

    switch (se_type) {
        case LOGITACKER_SUBEVENT_TIMER:
        {           
            // setup radio as PTX
            m_state_local.substate_active_enumeration.phase = LOGITACKER_ACTIVE_ENUM_PHASE_TEST_PLAIN_KEYSTROKE_INJECTION;

            // write payload (autostart TX is enabled for PTX mode)
            if (nrf_esb_write_payload(&tmp_tx_payload) != NRF_SUCCESS) {
                NRF_LOG_INFO("Error writing payload");
            }

            break;
        }
        case LOGITACKER_SUBEVENT_ESB_TX_FAILED:
        {
            NRF_LOG_DEBUG("ACTIVE ENUMERATION TX_FAILED channel: %d", channel_freq);
            if (p_state->phase == LOGITACKER_ACTIVE_ENUM_PHASE_STARTED) {
                p_state->receiver_in_range = false;
                p_state->phase = LOGITACKER_ACTIVE_ENUM_PHASE_FINISHED;
                NRF_LOG_INFO("Failed to reach receiver in first transmission, aborting active enumeration");
                return;
            }
         
            // continue with next prefix, return if first prefixe has been reached again
            if (active_enumeration_set_next_prefix(p_state)) return;

            //re-transmit last frame (payload still enqued)
            nrf_esb_start_tx();
            

            break;
        }
        case LOGITACKER_SUBEVENT_ESB_TX_SUCCESS_ACK_PAY:
        {
            NRF_LOG_INFO("ACTIVE ENUMERATION TX_SUCCESS_ACK_PAY channel: %d", channel_freq);
            while (nrf_esb_read_rx_payload(&tmp_payload) == NRF_SUCCESS) {
                NRF_LOG_HEXDUMP_INFO(tmp_payload.data, tmp_payload.length);


                //Note: LED testing doesn't work on presenters like "R400", because no HID led ouput reports are sent
                // test if LED report
                if ((tmp_payload.data[1] & 0x1f) == 0x0e) {
                    m_active_enum_led_count++;

                    //device supports plain injection
                    NRF_LOG_INFO("LED test succeeded .. devices accepts plain injection");
                }
                //
            }
        }
        case LOGITACKER_SUBEVENT_ESB_TX_SUCCESS:
        {
            NRF_LOG_INFO("ACTIVE ENUMERATION TX_SUCCESS channel: %d", channel_freq);

            
            // hit, try to add the device if not dongle address (prefix 0x00)
            if (m_active_enum_tmp_device.addr_prefix != 0x00) {
                if (logitacker_device_list_get_by_base_prefix(m_active_enum_tmp_device.base_addr, m_active_enum_tmp_device.addr_prefix) == NULL) {
                    if (logitacker_device_list_add(m_active_enum_tmp_device) != NULL) {
                        // new device
                        NRF_LOG_INFO("active enum: address %s accepted by dongle, added as new device", addr_str_buff);
                    } else {
                        NRF_LOG_WARNING("active enum: address %s accepted by dongle, but device couldn't be added ... list full?!" , addr_str_buff);
                    }
                } else {
                    NRF_LOG_DEBUG("active enum: device already known: %s", addr_str_buff);    
                }
            }

            if (m_active_enum_key_press) {
                memcpy (tmp_tx_payload.data, key_report_caps, sizeof(key_report_caps));
            } else {
                memcpy (tmp_tx_payload.data, key_report_release, sizeof(key_report_release));
            }

            if (m_active_enum_inner_loop_count < ACTIVE_ENUM_INNER_LOOP_MAX) {
                // schedule next transmission
                //NRF_LOG_INFO("start active enum timer");
                app_timer_start(m_timer_next_tx_action, APP_TIMER_TICKS(8), p_subevent);
                m_active_enum_key_press = !m_active_enum_key_press;
                m_active_enum_inner_loop_count++;
            } else {
                // we are done with this device
                            
                // continue with next prefix, return if first prefixe has been reached again
                if (active_enumeration_set_next_prefix(p_state)) return;

                // schedule next transmission
                app_timer_start(m_timer_next_tx_action, APP_TIMER_TICKS(1), p_subevent);
            }
            break;
        }
        case LOGITACKER_SUBEVENT_ESB_RX_RECEIVED:
        {
            NRF_LOG_INFO("ESB EVENT HANDLER ACTIVE ENUMERATION RX_RECEIVED ... !!shouldn't happen!!");
            break;
        }
    }
}

// Transfers execution to active_enumeration_subevent_process
void esb_event_handler_active_enum(nrf_esb_evt_t * p_event) {
    
    switch (p_event->evt_id) {
        case NRF_ESB_EVENT_TX_SUCCESS:
            active_enumeration_subevent_process(LOGITACKER_SUBEVENT_ESB_TX_SUCCESS, p_event);
            break;
        case NRF_ESB_EVENT_TX_SUCCESS_ACK_PAY:
            active_enumeration_subevent_process(LOGITACKER_SUBEVENT_ESB_TX_SUCCESS_ACK_PAY, p_event);
            break;
        case NRF_ESB_EVENT_TX_FAILED:
            active_enumeration_subevent_process(LOGITACKER_SUBEVENT_ESB_TX_FAILED, p_event);
            break;
        case NRF_ESB_EVENT_RX_RECEIVED:
            active_enumeration_subevent_process(LOGITACKER_SUBEVENT_ESB_RX_RECEIVED, p_event);
            break;
        default:
            break;
    }
}

// Transfers execution to active_enumeration_subevent_process
void timer_next_tx_action_handler_active_enum(void* p_context) {
    active_enumeration_subevent_process(LOGITACKER_SUBEVENT_TIMER, p_context);
}

void radio_event_handler_discovery(radio_evt_t const *p_event) {
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


void radio_event_handler_passive_enum(radio_evt_t const *p_event) {
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

void radio_event_handler_main(radio_evt_t const *p_event) {
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

    if (m_state_local.current_radio_event_handler != 0) m_state_local.current_radio_event_handler(p_event);
}

static bool long_pushed;
static void bsp_event_handler_main(bsp_event_t ev)
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
            long_pushed = true;
            NRF_LOG_INFO("ACTION BUTTON PUSHED LONG");          
            break;

        case BSP_USER_EVENT_RELEASE_0:
            if (long_pushed) {
                long_pushed = false;
                break; // don't act if there was already a long press event
            }
            NRF_LOG_INFO("ACTION BUTTON RELEASED");
            break;

        default:
            break; // no implementation needed
    }

    if (m_state_local.current_bsp_event_handler != 0) m_state_local.current_bsp_event_handler(ev);
}

static void bsp_event_handler_discovery(bsp_event_t ev) {
    NRF_LOG_INFO("Discovery BSP event: %d", (unsigned int)ev);
}

static void bsp_event_handler_passive_enumeration(bsp_event_t ev) {
    NRF_LOG_INFO("Passive enumeration BSP event: %d", (unsigned int)ev);
    switch ((unsigned int)ev)
    {
        case BSP_USER_EVENT_RELEASE_0:
            NRF_LOG_INFO("ACTION BUTTON RELEASED in passive enumeration mode, changing back to discovery...");
            logitacker_enter_state_discovery();
            break;

        default:
            break; // no implementation needed
    }

}

static void bsp_event_handler_active_enumeration(bsp_event_t ev) {
    NRF_LOG_INFO("active enumeration BSP event: %d", (unsigned int)ev);
    switch ((unsigned int)ev)
    {
        case BSP_USER_EVENT_RELEASE_0:
            NRF_LOG_INFO("ACTION BUTTON RELEASED in active enumeration mode, retransmit frame...");
            nrf_esb_flush_tx();
            nrf_esb_write_payload(&tmp_tx_payload);
            break;

        default:
            break; // no implementation needed
    }

}


void logitacker_enter_state_discovery() {
    NRF_LOG_INFO("Entering discovery mode");

    nrf_esb_stop_rx(); //stop rx in case running
    radio_disable_rx_timeout_event(); // disable RX timeouts
    radio_stop_channel_hopping(); // disable channel hopping

    m_state_local.mainstate = LOGITACKER_DEVICE_DISCOVERY;

    m_state_local.current_bsp_event_handler = bsp_event_handler_discovery;
    m_state_local.current_esb_event_handler = esb_event_handler_discovery; // process RX_RECEIVED
    m_state_local.current_radio_event_handler = radio_event_handler_discovery; //restarts channel hopping on rx timeout
    m_state_local.current_timer_event_handler = NULL;

    bsp_board_leds_off(); //disable all LEDs

    //set radio to promiscuous mode and start RX
    nrf_esb_set_mode(NRF_ESB_MODE_PROMISCOUS); //use promiscuous mode
    nrf_esb_start_rx(); //start rx
    radio_enable_rx_timeout_event(LOGITACKER_DISCOVERY_STAY_ON_CHANNEL_AFTER_RX_MS); //set RX timeout, the eventhandler starts channel hopping once this timeout is reached    
}

void logitacker_enter_state_passive_enumeration(uint8_t * rf_address) {
    NRF_LOG_INFO("Entering passive enumeration mode");

    
    nrf_esb_stop_rx(); //stop rx in case running
    radio_disable_rx_timeout_event(); // disable RX timeouts
    radio_stop_channel_hopping(); // disable channel hopping
    
    m_state_local.mainstate = LOGITACKER_DEVICE_PASSIVE_ENUMERATION;
    m_state_local.current_bsp_event_handler = bsp_event_handler_passive_enumeration;
    m_state_local.current_radio_event_handler = radio_event_handler_passive_enum;
    m_state_local.current_esb_event_handler = esb_event_handler_passive_enum;    
    m_state_local.current_timer_event_handler = NULL;

    //reset device state
    memset(m_state_local.substate_passive_enumeration.devices, 0, sizeof(m_state_local.substate_passive_enumeration.devices));

    uint8_t base_addr[4] = { 0 };
    uint8_t prefix = 0;
    helper_addr_to_base_and_prefix(base_addr, &prefix, rf_address, 5);
    memcpy(m_state_local.substate_passive_enumeration.base_addr, base_addr, 4);
    m_state_local.substate_passive_enumeration.known_prefix = prefix;
    

    bsp_board_leds_off(); //disable all LEDs


    
    nrf_esb_set_mode(NRF_ESB_MODE_SNIFF);
    nrf_esb_set_base_address_1(m_state_local.substate_passive_enumeration.base_addr); // set base addr1
    nrf_esb_update_prefix(1, 0x00); // set suffix 0x00 for pipe 1 (dongle address)
    nrf_esb_update_prefix(2, m_state_local.substate_passive_enumeration.known_prefix); // set known suffix for pipe 2
    //set neighbouring prefixes
    nrf_esb_update_prefix(3, m_state_local.substate_passive_enumeration.known_prefix-1); // set known suffix for pipe 2
    nrf_esb_update_prefix(4, m_state_local.substate_passive_enumeration.known_prefix-2); // set known suffix for pipe 2
    nrf_esb_update_prefix(6, m_state_local.substate_passive_enumeration.known_prefix+1); // set known suffix for pipe 2
    nrf_esb_update_prefix(7, m_state_local.substate_passive_enumeration.known_prefix+2); // set known suffix for pipe 2
    nrf_esb_update_prefix(8, m_state_local.substate_passive_enumeration.known_prefix+3); // set known suffix for pipe 2

    // add global pairing address to pipe 0
    helper_addr_to_base_and_prefix(base_addr, &prefix, UNIFYING_GLOBAL_PAIRING_ADDRESS, 5);
    nrf_esb_set_base_address_0(base_addr);
    nrf_esb_update_prefix(0, prefix);

    while (nrf_esb_start_rx() != NRF_SUCCESS) {};

    radio_enable_rx_timeout_event(LOGITACKER_PASSIVE_ENUM_STAY_ON_CHANNEL_AFTER_RX_MS); //set RX timeout, the eventhandler starts channel hopping once this timeout is reached    
}



void logitacker_enter_state_active_enumeration(uint8_t * rf_address) {
    //clear state
    memset(&m_state_local.substate_active_enumeration, 0, sizeof(m_state_local.substate_active_enumeration));
    
    m_state_local.mainstate = LOGITACKER_DEVICE_ACTIVE_ENUMERATION;
    m_state_local.current_bsp_event_handler = bsp_event_handler_active_enumeration;
    m_state_local.current_radio_event_handler = NULL;
    m_state_local.current_esb_event_handler = esb_event_handler_active_enum;    
    m_state_local.current_timer_event_handler = timer_next_tx_action_handler_active_enum;
    m_state_local.substate_active_enumeration.tx_delay_ms = 8;

    helper_addr_to_base_and_prefix(m_state_local.substate_active_enumeration.base_addr, &m_state_local.substate_active_enumeration.known_prefix, rf_address, LOGITACKER_DEVICE_ADDR_LEN);
    memcpy(m_active_enum_current_address, rf_address, 5);
    m_state_local.substate_active_enumeration.next_prefix = m_state_local.substate_active_enumeration.known_prefix + 1;

    /*
    uint8_t base_addr[4] = { rf_address[3], rf_address[2], rf_address[1], rf_address[0] };
    memcpy(m_state_local.substate_active_enumeration.base_addr, base_addr, 4);
    m_state_local.substate_active_enumeration.known_prefix = rf_address[4];
    */

    nrf_esb_stop_rx(); //stop rx in case running
    radio_disable_rx_timeout_event(); // disable RX timeouts
    radio_stop_channel_hopping(); // disable channel hopping

    //update temporary device
    memcpy(m_active_enum_tmp_device.base_addr, m_state_local.substate_active_enumeration.base_addr, 4);
    m_active_enum_tmp_device.addr_prefix = m_state_local.substate_active_enumeration.known_prefix;

    // set current address for pipe 1
    nrf_esb_enable_pipes(0x00); //disable all pipes
    nrf_esb_set_base_address_1(m_state_local.substate_active_enumeration.base_addr); // set base addr1
    nrf_esb_update_prefix(1, m_state_local.substate_active_enumeration.known_prefix); // set prefix and enable pipe 1


    // prepare test TX payload
    tmp_tx_payload.length = sizeof(key_report_release);
    memcpy(tmp_tx_payload.data, key_report_release, sizeof(key_report_release)); //note: key reports have keep-alive bit enabled
    tmp_tx_payload.pipe = 1;
    tmp_tx_payload.noack = false; // we need an ack
    
    // setup radio as PTX
    nrf_esb_set_mode(NRF_ESB_MODE_PTX);
    nrf_esb_enable_all_channel_tx_failover(true); //retransmit payloads on all channels if transmission fails
    nrf_esb_set_all_channel_tx_failover_loop_count(2); //iterate over channels two time before failing

    m_state_local.substate_active_enumeration.phase = LOGITACKER_ACTIVE_ENUM_PHASE_STARTED;

    helper_addr_to_hex_str(addr_str_buff, LOGITACKER_DEVICE_ADDR_LEN, m_active_enum_current_address);
    //NRF_LOG_INFO("Testing next device address prefix %.2x", p_state->next_prefix);
    NRF_LOG_INFO("Start active enumeration for address %s", addr_str_buff);


    // write payload (autostart TX is enabled for PTX mode)
    nrf_esb_write_payload(&tmp_tx_payload);
}

uint32_t logitacker_init() {
    app_timer_create(&m_timer_next_tx_action, APP_TIMER_MODE_SINGLE_SHOT, timer_next_tx_action_handler_main);

    //update checksums for logitech payloads
    unifying_payload_update_checksum(key_report_caps, 10);
    unifying_payload_update_checksum(key_report_release, 10);

    m_state_local.substate_discovery.on_new_address_action = LOGITACKER_DISCOVERY_ON_NEW_ADDRESS_DO_NOTHING;
    logitacker_bsp_init(bsp_event_handler_main);
    logitacker_radio_init(esb_event_handler_main, radio_event_handler_main);
    logitacker_enter_state_discovery();


    return NRF_SUCCESS;
}

void logitacker_discover_on_new_address_action(logitacker_discovery_on_new_address_t on_new_address_action) {
    m_state_local.substate_discovery.on_new_address_action = on_new_address_action;
}




