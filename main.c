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


#define BTN_TRIGGER_ACTION   0
bool report_frames_without_crc_match = true; // if enabled, invalid promiscuous mode frames are pushed through as USB HID reports
bool switch_from_promiscous_to_sniff_on_discovered_address = true; // if enabled, the dongle automatically toggles to sniffing mode for captured addresses

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


/*
static uint8_t hid_out_report[LOGITACKER_USB_HID_GENERIC_OUT_REPORT_MAXSIZE];
static bool processing_hid_out_report = false;
*/

void clocks_start( void )
{
    NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
    NRF_CLOCK->TASKS_HFCLKSTART = 1;

    while (NRF_CLOCK->EVENTS_HFCLKSTARTED == 0);
}

/* FDS */
/*
static dongle_state_t m_dongle_state = {
    .boot_count = 0,
    .device_info_count = 0,
};


// current device info
static device_info_t m_current_device_info =
{
    .RfAddress = {0x75, 0xa5, 0xdc, 0x0a, 0xbb}, //prefix, addr3, addr2, addr1, addr0
};
*/

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

    //BSP
  //  init_bsp();

    //FDS
    // Register first to receive an event when initialization is complete.
    (void) fds_register(fds_evt_handler);
    //init
    ret = fds_init();
    APP_ERROR_CHECK(ret);
    // Wait for fds to initialize.
    wait_for_fds_ready();


    //USB
    logitacker_usb_init();

    if (with_log) {
        NRF_LOG_DEFAULT_BACKENDS_INIT();  
    } 

    /* CLI configured as NRF_LOG backend */
    ret = nrf_cli_init(&m_cli_cdc_acm, NULL, true, true, NRF_LOG_SEVERITY_INFO);
    APP_ERROR_CHECK(ret);
    ret = nrf_cli_start(&m_cli_cdc_acm);
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

/*
    ret = nrf_crypto_init();
    if (ret == NRF_SUCCESS) {
        NRF_LOG_ERROR("nrf_crypto_init error: 0x%x", ret);
        return ret;
    }
*/
//    timestamp_init();

    while (true)
    {


        app_sched_execute(); //!! esb_promiscuous mode frame validation is handled by scheduler !!
        //while (app_usbd_event_queue_process()) { }
        nrf_cli_process(&m_cli_cdc_acm);

        /*
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
                    //nrf_esb_stop_rx();
                    if (nrf_esb_set_rf_channel(ch) == NRF_SUCCESS) {
                        hid_out_report[2] = 0;
                    } else {
                        hid_out_report[2] = -1;
                    }
                    //while (nrf_esb_start_rx() != NRF_SUCCESS) {};
                    
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
        */

        UNUSED_RETURN_VALUE(NRF_LOG_PROCESS());
        /* Sleep CPU only if there was no interrupt since last loop processing */
        __WFE();
    }
}
