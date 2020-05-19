// LOGITacker - Hardware tool to enumerate and test vulnerabilities of Logitech wireless input devices
//
// Copyright © 2019 Marcus Mengs
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.#include <stdint.h>
#include "stdbool.h"
#include "stddef.h"

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
#include "string.h"
#include "fds.h"
#include "logitacker_unifying.h"

#include "logitacker_bsp.h"
#include "logitacker_radio.h"
#include "logitacker.h"

//crypto
#include "nrf_crypto.h"


#define CHANNEL_HOP_RESTART_DELAY 1300

// Scheduler settings
#define SCHED_MAX_EVENT_DATA_SIZE   BYTES_PER_WORD*BYTES_TO_WORDS(MAX(NRF_ESB_CHECK_PROMISCUOUS_SCHED_EVENT_DATA_SIZE,MAX(APP_TIMER_SCHED_EVENT_DATA_SIZE,MAX(sizeof(nrf_esb_payload_t),sizeof(nrf_esb_evt_t)))))


#define SCHED_QUEUE_SIZE            64



#ifdef NRF52840_MDK
    bool with_log = true;
#else
    bool with_log = false;
#endif


/*
#define AUTO_BRUTEFORCE true
static bool continue_frame_recording = true;
static bool enough_frames_recorded = false;
static bool continuo_redording_even_if_enough_frames = false;


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
*/

NRF_CLI_CDC_ACM_DEF(m_cli_cdc_acm_transport);
NRF_CLI_DEF(m_cli_cdc_acm, g_logitacker_cli_name, &m_cli_cdc_acm_transport.transport, '\r', 20);




int main(void)
{
//    continue_frame_recording = true;

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

    if (with_log) {
        NRF_LOG_DEFAULT_BACKENDS_INIT();  
    } 

    /* CLI configured as NRF_LOG backend */
    ret = nrf_cli_init(&m_cli_cdc_acm, NULL, true, true, NRF_LOG_SEVERITY_INFO);
    APP_ERROR_CHECK(ret);
    ret = nrf_cli_start(&m_cli_cdc_acm);
    APP_ERROR_CHECK(ret);


    //unifying_init(unifying_event_handler);


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
