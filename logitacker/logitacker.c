#include "logitacker.h"
#include "logitacker_bsp.h"
#include "logitacker_radio.h"
#include "nrf.h"
#include "nrf_esb_illegalmod.h"
#include "bsp.h"

#define NRF_LOG_MODULE_NAME LOGITACKER
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();



typedef enum {
    LOGITACKER_DEVICE_DISCOVERY,   // radio in promiscuous mode, logs devices
    LOGITACKER_DEVICE_ACTIVE_ENUMERATION,   // radio in PTX mode, actively collecting dongle info
    LOGITACKER_DEVICE_PASSIVE_ENUMERATION   // radio in SNIFF mode, collecting device frames to determin caps
} logitacker_mainstate_t;

typedef enum {
    LOGITACKER_DISCOVERY_ON_NEW_ADDRESS_DO_NOTHING,   // continues in discovery mode, when new address has been found
    LOGITACKER_DISCOVERY_ON_NEW_ADDRESS_SWITCH_PASSIVE_ENUMERATION,   // continues in passive enumeration mode when address found
    LOGITACKER_DISCOVERY_ON_NEW_ADDRESS_SWITCH_ACTIVE_ENUMERATION   // continues in active enumeration mode when address found
} logitacker_discovery_on_new_address_t;

typedef struct {
    logitacker_discovery_on_new_address_t on_new_address_action;
} logitacker_substate_discovery_t;


typedef struct {
    bool initialized;
    logitacker_mainstate_t   mainstate;
    logitacker_substate_discovery_t substate_discovery;

    bsp_event_callback_t current_bsp_event_handler;
    nrf_esb_event_handler_t current_esb_event_handler;
    radio_event_handler_t current_radio_event_handler;
} logitacker_state_t;

logitacker_state_t m_state_local;

void esb_event_handler_main(nrf_esb_evt_t * p_event)
{
    switch (p_event->evt_id)
    {
        case NRF_ESB_EVENT_TX_SUCCESS:
            NRF_LOG_INFO("ESB EVENT HANDLER MAIN TX_SUCCESS");
            break;
        case NRF_ESB_EVENT_TX_FAILED:
            NRF_LOG_INFO("ESB EVENT HANDLER MAIN TX_FAILED");
            break;
        case NRF_ESB_EVENT_RX_RECEIVED:
            NRF_LOG_DEBUG("ESB EVENT HANDLER MAIN RX_RECEIVED");           
            break;
        default:
            break;
    }
    
    if (m_state_local.current_esb_event_handler != 0) m_state_local.current_esb_event_handler(p_event);
}

static char strbuf[20] = {0};
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

            sprintf(strbuf, "%.2x:%.2x:%.2x:%.2x:%.2x", addr[0], addr[1], addr[2], addr[3], addr[4]);
            strbuf[16] = 0x00;
            NRF_LOG_INFO("received valid ESB frame in discovery mode (addr %s, len: %d, ch idx %d, raw ch %d)", strbuf, len, ch_idx, ch);
           
            NRF_LOG_HEXDUMP_INFO(p_rx_payload->data, p_rx_payload->length);
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
}

static void bsp_event_handler_active_enumeration(bsp_event_t ev) {
    NRF_LOG_INFO("active enumeration BSP event: %d", (unsigned int)ev);
}


void logitacker_enter_state_discovery() {
    m_state_local.mainstate = LOGITACKER_DEVICE_DISCOVERY;
    m_state_local.substate_discovery.on_new_address_action = LOGITACKER_DISCOVERY_ON_NEW_ADDRESS_DO_NOTHING;
    m_state_local.current_bsp_event_handler = bsp_event_handler_discovery;
    m_state_local.current_esb_event_handler = esb_event_handler_discovery; // process RX_RECEIVED
    m_state_local.current_radio_event_handler = radio_event_handler_discovery; //restarts channel hopping on rx timeout

    bsp_board_leds_off(); //disable all LEDs

    //set radio to promiscuous mode and start RX
    nrf_esb_set_mode(NRF_ESB_MODE_PROMISCOUS); //use promiscuous mode
    nrf_esb_start_rx(); //start rx
    radio_enable_rx_timeout_event(LOGITACKER_DISCOVERY_STAY_ON_CHANNEL_AFTER_RX_MS); //set RX timeout, the eventhandler starts channel hopping once this timeout is reached    
}

void logitacker_enter_state_passive_enumeration(uint8_t * rf_address) {
    // ToDo: set RF address

    m_state_local.mainstate = LOGITACKER_DEVICE_PASSIVE_ENUMERATION;
    m_state_local.current_bsp_event_handler = bsp_event_handler_passive_enumeration;
}

void logitacker_enter_state_active_enumeration(uint8_t * rf_address) {
    // ToDo: set RF address

    m_state_local.mainstate = LOGITACKER_DEVICE_ACTIVE_ENUMERATION;
    m_state_local.current_bsp_event_handler = bsp_event_handler_active_enumeration;
}

uint32_t logitacker_init() {
    logitacker_bsp_init(bsp_event_handler_main);
    logitacker_radio_init(esb_event_handler_main, radio_event_handler_main);
    logitacker_enter_state_discovery();
    return NRF_SUCCESS;
}