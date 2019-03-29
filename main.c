/**
 * Copyright (c) 2017 - 2018, Nordic Semiconductor ASA
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "nrf.h"
#include "nrf_esb_illegalmod.h"
#include "nrf_esb_illegalmod_error_codes.h"
#include "nrf_delay.h"
#include "app_util_platform.h"
#include "nrf_drv_usbd.h"
#include "nrf_drv_clock.h"
#include "nrf_gpio.h"
#include "nrf_drv_power.h"

#include "app_timer.h"
#include "app_usbd.h"
#include "app_usbd_core.h"
#include "app_usbd_hid_generic.h"
#include "app_usbd_hid_mouse.h"
#include "app_usbd_hid_kbd.h"
#include "app_error.h"
#include "bsp.h"


#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "app_scheduler.h"

//Flash FDS
#include <string.h>
#include "fds.h"
#include "flash_device_info.h"
#include "state.h"
#include "unifying.h"
#include "hid.h"
#include "radio.h"

#include "led_rgb.h"

#include "timestamp.h"

#define CHANNEL_HOP_INTERVAL 30
#define CHANNEL_HOP_RESTART_DELAY 1200

// Scheduler settings
#define SCHED_MAX_EVENT_DATA_SIZE   BYTES_TO_WORDS(MAX(sizeof(unifying_rf_record_set_t),MAX(MAX(sizeof(nrf_esb_evt_t), APP_TIMER_SCHED_EVENT_DATA_SIZE), sizeof(nrf_esb_payload_t))))
//#define SCHED_MAX_EVENT_DATA_SIZE   APP_TIMER_SCHED_EVENT_DATA_SIZE
#define SCHED_QUEUE_SIZE            32

// channel hop timer
APP_TIMER_DEF(m_timer_channel_hop);
static bool m_channel_hop_data_received = false;
uint32_t m_channel_hop_delay_ms = CHANNEL_HOP_INTERVAL;


/**
 * @brief Enable USB power detection
 */
#ifndef USBD_POWER_DETECTION
#define USBD_POWER_DETECTION true
#endif


#define BTN_TRIGGER_ACTION   0

    bool report_frames_without_crc_match = false; // if enabled, invalid promiscuous mode frames are pushed through as USB HID reports
    bool switch_from_promiscous_to_sniff_on_discovered_address = true; // if enabled, the dongle automatically toggles to sniffing mode for captured addresses
#ifdef NRF52840_MDK
    bool test_replay_rx = false;
    bool with_log = true;
#else
    bool test_replay_rx = false;
    bool with_log = false;
#endif


/**
 * @brief Additional key release events
 *
 * This example needs to process release events of used buttons
 */
enum {
    BSP_USER_EVENT_RELEASE_0 = BSP_EVENT_KEY_LAST + 1, /**< Button 0 released */
    BSP_USER_EVENT_RELEASE_1,                          /**< Button 1 released */
    BSP_USER_EVENT_RELEASE_2,                          /**< Button 2 released */
    BSP_USER_EVENT_RELEASE_3,                          /**< Button 3 released */
    BSP_USER_EVENT_RELEASE_4,                          /**< Button 4 released */
    BSP_USER_EVENT_RELEASE_5,                          /**< Button 5 released */
    BSP_USER_EVENT_RELEASE_6,                          /**< Button 6 released */
    BSP_USER_EVENT_RELEASE_7,                          /**< Button 7 released */
};


// created HID report descriptor with vendor define output / input report of max size in raw_desc
APP_USBD_HID_GENERIC_SUBCLASS_REPORT_DESC(raw_desc,APP_USBD_HID_RAW_REPORT_DSC_SIZE(REPORT_OUT_MAXSIZE));
// add created HID report descriptor to subclass descriptor list
static const app_usbd_hid_subclass_desc_t * reps[] = {&raw_desc};
// setup generic HID interface 
APP_USBD_HID_GENERIC_GLOBAL_DEF(m_app_hid_generic,
                                HID_GENERIC_INTERFACE,
                                usbd_hid_event_handler,
                                ENDPOINT_LIST(),
                                reps,
                                REPORT_IN_QUEUE_SIZE,
                                REPORT_OUT_MAXSIZE,
                                APP_USBD_HID_SUBCLASS_BOOT,
                                APP_USBD_HID_PROTO_GENERIC);



// internal state
struct
{
    int16_t counter;    /**< Accumulated x state */
    int16_t lastCounter;
}m_state;

static bool m_report_pending; //Mark ongoing USB transmission


void logPriority(char* source) {
    if (current_int_priority_get() == APP_IRQ_PRIORITY_THREAD) {
        NRF_LOG_INFO("%s: Running in Thread/main mode", source);
    } else {
        NRF_LOG_INFO("%s: Running in Interrupt mode", source);
    } 
}

static uint8_t hid_out_report[REPORT_OUT_MAXSIZE];
static bool processing_hid_out_report = false;
/**
 * @brief Class specific event handler.
 *
 * @param p_inst    Class instance.
 * @param event     Class specific event.
 * */
static void usbd_hid_event_handler(app_usbd_class_inst_t const * p_inst,
                                app_usbd_hid_user_event_t event)
{
    //logPriority("usbd_hid_event_handler");
    switch (event)
    {
        case APP_USBD_HID_USER_EVT_OUT_REPORT_READY:
        {
            size_t out_rep_size = REPORT_OUT_MAXSIZE;
            const uint8_t* out_rep = app_usbd_hid_generic_out_report_get(&m_app_hid_generic, &out_rep_size);
            memcpy(&hid_out_report, out_rep, REPORT_OUT_MAXSIZE);
            processing_hid_out_report = true;
            break;
        }
        case APP_USBD_HID_USER_EVT_IN_REPORT_DONE:
        {
            m_report_pending = false;
            break;
        }
        case APP_USBD_HID_USER_EVT_SET_BOOT_PROTO:
        {
            UNUSED_RETURN_VALUE(hid_generic_clear_buffer(p_inst));
            NRF_LOG_INFO("SET_BOOT_PROTO");
            break;
        }
        case APP_USBD_HID_USER_EVT_SET_REPORT_PROTO:
        {
            UNUSED_RETURN_VALUE(hid_generic_clear_buffer(p_inst));
            NRF_LOG_INFO("SET_REPORT_PROTO");
            break;
        }
        default:
            break;
    }
}

/**
 * @brief USBD library specific event handler.
 *
 * @param event     USBD library event.
 * */
static void usbd_user_ev_handler(app_usbd_event_type_t event)
{
    //runs in thread mode
    switch (event)
    {
        case APP_USBD_EVT_DRV_SOF:
            break;
        case APP_USBD_EVT_DRV_RESET:
            m_report_pending = false;
            break;
        case APP_USBD_EVT_DRV_SUSPEND:
            m_report_pending = false;
            app_usbd_suspend_req(); // Allow the library to put the peripheral into sleep mode
            bsp_board_led_off(LED_G);
            break;
        case APP_USBD_EVT_DRV_RESUME:
            m_report_pending = false;
            bsp_board_led_on(LED_G);
            break;
        case APP_USBD_EVT_STARTED:
            m_report_pending = false;
            bsp_board_led_on(LED_G);
            break;
        case APP_USBD_EVT_STOPPED:
            app_usbd_disable();
            bsp_board_led_off(LED_G);
            break;
        case APP_USBD_EVT_POWER_DETECTED:
            NRF_LOG_INFO("USB power detected");
            if (!nrf_drv_usbd_is_enabled())
            {
                app_usbd_enable();
            }
            break;
        case APP_USBD_EVT_POWER_REMOVED:
            NRF_LOG_INFO("USB power removed");
            app_usbd_stop();
            break;
        case APP_USBD_EVT_POWER_READY:
            NRF_LOG_INFO("USB ready");
            app_usbd_start();
            break;
        default:
            break;
    }
}

// handle event (press of physical dongle button)
static void bsp_event_callback(bsp_event_t ev)
{
    // runs in interrupt mode
    //logPriority("bsp_event_callback");
    //uint32_t ret;
    switch ((unsigned int)ev)
    {
        case CONCAT_2(BSP_EVENT_KEY_, BTN_TRIGGER_ACTION):
            //Toggle radio back to promiscous mode
            bsp_board_led_on(LED_B);
            break;

        case CONCAT_2(BSP_USER_EVENT_RELEASE_, BTN_TRIGGER_ACTION):
            NRF_LOG_INFO("Button pressed, falling back to promiscuous mode...")
            
            while (nrf_esb_stop_rx() != NRF_SUCCESS) {};

            radioSetMode(RADIO_MODE_PROMISCOUS); //set back to promiscous

            m_channel_hop_delay_ms = CHANNEL_HOP_INTERVAL; // set timey delay to channel hop interval
            m_channel_hop_data_received = false;

            app_timer_start(m_timer_channel_hop, APP_TIMER_TICKS(m_channel_hop_delay_ms), m_timer_channel_hop); //restart channel hopping timer
            nrf_esb_start_rx();

            bsp_board_led_off(LED_B);
            break;

        default:
            return; // no implementation needed
    }
}


/**
 * @brief Auxiliary internal macro
 *
 * Macro used only in @ref init_bsp to simplify the configuration
 */
#define INIT_BSP_ASSIGN_RELEASE_ACTION(btn)                      \
    APP_ERROR_CHECK(                                             \
        bsp_event_to_button_action_assign(                       \
            btn,                                                 \
            BSP_BUTTON_ACTION_RELEASE,                           \
            (bsp_event_t)CONCAT_2(BSP_USER_EVENT_RELEASE_, btn)) \
    )

static void init_bsp(void)
{
    ret_code_t ret;
    ret = bsp_init(BSP_INIT_BUTTONS, bsp_event_callback);
    APP_ERROR_CHECK(ret);

    INIT_BSP_ASSIGN_RELEASE_ACTION(BTN_TRIGGER_ACTION );

    /* Configure LEDs */
    bsp_board_init(BSP_INIT_LEDS);
}

static ret_code_t idle_handle(app_usbd_class_inst_t const * p_inst, uint8_t report_id)
{
    switch (report_id)
    {
        case 0:
        {
            //uint8_t report[] = {0xBE, 0xEF};
            uint8_t report[] = {};
            return app_usbd_hid_generic_idle_report_set(&m_app_hid_generic, report, sizeof(report));
        }
        default:
            return NRF_ERROR_NOT_SUPPORTED;
    }
    
}

/*
* ESB
*/


void nrf_esb_process_rx() {
    static nrf_esb_payload_t rx_payload;

    // we check current channel here, which isn't reliable as the frame from fifo could have been received on a
    // different one, but who cares
    uint32_t ch = 0;
    nrf_esb_get_rf_channel(&ch);

    static uint8_t report[REPORT_IN_MAXSIZE];
    switch (radioGetMode()) {
        case RADIO_MODE_PROMISCOUS:
            // pull RX payload from fifo, till no more left
            while (nrf_esb_read_rx_payload(&rx_payload) == NRF_SUCCESS) {
                
                if (report_frames_without_crc_match) {
                    memset(report,0,REPORT_IN_MAXSIZE);
                    report[0] = rx_payload.pipe;
                    memcpy(&report[2], rx_payload.data, rx_payload.length);
                    app_usbd_hid_generic_in_report_set(&m_app_hid_generic, report, sizeof(report));
                    bsp_board_led_invert(LED_G); // toggle green to indicate received RF data from promiscuous sniffing
                }

                if (validate_esb_payload(&rx_payload) == NRF_SUCCESS) {
                    bsp_board_led_invert(LED_G); // toggle green to indicate valid ESB frame in promiscous mode
                    memset(report,0,REPORT_IN_MAXSIZE);
                    memcpy(report, rx_payload.data, rx_payload.length);
                    app_usbd_hid_generic_in_report_set(&m_app_hid_generic, report, sizeof(report));


                    // assign discovered address to pipe 1 and switch over to passive sniffing (doesn't send ACK payloads)
                    if (switch_from_promiscous_to_sniff_on_discovered_address) {
                        uint8_t RfAddress1[4] = {rx_payload.data[5], rx_payload.data[4], rx_payload.data[3], rx_payload.data[2]}; //prefix, addr3, addr2, addr1, addr0
                        NRF_LOG_INFO("Received valid frame from %02x:%02x:%02x:%02x:%02x, sniffing this address", RfAddress1[3], RfAddress1[2], RfAddress1[1], RfAddress1[0], rx_payload.data[6])

                        bsp_board_leds_off();
                        //m_channel_hop_data_received = true; //don't restart channel hop timer when called ...
                        app_timer_stop(m_timer_channel_hop); // stop channel hop timer
                        app_timer_start(m_timer_channel_hop, APP_TIMER_TICKS(1000), m_timer_channel_hop); //... and restart in 1000ms if no further data received

                        nrf_esb_stop_rx();
                        
                        radioSetMode(RADIO_MODE_PRX_PASSIVE);
                        radioSetBaseAddress1(RfAddress1);
                        radioUpdatePrefix(1, rx_payload.data[6]);   
                        //radioUpdatePrefix(1, 0x4c);   
                        while (nrf_esb_start_rx() != NRF_SUCCESS) {};
                        break; //exit while loop
                    } else {
                        uint8_t RfAddress1[4] = {rx_payload.data[5], rx_payload.data[4], rx_payload.data[3], rx_payload.data[2]}; //prefix, addr3, addr2, addr1, addr0
                        NRF_LOG_INFO("Received valid frame from %02x:%02x:%02x:%02x:%02x, go on with promiscuous mode anyways", RfAddress1[3], RfAddress1[2], RfAddress1[1], RfAddress1[0], rx_payload.data[6])
                    }
                } 
            }
            break;
        case RADIO_MODE_PRX_PASSIVE:

            // pull RX payload from fifo, till no more left
            while (nrf_esb_read_rx_payload(&rx_payload) == NRF_SUCCESS) {
                if (rx_payload.length == 0) bsp_board_led_invert(LED_G); // toggle green led to indicate non-empty frame sniffed
                else if (test_replay_rx) {
                    static nrf_esb_payload_t tx_payload; // = NRF_ESB_CREATE_PAYLOAD(0, 0x01, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00);
                    memcpy(tx_payload.data, rx_payload.data, rx_payload.length);
                    //unifying_payload_update_checksum(tx_payload.data, tx_payload.length);
                    
                    tx_payload.length = rx_payload.length;
                    tx_payload.pipe = rx_payload.pipe;
                    tx_payload.noack = false;

                    if (radioTransmit(&tx_payload, true)) {
                        //NRF_LOG_INFO("TX success");
                    } else {
                        // NRF_LOG_INFO("TX fail");
                    }
                }
                
                
                uint8_t rfReportType;
                bool rfReportIsKeepAlive;
                unifying_frame_classify_log(rx_payload);
                unifying_frame_classify(rx_payload, &rfReportType, &rfReportIsKeepAlive);
                switch (rfReportType) {
                    case UNIFYING_RF_REPORT_SET_KEEP_ALIVE:
                    case UNIFYING_RF_REPORT_ENCRYPTED_KEYBOARD:
                    {
                        bool full_capture = unifying_record_rf_frame(rx_payload);
                        if (full_capture) {
                            NRF_LOG_INFO("scheduling replay");
                            unifying_transmit_records(rx_payload.pipe);
                        }
                        break;
                    }
                    
                    default:
                        break;
                }

                m_channel_hop_delay_ms = CHANNEL_HOP_RESTART_DELAY; // set restart timer interval (will start channel hopping, if no data received after this timeout)
                m_channel_hop_data_received = true; //don't restart channel hop timer
                bsp_board_led_off(LED_B); //assure LED indicating channel hops is disabled

                // hid report:
                // byte 0:    rx pipe
                // byte 1:    ESB payload length
                // byte 2..6: RF address on pipe (account for addr_len when copying over)
                // byte 7:    reserved, would be part of PCF in promiscuous mode (set to 0x00 here)
                // byte 8..:  ESB payload

                memset(report,0,REPORT_IN_MAXSIZE);
                report[0] = rx_payload.pipe;
                report[1] = rx_payload.length;
                radioPipeNumToRFAddress(rx_payload.pipe, &report[2]);
                memcpy(&report[8], rx_payload.data, rx_payload.length);
                
                app_usbd_hid_generic_in_report_set(&m_app_hid_generic, report, sizeof(report));
            }
            break;
        default:
            break;
    }
    
}

void nrf_esb_event_handler(nrf_esb_evt_t *p_event)
{
    //logPriority("nrf_esb_event_handler");
    switch (p_event->evt_id)
    {
        case NRF_ESB_EVENT_TX_SUCCESS:
            NRF_LOG_INFO("nrf_esb_event_handler TX_SUCCESS");
            break;
        case NRF_ESB_EVENT_TX_FAILED:
            NRF_LOG_INFO("nrf_esb_event_handler TX_FAILED");
            //(void) nrf_esb_flush_tx();
            //(void) nrf_esb_start_tx();            
            break;
        case NRF_ESB_EVENT_RX_RECEIVED:
            NRF_LOG_DEBUG("RX RECEIVED EVENT");
            nrf_esb_process_rx();
    }
}

void nrf_esb_event_handler_from_scheduler(void *p_event_data, uint16_t event_size) {
    nrf_esb_event_handler((nrf_esb_evt_t*) p_event_data);
}

void nrf_esb_event_handler_to_scheduler(nrf_esb_evt_t const *p_event) {
    uint16_t queue_space;
    CRITICAL_REGION_ENTER();
    queue_space = app_sched_queue_space_get();
    CRITICAL_REGION_EXIT();

    if (queue_space <= 1) {
        NRF_LOG_INFO("only one event schedule slot left, dropping rx frame");
        bsp_board_led_on(LED_R); //indicate frame arrived, while scheduler is full with red LED (overload)
        return; //drop frame, don't schedule event
    }
    bsp_board_led_off(LED_R);

    //NRF_LOG_INFO("Sched queue space %d", queue_space);
    app_sched_event_put(p_event, sizeof(*p_event), nrf_esb_event_handler_from_scheduler);

}


void clocks_start( void )
{
    NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
    NRF_CLOCK->TASKS_HFCLKSTART = 1;

    while (NRF_CLOCK->EVENTS_HFCLKSTARTED == 0);
}

/* FDS */
static dongle_state_t m_dongle_state = {
    .boot_count = 0,
    .device_info_count = 0,
};


// current device info
static device_info_t m_current_device_info =
{
    .RfAddress = {0x75, 0xa5, 0xdc, 0x0a, 0xbb}, //prefix, addr3, addr2, addr1, addr0
};

// Flag to check fds initialization.
static bool volatile m_fds_initialized;

// Keep track of the progress of a delete_all operation. 
static struct
{
    bool delete_next;   //!< Delete next record.
    bool pending;       //!< Waiting for an fds FDS_EVT_DEL_RECORD event, to delete the next record.
} m_delete_all;


// Sleep until an event is received.
static void power_manage(void)
{
    __WFE();
}


static void wait_for_fds_ready(void)
{
    while (!m_fds_initialized)
    {
        power_manage();
    }
}

static void fds_evt_handler(fds_evt_t const * p_evt)
{
    // runs in thread mode
    //logPriority("fds_evt_handler");
    /*
    NRF_LOG_GREEN("Event: %s received (%s)",
                  fds_evt_str[p_evt->id],
                  fds_err_str[p_evt->result]);
    */

    switch (p_evt->id)
    {
        case FDS_EVT_INIT:
            if (p_evt->result == FDS_SUCCESS)
            {
                m_fds_initialized = true;
            }
            break;

        case FDS_EVT_WRITE:
        {
            if (p_evt->result == FDS_SUCCESS)
            {
                NRF_LOG_INFO("Record ID:\t0x%04x",  p_evt->write.record_id);
                NRF_LOG_INFO("File ID:\t0x%04x",    p_evt->write.file_id);
                NRF_LOG_INFO("Record key:\t0x%04x", p_evt->write.record_key);
            }
        } break;

        case FDS_EVT_DEL_RECORD:
        {
            if (p_evt->result == FDS_SUCCESS)
            {
                NRF_LOG_INFO("Record ID:\t0x%04x",  p_evt->del.record_id);
                NRF_LOG_INFO("File ID:\t0x%04x",    p_evt->del.file_id);
                NRF_LOG_INFO("Record key:\t0x%04x", p_evt->del.record_key);
            }
            m_delete_all.pending = false;
        } break;

        default:
            break;
    }
}


// channel hop timer
void timer_channel_hop_event_handler(void* p_context)
{
    //runs in interrupt mode
    //logPriority("timer_channel_hop_event_handler");
//    app_timer_id_t timer = (app_timer_id_t)p_context;

    if (!m_channel_hop_data_received) {

        /*
        uint32_t err_code = app_timer_start(timer, APP_TIMER_TICKS(m_channel_hop_delay_ms), p_context); //restart timer
        APP_ERROR_CHECK(err_code);
        */

        uint32_t currentChannel;
        radioGetRfChannel(&currentChannel);
        currentChannel += 3;
        if (currentChannel > 74) {
            currentChannel = 5;
            bsp_board_led_invert(LED_B); // toggle blue LED everytime we jumped through all channels (only noticable if no RX frames, as LED is toggled on received RF frame, too)
        }
        radioSetRfChannel(currentChannel);
        m_channel_hop_delay_ms = CHANNEL_HOP_INTERVAL;
    }   
    m_channel_hop_data_received = false;
    //uint32_t err_code = app_timer_start(timer, APP_TIMER_TICKS(m_channel_hop_delay_ms), p_context); //restart timer
    uint32_t err_code = app_timer_start(m_timer_channel_hop, APP_TIMER_TICKS(m_channel_hop_delay_ms), NULL); //restart timer
    APP_ERROR_CHECK(err_code);
}

//Note: app_timer isn't configured to use scheduler, as we have timers which should run in interrupt mode,
// For channel hop, we manually make the timer use the scheduler, to avoid channel hopping during ESB frame processing.
// This works, as ESB frame processing is handled by the scheduler, too.
void timer_channel_hop_event_handler_from_scheduler(void *p_event_data, uint16_t event_size) {
    timer_channel_hop_event_handler((void*) p_event_data);
}

void timer_channel_hop_event_handler_to_scheduler(void* p_context) {
    app_sched_event_put(p_context, sizeof(app_timer_id_t), timer_channel_hop_event_handler_from_scheduler);
}





int main(void)
{
    // Note: For Makerdiary MDK dongle the button isn't working in event driven fashion (only BSP SIMPLE seems to be 
    // supported). Thus this code won't support button interaction on MDK dongle.

    ret_code_t ret;
    static const app_usbd_config_t usbd_config = {
        .ev_state_proc = usbd_user_ev_handler
    };

    APP_SCHED_INIT(SCHED_MAX_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);
    
    ret = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(ret);

    if (with_log) NRF_LOG_DEFAULT_BACKENDS_INIT();

    ret = nrf_drv_clock_init();
    APP_ERROR_CHECK(ret);

    nrf_drv_clock_lfclk_request(NULL);

    while(!nrf_drv_clock_lfclk_is_running())
    {
        /* Just waiting */
    }

    ret = app_timer_init();
    APP_ERROR_CHECK(ret);


    //BSP
    init_bsp();


    //FDS
    // Register first to receive an event when initialization is complete.
    (void) fds_register(fds_evt_handler);
    //init
    ret = fds_init();
    APP_ERROR_CHECK(ret);
    // Wait for fds to initialize.
    wait_for_fds_ready();


    //USB
    ret = app_usbd_init(&usbd_config);
    APP_ERROR_CHECK(ret);

    app_usbd_class_inst_t const * class_inst_generic;
    class_inst_generic = app_usbd_hid_generic_class_inst_get(&m_app_hid_generic);

    ret = hid_generic_idle_handler_set(class_inst_generic, idle_handle);
    APP_ERROR_CHECK(ret);

    ret = app_usbd_class_append(class_inst_generic);
    APP_ERROR_CHECK(ret);

    if (USBD_POWER_DETECTION)
    {
        ret = app_usbd_power_events_enable();
        APP_ERROR_CHECK(ret);
    }
    else
    {
        NRF_LOG_INFO("No USB power detection enabled\r\nStarting USB now");

        app_usbd_enable();
        app_usbd_start();
    }

    //high frequency clock needed for ESB
    clocks_start();

    //ESB
    ret = radioInit(nrf_esb_event_handler_to_scheduler);
    APP_ERROR_CHECK(ret);

    ret = radioSetMode(RADIO_MODE_PROMISCOUS);
    APP_ERROR_CHECK(ret);
    nrf_esb_start_rx();
    NRF_LOG_INFO("Start listening for devices in promiscuous mode");

    unifying_init();
    //ret = nrf_esb_start_rx();
    //if (ret == NRF_SUCCESS) bsp_board_led_on(BSP_BOARD_LED_3);
        

    //FDS
    restoreStateFromFlash(&m_dongle_state);

    //Try to load first device info record from flash, create if not existing
    ret = restoreDeviceInfoFromFlash(0, &m_current_device_info);
    if (ret != FDS_SUCCESS) {
        // restore failed, update/create record on flash with current data
        updateDeviceInfoOnFlash(0, &m_current_device_info); //ignore errors
    } 

    //app_timer_create(&m_timer_channel_hop, APP_TIMER_MODE_SINGLE_SHOT, timer_channel_hop_event_handler);
    app_timer_create(&m_timer_channel_hop, APP_TIMER_MODE_SINGLE_SHOT, timer_channel_hop_event_handler_to_scheduler);
    app_timer_start(m_timer_channel_hop, APP_TIMER_TICKS(m_channel_hop_delay_ms), m_timer_channel_hop);

    timestamp_init();

    while (true)
    {
        app_sched_execute();
        while (app_usbd_event_queue_process())
        {
            /* Nothing to do */
        }

        if (processing_hid_out_report) {
            uint8_t command = hid_out_report[1]; //preserve pos 0 for report ID
            uint32_t ch = 0;
            switch (command) {
                case HID_COMMAND_GET_CHANNEL:
                    nrf_esb_get_rf_channel(&ch);
                    hid_out_report[2] = (uint8_t) ch;
                    memset(&hid_out_report[3], 0, sizeof(hid_out_report)-3);
                    app_usbd_hid_generic_in_report_set(&m_app_hid_generic, hid_out_report, sizeof(hid_out_report)); //send back 
                    break;
                case HID_COMMAND_SET_CHANNEL:
                    //nrf_esb_get_rf_channel(&ch);
                    ch = (uint32_t) hid_out_report[2];
                    nrf_esb_stop_rx();
                    if (nrf_esb_set_rf_channel(ch) == NRF_SUCCESS) {
                        hid_out_report[2] = 0;
                    } else {
                        hid_out_report[2] = -1;
                    }
                    while (nrf_esb_start_rx() != NRF_SUCCESS) {};
                    
                    memset(&hid_out_report[3], 0, sizeof(hid_out_report)-3);
                    app_usbd_hid_generic_in_report_set(&m_app_hid_generic, hid_out_report, sizeof(hid_out_report)); //send back 
                    break;
                case HID_COMMAND_GET_ADDRESS:
                    hid_out_report[2] = m_current_device_info.RfAddress[4];
                    hid_out_report[3] = m_current_device_info.RfAddress[3];
                    hid_out_report[4] = m_current_device_info.RfAddress[2];
                    hid_out_report[5] = m_current_device_info.RfAddress[1];
                    hid_out_report[6] = m_current_device_info.RfAddress[0];
                    memset(&hid_out_report[7], 0, sizeof(hid_out_report)-7);

                    hid_out_report[8] = (uint8_t) (m_dongle_state.boot_count &0xFF);
                    app_usbd_hid_generic_in_report_set(&m_app_hid_generic, hid_out_report, sizeof(hid_out_report)); //send back 
                    break;
                default:
                    //echo back
                    app_usbd_hid_generic_in_report_set(&m_app_hid_generic, hid_out_report, sizeof(hid_out_report)); //send back copy of out report as in report
                    
            }
            processing_hid_out_report = false;
        }

        UNUSED_RETURN_VALUE(NRF_LOG_PROCESS());
        /* Sleep CPU only if there was no interrupt since last loop processing */
        __WFE();
    }
}
