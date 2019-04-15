#include <string.h> //memcpy, memset
#include <stddef.h> //NULL

#include "radio.h"
#include "crc16.h"
#include "nrf_error.h"
#include "nrf_esb_illegalmod.h"
#include "unifying.h"

#include "sdk_macros.h"
#include "nrf_delay.h"
#include "nrf_log.h"
#include "led_rgb.h"
#include "app_timer.h"
#include "app_scheduler.h"

APP_TIMER_DEF(m_timer_delayed_tx_frame);
APP_TIMER_DEF(m_timer_ping_sweep);

// channel hop timer
APP_TIMER_DEF(m_timer_channel_hop);

APP_TIMER_DEF(m_timer_no_rx_timeout);


 //validates promiscous mode packets based on common length of logitech RF frames and based on 8bit logitech payload checksum 

#define BIT_MASK_UINT_8(x) (0xFF >> (8 - (x)))

radio_evt_t event;



typedef struct
{
    bool initialized;
    radio_rf_mode_t         mode;
    nrf_esb_event_handler_t event_handler;
    uint8_t rf_channel;             /**< Channel to use (must be between 0 and 100). */
    uint8_t base_addr_p0[4];        /**< Base address for pipe 0 encoded in big endian. */
    uint8_t base_addr_p1[4];        /**< Base address for pipe 1-7 encoded in big endian. */
    uint8_t pipe_prefixes[8];       /**< Address prefix for pipe 0 to 7. */
    uint8_t num_pipes;              /**< Number of pipes available. */
    uint8_t addr_length;            /**< Length of the address including the prefix. */
    uint8_t rx_pipes_enabled;       /**< Bitfield for enabled pipes. */    
    radio_channel_set_t channel_set;
    uint8_t rf_channel_index;

    radio_event_handler_t radio_event_handler; //handler for unifying events

    uint32_t channel_hop_delay_ms;
    bool channel_hop_enabled;
    bool channel_hop_disable_on_rx;
    bool channel_hop_rx_after_last_hop;

    bool rx_received_before_timeout;
    bool rx_timeout_enabled;
    uint32_t rx_timeout_delay_ms;


} radio_state_t;

//static nrf_esb_config_t m_local_esb_config = NRF_ESB_DEFAULT_CONFIG;

static radio_state_t m_radio_state = {
    .mode = RADIO_MODE_DISABLED,
    .initialized = false,
    .base_addr_p0 = { 0xE7, 0xE7, 0xE7, 0xE7 },
    .base_addr_p1       = { 0xC2, 0xC2, 0xC2, 0xC2 },                           \
    .pipe_prefixes      = { 0xE7, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8 },   \
    .addr_length        = 5,                                                    \
    .num_pipes          = NRF_ESB_PIPE_COUNT,                                   \
    .rf_channel         = 5,                                                    \
    .rx_pipes_enabled   = 0xFF,                                                 \
    .channel_set        = RADIO_DEFAULT_CHANNELS,                               \
    .rf_channel_index   = 0,                                                    \

    .channel_hop_delay_ms = RADIO_DEFAULT_CHANNEL_HOP_INTERVAL_MS, \
    .channel_hop_enabled = false, \
    .channel_hop_disable_on_rx = false, \
    .channel_hop_rx_after_last_hop = false \



};


static nrf_esb_evt_t *m_last_tx_event;
//void radio_esb_event_handler(nrf_esb_evt_t const * p_event)
void radio_esb_event_handler(nrf_esb_evt_t * p_event)
{
    switch (p_event->evt_id)
    {
        case NRF_ESB_EVENT_TX_SUCCESS:
        case NRF_ESB_EVENT_TX_FAILED:
            CRITICAL_REGION_ENTER();
            m_last_tx_event = (nrf_esb_evt_t*) p_event;
            CRITICAL_REGION_EXIT();
            break;
        case NRF_ESB_EVENT_RX_RECEIVED:
            if (m_radio_state.channel_hop_enabled) m_radio_state.channel_hop_rx_after_last_hop = true;
            
            NRF_LOG_DEBUG("RX_RECEIVED");
            //stop rx timeout watcher and restart
            if (m_radio_state.rx_timeout_enabled) {
                m_radio_state.rx_received_before_timeout = true;
                // restart timer
                //radio_disable_rx_timeout_event();
                NRF_LOG_DEBUG("Restart RX timeout timer with %d ms", m_radio_state.rx_timeout_delay_ms);
                radio_enable_rx_timeout_event(m_radio_state.rx_timeout_delay_ms);
            }
            
            break;
        default:
            break;
    }
    
    if (m_radio_state.event_handler != NULL) m_radio_state.event_handler(p_event);
}

bool radioTransmit(nrf_esb_payload_t *p_tx_payload, bool blockTillResult) {
    uint32_t ret;

    // if mode isn't PTX, switch over to ptx
    radio_rf_mode_t oldMode = m_radio_state.mode;
    if (oldMode == RADIO_MODE_SNIFF) {
        NRF_LOG_DEBUG("Want to TX but mode is PRX_PASSIVE, stop RX ...")
        nrf_esb_stop_rx();
    }

    radioSetMode(RADIO_MODE_PTX);
    ret = nrf_esb_write_payload(p_tx_payload);
    if(ret != NRF_SUCCESS) {
        NRF_LOG_WARNING("Error write payload: %d", ret);
        if (oldMode == RADIO_MODE_SNIFF) {
            NRF_LOG_DEBUG("switching back to passive PRX after TX...")
            radioSetMode(oldMode);
            nrf_esb_start_rx();
        }
        return false;
    }

    if (!blockTillResult) {
        if (oldMode == RADIO_MODE_SNIFF) {
            NRF_LOG_DEBUG("switching back to passive PRX after TX...")
            radioSetMode(oldMode);
            nrf_esb_start_rx();
        }
        return true;
    }

    bool tx_done = false;
    bool tx_success = false;
    while (!tx_done) {
        CRITICAL_REGION_ENTER();
        if (m_last_tx_event != NULL) {
            tx_done = true;
            if (m_last_tx_event->evt_id == NRF_ESB_EVENT_TX_SUCCESS) tx_success = true;
        }
        CRITICAL_REGION_EXIT();
        __WFI();
    }
    CRITICAL_REGION_ENTER();
    m_last_tx_event = NULL;
    CRITICAL_REGION_EXIT();

    if (oldMode == RADIO_MODE_SNIFF) {
        NRF_LOG_DEBUG("switching back to passive PRX after TX...")
        radioSetMode(oldMode);
        nrf_esb_start_rx();
    }

    return tx_success;
    
}

bool radioTransmitCollectAck(nrf_esb_payload_t *p_tx_payload, bool blockTillResult, bool *ack_payload_received, nrf_esb_payload_t *ack_payload) {
    uint32_t ret;

    // if mode isn't PTX, switch over to ptx
    radio_rf_mode_t oldMode = m_radio_state.mode;
    if (oldMode == RADIO_MODE_SNIFF) {
        NRF_LOG_DEBUG("Want to TX but mode is PRX_PASSIVE, stop RX ...")
        nrf_esb_stop_rx();
    }

    radioSetMode(RADIO_MODE_PTX);
    ret = nrf_esb_write_payload(p_tx_payload);
    if(ret != NRF_SUCCESS) {
        NRF_LOG_WARNING("Error write payload: %d", ret);
    }
    if (!blockTillResult) {
        if (oldMode == RADIO_MODE_SNIFF) {
            NRF_LOG_DEBUG("switching back to passive PRX after TX...")
            radioSetMode(oldMode);
            nrf_esb_start_rx();
        }
        return true;
    }

    bool tx_done = false;
    bool tx_success = false;
    while (!tx_done) {
        CRITICAL_REGION_ENTER();
        if (m_last_tx_event != NULL) {
            tx_done = true;
            if (m_last_tx_event->evt_id == NRF_ESB_EVENT_TX_SUCCESS) tx_success = true;
        }
        CRITICAL_REGION_EXIT();
        __WFI();
    }
    CRITICAL_REGION_ENTER();
    m_last_tx_event = NULL;
    CRITICAL_REGION_EXIT();

    if (nrf_esb_read_rx_payload(ack_payload) == NRF_SUCCESS) {
        //NRF_LOG_INFO("ACK PAYLOAD received %02x", ack_payload->data[1]);
        tx_success = true; //if TX event was FAILED, but an ACK payload arrived meanwhile return success, anyways
        *ack_payload_received = true;
    } else {
        *ack_payload_received = false;
    } 

    if (oldMode == RADIO_MODE_SNIFF) {
        NRF_LOG_DEBUG("switching back to passive PRX after TX...")
        radioSetMode(oldMode);
        nrf_esb_start_rx();
    }

    return tx_success;
    
}


void timer_tx_frame_from_scheduler(void *p_event_data, uint16_t event_size) {
    // process scheduled event for this handler in main loop during scheduler processing
    nrf_esb_payload_t *p_tx_payload = (nrf_esb_payload_t *) p_event_data;
    radioTransmit(p_tx_payload, false);
}

void timer_tx_frame_to_scheduler(void* p_context) {
    // schedule as event to main  (not isr) instead of executing directly
    app_sched_event_put(p_context, sizeof(app_timer_id_t), timer_tx_frame_from_scheduler);
}

void radioTransmitDelayed(nrf_esb_payload_t *p_tx_payload, uint32_t delay_ms) {
    // starts the single shot timer, which calls back to timer_tx_frame_to_scheduler
    app_timer_start(m_timer_delayed_tx_frame, APP_TIMER_TICKS(delay_ms), p_tx_payload);
}


static bool m_ping_sweep_running;
static uint8_t m_ping_sweep_pipe;
static bool m_ping_sweep_succeeded;
void timer_ping_from_scheduler(void *p_event_data, uint16_t event_size) {
    uint8_t pipe_num = *((uint8_t*) p_event_data);
    // process scheduled event for this handler in main loop during scheduler processing
    if (radioPingPRX(pipe_num)) {
        // ping succeeded
        NRF_LOG_INFO("PING SWEEP SUCCEEDED ON CH %d", m_radio_state.rf_channel);
        m_ping_sweep_succeeded = true;
        m_ping_sweep_running = false;
        return;
    } else {
        // ping failed, change to next channel and re-schedule
        uint8_t ch_idx;
        radioNextRfChannel();
        radioGetRfChannelIndex(&ch_idx);
        if (ch_idx == 0) {
            NRF_LOG_INFO("PING SWEEP FAILED", m_radio_state.rf_channel);
            // iterate over all channels without success, ping sweep failed
            m_ping_sweep_running = false;
            m_ping_sweep_succeeded = false;
            return;
        }

        // restart timer
        app_timer_start(m_timer_ping_sweep, APP_TIMER_TICKS(1), &m_ping_sweep_pipe);
    }
    
}

void timer_ping_to_scheduler(void* p_context) {
    // schedule as event to main  (not isr) instead of executing directly
    app_sched_event_put(p_context, sizeof(uint8_t), timer_ping_from_scheduler);
}

void timer_channel_hop_event_handler(void* p_context)
{
    if (m_radio_state.channel_hop_enabled) {
        
        radioNextRfChannel(); // hop to next channel


        if (m_radio_state.radio_event_handler != NULL) {
            uint8_t currentChIdx;
            radioGetRfChannelIndex(&currentChIdx);

            // send event
            event.evt_id = RADIO_EVENT_CHANNEL_CHANGED;
            event.channel_index = currentChIdx; // encode channel index in pipe num
            m_radio_state.radio_event_handler(&event);

            if (currentChIdx == 0) {
                event.evt_id = RADIO_EVENT_CHANNEL_CHANGED_FIRST_INDEX;
                m_radio_state.radio_event_handler(&event);
            }

        } 

        if (m_radio_state.channel_hop_disable_on_rx && m_radio_state.channel_hop_rx_after_last_hop) {
            // auto disable channel hopping
            app_timer_stop(m_timer_channel_hop);
            m_radio_state.channel_hop_enabled = false;
        } else {
            //re-schedule next hop
            uint32_t err_code = app_timer_start(m_timer_channel_hop, APP_TIMER_TICKS(m_radio_state.channel_hop_delay_ms), NULL); //restart timer
            APP_ERROR_CHECK(err_code);

        }
        m_radio_state.channel_hop_rx_after_last_hop = false;// clear rx indicator
    }   
}

void timer_no_rx_event_handler(void* p_context)
{
    if (m_radio_state.rx_timeout_enabled && !m_radio_state.rx_received_before_timeout) {
        //There was no RX since time has started, fire event
        uint8_t currentChIdx;
        radioGetRfChannelIndex(&currentChIdx);

        // send event
        event.evt_id = RADIO_EVENT_NO_RX_TIMEOUT;
        event.channel_index = currentChIdx; // encode channel index in pipe num
        m_radio_state.radio_event_handler(&event);
    } else {
        NRF_LOG_INFO("RX RECEIVED before RX timeout reached");
    }
}


uint32_t radioInit(nrf_esb_event_handler_t event_handler, radio_event_handler_t radio_event_handler) {
    app_timer_create(&m_timer_delayed_tx_frame, APP_TIMER_MODE_SINGLE_SHOT, timer_tx_frame_to_scheduler);
    app_timer_create(&m_timer_ping_sweep, APP_TIMER_MODE_SINGLE_SHOT, timer_ping_to_scheduler);
    app_timer_create(&m_timer_channel_hop, APP_TIMER_MODE_SINGLE_SHOT, timer_channel_hop_event_handler);
    app_timer_create(&m_timer_no_rx_timeout, APP_TIMER_MODE_SINGLE_SHOT, timer_no_rx_event_handler);

    //uint32_t err_code;
    m_radio_state.event_handler            = event_handler;
    m_radio_state.radio_event_handler      = radio_event_handler;
    //err_code = nrf_esb_init(&m_local_esb_config);
    //VERIFY_SUCCESS(err_code);
    m_radio_state.initialized = true;
    m_radio_state.mode = RADIO_MODE_DISABLED;
    m_radio_state.addr_length = 5;
    m_radio_state.num_pipes = 8;
    m_radio_state.rx_pipes_enabled = 0xFF;

    uint8_t base_addr_0[4] = {0xa5, 0xdc, 0x0a, 0xbb}; //Unifying pairing address for pipe 0
    uint8_t base_addr_1[4] = { 0xC2, 0xC2, 0xC2, 0xC2 }; 
    uint8_t addr_prefix[8] = { 0x75, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8 }; //prefix for pipe 0..7 (pipe 0 prefix for unifying pairing addr)

    memcpy(m_radio_state.base_addr_p0, base_addr_0, 4);
    memcpy(m_radio_state.base_addr_p1, base_addr_1, 4);
    memcpy(m_radio_state.pipe_prefixes, addr_prefix, 8);

    // work with channel index
    m_radio_state.rf_channel = m_radio_state.channel_set.channel_list[m_radio_state.rf_channel_index];

    m_radio_state.channel_hop_delay_ms = RADIO_DEFAULT_CHANNEL_HOP_INTERVAL_MS;
    m_radio_state.channel_hop_enabled = false;

    return NRF_SUCCESS;
    //return err_code;
}

uint32_t radioInitPromiscuousMode() {
    if (!m_radio_state.initialized) return NRF_ERROR_INVALID_STATE;

    uint32_t err_code;
    nrf_esb_config_t esb_config = NRF_ESB_PROMISCUOUS_CONFIG;

    esb_config.event_handler = radio_esb_event_handler;

    err_code = nrf_esb_init(&esb_config);
    VERIFY_SUCCESS(err_code);
    //m_local_esb_config = esb_config;

    while (nrf_esb_flush_rx() != NRF_SUCCESS) {}; //assure we have no frames pending, which have been captured in non-PRX_PASSIVE mode and could get mis-interpreted

    err_code = nrf_esb_set_rf_channel(m_radio_state.rf_channel);
    //err_code = radioSetRfChannelIndex(m_radio_state.rf_channel_index);
    VERIFY_SUCCESS(err_code);

    

    //Note: start_rx here would hinder changing RF address after calling radioSetMode() unless stop_rx is called upfront

    return NRF_SUCCESS;
}

uint32_t radioInitSnifferMode(bool flushrx) {
    if (!m_radio_state.initialized) return NRF_ERROR_INVALID_STATE;

    uint32_t err_code;
    
    nrf_esb_config_t esb_config = NRF_ESB_SNIFF_CONFIG;
    esb_config.event_handler = radio_esb_event_handler;

    err_code = nrf_esb_init(&esb_config);
    VERIFY_SUCCESS(err_code);

    
    //m_local_esb_config = esb_config;
    if (flushrx) {
        while (nrf_esb_flush_rx() != NRF_SUCCESS) {}; //assure we have no frames pending, which have been captured in non-PROMISCOUS mode and could get mis-interpreted
    }
    

    err_code = nrf_esb_set_rf_channel(m_radio_state.rf_channel);
    //err_code = radioSetRfChannelIndex(m_radio_state.rf_channel_index);
    VERIFY_SUCCESS(err_code);

    

    //Note: start_rx here would hinder changing RF address after calling radioSetMode() unless stop_rx is called upfront

    return NRF_SUCCESS;
}

uint32_t radioInitPTXMode() {
    if (!m_radio_state.initialized) return NRF_ERROR_INVALID_STATE;

    uint32_t err_code;
    
    nrf_esb_config_t esb_config = NRF_ESB_DEFAULT_CONFIG;
    //esb_config.mode = NRF_ESB_MODE_PRX;
    //esb_config.selective_auto_ack = true; //Don't send ack if received frame has 'no ack' set
    //esb_config.disallow_auto_ack = true; //never send back acks
    
    //esb_config.event_handler    = m_radio_state.event_handler;
    esb_config.event_handler = radio_esb_event_handler;    
    esb_config.crc = NRF_ESB_CRC_16BIT;
    esb_config.retransmit_count = 1;
    esb_config.retransmit_delay = 5*250;

    err_code = nrf_esb_init(&esb_config);
    VERIFY_SUCCESS(err_code);

    
    //m_local_esb_config = esb_config;

    //while (nrf_esb_flush_rx() != NRF_SUCCESS) {}; //assure we have no frames pending, which have been captured in non-PROMISCOUS mode and could get mis-interpreted


    err_code = nrf_esb_set_rf_channel(m_radio_state.rf_channel);
    //err_code = radioSetRfChannelIndex(m_radio_state.rf_channel_index);
    VERIFY_SUCCESS(err_code);

    

    //Note: start_rx here would hinder changing RF address after calling radioSetMode() unless stop_rx is called upfront

    return NRF_SUCCESS;
}

radio_rf_mode_t radioGetMode() {
    return m_radio_state.mode;
}

uint32_t radioSetMode(radio_rf_mode_t mode) {
    if (m_radio_state.mode == mode) {
        return NRF_SUCCESS; //no change
    }

    uint32_t err_code;

    switch (mode) {
        case RADIO_MODE_DISABLED:
            break;
        case RADIO_MODE_PTX:
            err_code = radioInitPTXMode();
            VERIFY_SUCCESS(err_code);
            m_radio_state.mode = mode; //update current mode
            break;
        case RADIO_MODE_SNIFF:
            /*
            //bring radio to IDLE state, depends on current mode
            if (m_radio_state.mode == RADIO_MODE_PROMISCOUS) {
                nrf_esb_stop_rx();
            }
            */
            if (m_radio_state.mode == RADIO_MODE_PROMISCOUS) {
                err_code = radioInitSnifferMode(true);
            } else {
                err_code = radioInitSnifferMode(false); //don't flush RX if not comming from promiscuous mode
            }
            
            VERIFY_SUCCESS(err_code);
            m_radio_state.mode = mode; //update current mode
            break;
        case RADIO_MODE_PROMISCOUS:
            /*
            //bring radio to IDLE state, depends on current mode
            if (m_radio_state.mode == RADIO_MODE_SNIFF) {
                nrf_esb_stop_rx();
            }
            */
            // promiscous mode always needs (re)init, as Packer Format is changed
            err_code = radioInitPromiscuousMode();
            VERIFY_SUCCESS(err_code);
            m_radio_state.mode = mode; //update current mode
            break;
    }

    return 0;
}

uint32_t radioSetAddressLength(uint8_t length) {
    if (m_radio_state.mode == RADIO_MODE_PROMISCOUS) return NRF_ERROR_FORBIDDEN;
    uint32_t err_code = nrf_esb_set_address_length(length);
    VERIFY_SUCCESS(err_code);
    m_radio_state.addr_length = length;
    return err_code;
}

uint32_t radioSetBaseAddress0(uint8_t const * p_addr) {
    if (m_radio_state.mode == RADIO_MODE_PROMISCOUS) return NRF_ERROR_FORBIDDEN;
    uint32_t err_code = nrf_esb_set_base_address_0(p_addr);
    VERIFY_SUCCESS(err_code);
    memcpy(m_radio_state.base_addr_p0, p_addr, 4);
    return err_code;
}

uint32_t radioSetBaseAddress1(uint8_t const * p_addr) {
    if (m_radio_state.mode == RADIO_MODE_PROMISCOUS) return NRF_ERROR_FORBIDDEN;
    uint32_t err_code = nrf_esb_set_base_address_1(p_addr);
    VERIFY_SUCCESS(err_code);
    memcpy(m_radio_state.base_addr_p1, p_addr, 4);
    return err_code;
}

uint32_t radioSetPrefixes(uint8_t const * p_prefixes, uint8_t num_pipes) {
    if (m_radio_state.mode == RADIO_MODE_PROMISCOUS) return NRF_ERROR_FORBIDDEN;
    uint32_t err_code = nrf_esb_set_prefixes(p_prefixes, num_pipes);
    VERIFY_SUCCESS(err_code);
    memcpy(m_radio_state.pipe_prefixes, p_prefixes, num_pipes);
    m_radio_state.num_pipes = num_pipes;
    m_radio_state.rx_pipes_enabled = BIT_MASK_UINT_8(num_pipes);
    return err_code;
}

uint32_t radioEnablePipes(uint8_t enable_mask) {
    if (m_radio_state.mode == RADIO_MODE_PROMISCOUS) return NRF_ERROR_FORBIDDEN;
    uint32_t err_code = nrf_esb_enable_pipes(enable_mask);
    VERIFY_SUCCESS(err_code);

    m_radio_state.rx_pipes_enabled = enable_mask;
    return err_code;
}

uint32_t radioUpdatePrefix(uint8_t pipe, uint8_t prefix) {
    if (m_radio_state.mode == RADIO_MODE_PROMISCOUS) return NRF_ERROR_FORBIDDEN;
    uint32_t err_code = nrf_esb_update_prefix(pipe, prefix);
    VERIFY_SUCCESS(err_code);
    m_radio_state.pipe_prefixes[pipe] = prefix;
    return err_code;
}

uint32_t radioSetRfChannel(uint32_t channel) {
    uint32_t ret;
    ret = nrf_esb_set_rf_channel(channel);
    if (ret == NRF_SUCCESS) m_radio_state.rf_channel = channel;
    return ret;
}

uint32_t radioSetRfChannelIndex(uint8_t channel_idx) {
    uint8_t idx = channel_idx % m_radio_state.channel_set.channel_list_length;
    if (idx != channel_idx) return NRF_ERROR_INVALID_PARAM;

    m_radio_state.rf_channel_index = channel_idx;
    return radioSetRfChannel(m_radio_state.channel_set.channel_list[channel_idx]);
}

uint32_t radioGetRfChannelIndex(uint8_t *channel_index_result) {
    if (channel_index_result == NULL) return NRF_ERROR_INVALID_PARAM;
    *channel_index_result = m_radio_state.rf_channel_index;
    return NRF_SUCCESS;
}


uint32_t radioNextRfChannel() {
    uint8_t new_idx = m_radio_state.rf_channel_index;
    new_idx++;
    new_idx %= m_radio_state.channel_set.channel_list_length;
    //NRF_LOG_INFO("channel idx set to %d", new_idx);
    return radioSetRfChannelIndex(new_idx);
}

uint32_t radioGetRfChannel(uint32_t * p_channel) {
    return nrf_esb_get_rf_channel(p_channel);
}

uint32_t radioPipeNumToRFAddress(uint8_t pipeNum, uint8_t *p_dst) {
    if (pipeNum > 8) return NRF_ERROR_INVALID_PARAM;
    if (p_dst == NULL) return NRF_ERROR_INVALID_PARAM;

    //ToDo: account for address length, currently length 5 is assumed

    if (pipeNum == 0) {
        p_dst[0] = m_radio_state.base_addr_p0[3];
        p_dst[1] = m_radio_state.base_addr_p0[2];
        p_dst[2] = m_radio_state.base_addr_p0[1];
        p_dst[3] = m_radio_state.base_addr_p0[0];
    } else {
        p_dst[0] = m_radio_state.base_addr_p1[3];
        p_dst[1] = m_radio_state.base_addr_p1[2];
        p_dst[2] = m_radio_state.base_addr_p1[1];
        p_dst[3] = m_radio_state.base_addr_p1[0];
    }

    p_dst[4] = m_radio_state.pipe_prefixes[pipeNum];

    return NRF_SUCCESS;
}

static nrf_esb_payload_t m_ping_payload;

bool radioPingPRX(uint8_t pipe_num) {
    m_ping_payload.pipe = pipe_num;
    m_ping_payload.length = 1; // to avoid error during payload write
    nrf_esb_flush_tx(); // clear TX pipe before ping
    return radioTransmit(&m_ping_payload, true);
}

// sends ping on all channel, returns NRF_SUCCESS on hit (without further channel change)
// returns error if no hit on any channel
uint32_t radioPingSweepPRX(uint8_t pipe_num, uint8_t *channel_index_result) {
    if (m_ping_sweep_running) {
        NRF_LOG_ERROR("ping sweep already running");
        return NRF_ERROR_BUSY;
    }

    m_ping_sweep_running = true;
    m_ping_sweep_pipe = pipe_num;
    app_timer_start(m_timer_ping_sweep, APP_TIMER_TICKS(1), &m_ping_sweep_pipe);

    return NRF_SUCCESS;
}



uint32_t radio_start_channel_hopping(uint32_t interval, uint32_t start_delay_ms, bool disable_on_rx) {
    if (m_radio_state.channel_hop_enabled) return NRF_SUCCESS;
    m_radio_state.channel_hop_enabled = true;
    if (start_delay_ms == 0) start_delay_ms++;
    m_radio_state.channel_hop_disable_on_rx = disable_on_rx;
    m_radio_state.channel_hop_rx_after_last_hop = false;
    app_timer_start(m_timer_channel_hop, APP_TIMER_TICKS(start_delay_ms), m_timer_channel_hop);
    NRF_LOG_INFO("Channel hopping started");

    return NRF_SUCCESS;
}

uint32_t radio_stop_channel_hopping() {
    app_timer_stop(m_timer_channel_hop);
    m_radio_state.channel_hop_enabled = false;
    return NRF_SUCCESS;
}

uint32_t radio_enable_rx_timeout_event(uint32_t timeout_ms) {
    m_radio_state.rx_received_before_timeout = false;
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