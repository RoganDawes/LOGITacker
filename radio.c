#include <string.h> //memcpy, memset
#include <stddef.h> //NULL

#include "radio.h"
#include "nrf_error.h"
#include "nrf_esb_illegalmod.h"

#include "nrf_log.h"
#include "app_timer.h"

// channel hop timer
APP_TIMER_DEF(m_timer_channel_hop);
// rx timeout timer
APP_TIMER_DEF(m_timer_no_rx_timeout);


radio_evt_t event;



typedef struct
{
    bool initialized;
    nrf_esb_event_handler_t event_handler;

    radio_channel_set_t channel_set;
    uint8_t rf_channel_index;

    radio_event_handler_t radio_event_handler; //handler for unifying events

    uint32_t channel_hop_delay_ms;
    bool channel_hop_enabled;
    bool channel_hop_disable_on_rx;

    bool rx_timeout_enabled;
    uint32_t rx_timeout_delay_ms;
} radio_state_t;


static radio_state_t m_radio_state = {
    .initialized = false,

    .channel_set        = RADIO_DEFAULT_CHANNELS,                               \
    .rf_channel_index   = 0,                                                    \

    .channel_hop_delay_ms = RADIO_DEFAULT_CHANNEL_HOP_INTERVAL_MS, \
    .channel_hop_enabled = false, \
    .channel_hop_disable_on_rx = false \
};

void radio_esb_event_handler(nrf_esb_evt_t * p_event)
{
    switch (p_event->evt_id)
    {
        case NRF_ESB_EVENT_TX_SUCCESS:
        case NRF_ESB_EVENT_TX_FAILED:
            break;
        case NRF_ESB_EVENT_RX_RECEIVED:
            NRF_LOG_DEBUG("RX_RECEIVED");
            if (m_radio_state.channel_hop_enabled && m_radio_state.channel_hop_disable_on_rx) radio_stop_channel_hopping();
            
            //stop rx timeout watcher and restart
            if (m_radio_state.rx_timeout_enabled) {
                NRF_LOG_DEBUG("Restart RX timeout timer with %d ms", m_radio_state.rx_timeout_delay_ms);
                radio_enable_rx_timeout_event(m_radio_state.rx_timeout_delay_ms);
            }
            
            break;
        default:
            break;
    }
    
    if (m_radio_state.event_handler != NULL) m_radio_state.event_handler(p_event);
}


void timer_channel_hop_event_handler(void* p_context)
{
    if (m_radio_state.channel_hop_enabled) {
        
        //radioNextRfChannel(); // hop to next channel
        nrf_esb_set_rf_channel_next();


        if (m_radio_state.radio_event_handler != NULL) {
            uint32_t currentChIdx;
            //radioGetRfChannelIndex(&currentChIdx);
            nrf_esb_get_rf_channel(&currentChIdx);

            // send event
            event.evt_id = RADIO_EVENT_CHANNEL_CHANGED;
            event.channel_index = currentChIdx; // encode channel index in pipe num
            m_radio_state.radio_event_handler(&event);

            if (currentChIdx == 0) {
                event.evt_id = RADIO_EVENT_CHANNEL_CHANGED_FIRST_INDEX;
                m_radio_state.radio_event_handler(&event);
            }

        } 

        uint32_t err_code = app_timer_start(m_timer_channel_hop, APP_TIMER_TICKS(m_radio_state.channel_hop_delay_ms), NULL); //restart timer
        APP_ERROR_CHECK(err_code);
    }   
}

void timer_no_rx_event_handler(void* p_context)
{
    uint32_t currentChIdx;
    //radioGetRfChannelIndex(&currentChIdx);
    nrf_esb_get_rf_channel(&currentChIdx);

    // send event
    event.evt_id = RADIO_EVENT_NO_RX_TIMEOUT;
    event.channel_index = currentChIdx; // encode channel index in pipe num
    m_radio_state.radio_event_handler(&event);
}

uint32_t radioInit(nrf_esb_event_handler_t event_handler, radio_event_handler_t radio_event_handler) {
    app_timer_create(&m_timer_channel_hop, APP_TIMER_MODE_SINGLE_SHOT, timer_channel_hop_event_handler);
    app_timer_create(&m_timer_no_rx_timeout, APP_TIMER_MODE_SINGLE_SHOT, timer_no_rx_event_handler);

    m_radio_state.event_handler            = event_handler;
    m_radio_state.radio_event_handler      = radio_event_handler;


    m_radio_state.channel_hop_delay_ms = RADIO_DEFAULT_CHANNEL_HOP_INTERVAL_MS;
    m_radio_state.channel_hop_enabled = false;

    m_radio_state.initialized = true;

    

    nrf_esb_config_t esb_config = NRF_ESB_PROMISCUOUS_CONFIG;
    esb_config.event_handler = radio_esb_event_handler; // pass custom event handler with call through
    uint32_t err_code = nrf_esb_init(&esb_config);

    nrf_esb_update_channel_frequency_table_unifying();

    VERIFY_SUCCESS(err_code);

    //nrf_esb_init_promiscuous_mode();

    return NRF_SUCCESS;
}

uint32_t radio_start_channel_hopping(uint32_t interval, uint32_t start_delay_ms, bool disable_on_rx) {
    if (m_radio_state.channel_hop_enabled) return NRF_SUCCESS;
    m_radio_state.channel_hop_enabled = true;
    if (start_delay_ms == 0) start_delay_ms++;
    m_radio_state.channel_hop_disable_on_rx = disable_on_rx;
    app_timer_start(m_timer_channel_hop, APP_TIMER_TICKS(start_delay_ms), m_timer_channel_hop);
    NRF_LOG_INFO("Channel hopping started");

    return NRF_SUCCESS;
}

uint32_t radio_stop_channel_hopping() {
    NRF_LOG_INFO("Channel hopping stopped");
    m_radio_state.channel_hop_enabled = false;
    app_timer_stop(m_timer_channel_hop);
    return NRF_SUCCESS;
}

uint32_t radio_enable_rx_timeout_event(uint32_t timeout_ms) {
    m_radio_state.rx_timeout_delay_ms = timeout_ms;
    if (m_radio_state.rx_timeout_enabled) app_timer_stop(m_timer_no_rx_timeout); //stop timer before restart
    m_radio_state.rx_timeout_enabled = true;
    app_timer_start(m_timer_no_rx_timeout, APP_TIMER_TICKS(timeout_ms), m_timer_no_rx_timeout);

    NRF_LOG_DEBUG("RX timeout schedule in %d ms", timeout_ms);
    return NRF_SUCCESS;
}

uint32_t radio_disable_rx_timeout_event() {
    m_radio_state.rx_timeout_enabled = false;
    app_timer_stop(m_timer_no_rx_timeout);

    NRF_LOG_DEBUG("RX timeout disabled");
    return NRF_SUCCESS;

}
