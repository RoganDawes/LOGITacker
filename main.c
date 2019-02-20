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
#include "crc16.h"


#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"


/**
 * @brief Enable USB power detection
 */
#ifndef USBD_POWER_DETECTION
#define USBD_POWER_DETECTION true
#endif

/**
 * @brief HID generic class interface number.
 * */
#define HID_GENERIC_INTERFACE  0

/**
 * @brief HID generic class endpoint number.
 * */
#define HID_GENERIC_EPIN       NRF_DRV_USBD_EPIN1

#define BTN_TRIGGER_ACTION   0

/**
 * @brief Number of reports defined in report descriptor.
 */
#define REPORT_IN_QUEUE_SIZE    1

/**
 * @brief Size of maximum output report. HID generic class will reserve
 *        this buffer size + 1 memory space. 
 *
 * Maximum value of this define is 63 bytes. Library automatically adds
 * one byte for report ID. This means that output report size is limited
 * to 64 bytes.
 */
#define REPORT_OUT_MAXSIZE  64
#define REPORT_IN_MAXSIZE REPORT_OUT_MAXSIZE

/**
 * @brief HID generic class endpoints count.
 * */
#define HID_GENERIC_EP_COUNT  1

/**
 * @brief List of HID generic class endpoints.
 * */
#define ENDPOINT_LIST()                                      \
(                                                            \
        HID_GENERIC_EPIN                                     \
)

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


#define HID_COMMAND_SET_RF_MODE 1
#define HID_COMMAND_SET_CHANNEL 3
#define HID_COMMAND_GET_CHANNEL 4
/**
 * @brief User event handler.
 * */
static void hid_user_ev_handler(app_usbd_class_inst_t const * p_inst,
                                app_usbd_hid_user_event_t event);

#define APP_USBD_HID_RAW_REPORT_DSC_SIZE(sizebytes) {                \
0x06, 0x00, 0xFF,  /* Usage Page (Vendor Defined 0xFF00) */    \
0x09, 0x01,        /* Usage (0x01) */    \
0xA1, 0x01,        /* Collection (Application) */   \
0x09, 0x01,        /* Usage (0x01) */   \
0x15, 0x00,        /* Logical Minimum (0) */   \
0x26, 0xFF, 0x00,  /* Logical Maximum (255) */   \
0x75, 0x08,        /* Report Size (8)  */  \
0x95, sizebytes,   /* Report Count (64) */   \
0x81, 0x02,        /* Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position) */   \
0x09, 0x02,        /* Usage (0x02) */   \
0x15, 0x00,        /* Logical Minimum (0) */   \
0x26, 0xFF, 0x00,  /* Logical Maximum (255) */   \
0x75, 0x08,        /* Report Size (8) */   \
0x95, sizebytes,   /* Report Count (64) */   \
0x91, 0x02,        /* Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)  */  \
0xC0,              /* End Collection */   \
}

APP_USBD_HID_GENERIC_SUBCLASS_REPORT_DESC(raw_desc,APP_USBD_HID_RAW_REPORT_DSC_SIZE(REPORT_OUT_MAXSIZE));

static const app_usbd_hid_subclass_desc_t * reps[] = {&raw_desc};

APP_USBD_HID_GENERIC_GLOBAL_DEF(m_app_hid_generic,
                                HID_GENERIC_INTERFACE,
                                hid_user_ev_handler,
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

/**
 * @brief Mark the ongoing transmission
 *
 * Marks that the report buffer is busy and cannot be used until transmission finishes
 * or invalidates (by USB reset or suspend event).
 */
static bool m_report_pending;



static uint8_t processing_rf_frame = 0;


/*
static void hid_send_in_report_if_state_changed(void)
{
    if (m_report_pending)
        return;

     if (m_state.counter != m_state.lastCounter) {

        ret_code_t ret;
        static uint8_t report[REPORT_IN_MAXSIZE];
        // We have some status changed that we need to transfer 

        for (uint8_t i=0; i<REPORT_IN_MAXSIZE;i++) {
            report[i] = i;
        }
        report[0] = (uint8_t) m_state.counter & 0xff;
        
        // Start the transfer 
        ret = app_usbd_hid_generic_in_report_set(
            &m_app_hid_generic,
            report,
            sizeof(report));
        if (ret == NRF_SUCCESS)
        {
            m_report_pending = true;

            CRITICAL_REGION_ENTER();
            m_state.lastCounter = m_state.counter;
            CRITICAL_REGION_EXIT();
        }
    }
}
*/

/**
 * @brief HID generic IN report send handling
 * */
static void hid_process_button_event(int8_t param)
{
    CRITICAL_REGION_ENTER();
    /*
     * Update mouse state
     */
    switch (param)
    {
        case 1: //press
            m_state.counter += 1;
            break;
        case -1: //release
            m_state.counter += 2;
            break;
    }
    CRITICAL_REGION_EXIT();
}

/**
 * @brief Class specific event handler.
 *
 * @param p_inst    Class instance.
 * @param event     Class specific event.
 * */
static void hid_user_ev_handler(app_usbd_class_inst_t const * p_inst,
                                app_usbd_hid_user_event_t event)
{
    switch (event)
    {
        case APP_USBD_HID_USER_EVT_OUT_REPORT_READY:
        {
            /* No output report defined for this example.*/
            size_t out_rep_size = REPORT_OUT_MAXSIZE;
            const uint8_t* out_rep = app_usbd_hid_generic_out_report_get(&m_app_hid_generic, &out_rep_size);

            //echo back
            static uint8_t report[REPORT_IN_MAXSIZE];
            for (uint8_t i=1; i<out_rep_size && i<REPORT_IN_MAXSIZE; i++) {
                report[i] = out_rep[i];
            }
            report[0] = 0x40;
            //report[0] = (uint8_t) m_state.counter & 0xff;
            app_usbd_hid_generic_in_report_set(&m_app_hid_generic, report, sizeof(report));
            break;
        }
        case APP_USBD_HID_USER_EVT_IN_REPORT_DONE:
        {
            m_report_pending = false;
            //hid_send_in_report_if_state_changed();
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
            bsp_board_leds_off();
            break;
        case APP_USBD_EVT_DRV_RESUME:
            m_report_pending = false;
            bsp_board_led_on(BSP_BOARD_LED_0);
            break;
        case APP_USBD_EVT_STARTED:
            m_report_pending = false;
            bsp_board_led_on(BSP_BOARD_LED_0);
            break;
        case APP_USBD_EVT_STOPPED:
            app_usbd_disable();
            bsp_board_leds_off();
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
    switch ((unsigned int)ev)
    {
        case CONCAT_2(BSP_EVENT_KEY_, BTN_TRIGGER_ACTION):
            bsp_board_led_on(1);
            hid_process_button_event(1);
            break;

        case CONCAT_2(BSP_USER_EVENT_RELEASE_, BTN_TRIGGER_ACTION):
            bsp_board_led_off(1);
            hid_process_button_event(-1);
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
            uint8_t report[] = {0xBE, 0xEF};
            return app_usbd_hid_generic_idle_report_set(
              &m_app_hid_generic,
              report,
              sizeof(report));
        }
        default:
            return NRF_ERROR_NOT_SUPPORTED;
    }
    
}

/*
* ESB
*/

nrf_esb_payload_t rx_payload;


void nrf_esb_event_handler(nrf_esb_evt_t const * p_event)
{
    switch (p_event->evt_id)
    {
        case NRF_ESB_EVENT_TX_SUCCESS:
            NRF_LOG_DEBUG("TX SUCCESS EVENT");
            break;
        case NRF_ESB_EVENT_TX_FAILED:
            NRF_LOG_DEBUG("TX FAILED EVENT");
            break;
        case NRF_ESB_EVENT_RX_RECEIVED:
            NRF_LOG_DEBUG("RX RECEIVED EVENT");

            bsp_board_led_invert(BSP_BOARD_LED_0);

            //following code disabled to avoid stalling ISR
            // --> rx_payload gets overwritten for newly received frames, even if former one hasn't
            // been processed
/*
            while (processing_rf_frame > 0) {
                //busy wait
            }
*/            
            

            if (processing_rf_frame != 0) {
                bsp_board_led_invert(BSP_BOARD_LED_1); //Toggle of LED 1 indicates processing of RF frames takes too long
//                nrf_esb_flush_rx();
                break;
            }

            processing_rf_frame = 1;
/*
            if (nrf_esb_read_rx_payload(&rx_payload) == NRF_SUCCESS)
            {

                processing_rf_frame = rx_payload.length;
                
                // Set LEDs identical to the ones on the PTX.
                nrf_gpio_pin_write(LED_1, !(rx_payload.data[1]%8>0 && rx_payload.data[1]%8<=4));
                nrf_gpio_pin_write(LED_2, !(rx_payload.data[1]%8>1 && rx_payload.data[1]%8<=5));
                nrf_gpio_pin_write(LED_3, !(rx_payload.data[1]%8>2 && rx_payload.data[1]%8<=6));
#ifndef BOARD_CUSTOM                
                nrf_gpio_pin_write(LED_4, !(rx_payload.data[1]%8>3));
#endif
                NRF_LOG_DEBUG("Receiving packet: %02x", rx_payload.data[1]);
            }
            //nrf_esb_flush_rx();        
*/         
            break;
    }
}

uint32_t esb_init( void )
{
    //bb:0a:dc:a5:75

    uint32_t err_code;
    //uint8_t base_addr_0[4] = {0xa5, 0xdc, 0x0a, 0xbb};
    //uint8_t base_addr_1[4] = {0xC2, 0xC2, 0xC2, 0xC2};
    //uint8_t addr_prefix[8] = {0x75, 0xc1, 0xc2, 0xc3, 0xc4, 0xC5, 0xC6, 0xC7};

    //good addr+prefix combos: 0xaaaa, 
//    uint8_t base_addr_0[4] = {0xaa, 0xaa, 0xaa, 0xaa};
//    uint8_t base_addr_1[4] = {0x55, 0x55, 0x55, 0x55};
//    uint8_t addr_prefix[8] = {0xaa, 0xaf, 0x1f, 0x2f, 0x5f, 0xaa, 0xfa, 0x0a};
    
    uint8_t base_addr_0[4] = {0xa8, 0xa8, 0xa8, 0xa8}; //only one octet used in "illegal" mode
    uint8_t base_addr_1[4] = {0xaa, 0xaa, 0xaa, 0xaa}; //only one octet used in "illegal" mode
    uint8_t addr_prefix[8] = {0xaa, 0x1f, 0x9f, 0xa8, 0xaf, 0xa9, 0x8f, 0xaa};
    
    nrf_esb_config_t nrf_esb_config         = NRF_ESB_ILLEGAL_CONFIG;
    nrf_esb_config.event_handler            = nrf_esb_event_handler;
 
    
 
    err_code = nrf_esb_init(&nrf_esb_config);
    VERIFY_SUCCESS(err_code);

    err_code = nrf_esb_set_base_address_0(base_addr_0);
    VERIFY_SUCCESS(err_code);

    err_code = nrf_esb_set_base_address_1(base_addr_1);
    VERIFY_SUCCESS(err_code);

    err_code = nrf_esb_set_prefixes(addr_prefix, 8);
    VERIFY_SUCCESS(err_code);

    err_code = nrf_esb_set_rf_channel(5);
    VERIFY_SUCCESS(err_code);


    return err_code;
}

void clocks_start( void )
{
    NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
    NRF_CLOCK->TASKS_HFCLKSTART = 1;

    while (NRF_CLOCK->EVENTS_HFCLKSTARTED == 0);
}

bool check_crc16(uint8_t * p_array, uint8_t len) {
    //remove this hack, only for comparison
    uint16_t crc = crc16_compute(p_array, (uint32_t) len, NULL);

    if (crc == 0x0000) {
        return true;
    }

    return false;
}

bool validate_esb_frame(uint8_t * p_array, uint8_t addrlen) {
    uint8_t framelen = p_array[addrlen] >> 2;
    if (framelen > 32) {
        return false; // early out 
    }
    uint8_t crclen = addrlen + 1 + framelen + 2; //+1 for PCF (we ignore one bit), +2 for crc16

    return check_crc16(p_array, crclen);
}

void array_shl(uint8_t *p_array, uint8_t len, uint8_t bits) {
    if (len == 1) {
        p_array[0] = p_array[0] << bits;
        return;
    }
    
    for (uint8_t i=0; i<(len-1); i++) {
        p_array[i] = p_array[i] << bits | p_array[i+1] >> (8-bits);
    }
    p_array[len-1] = p_array[len-1] << bits;
    return;
}

int main(void)
{
    bool report_frames_without_crc_match = false;

    ret_code_t ret;
    static const app_usbd_config_t usbd_config = {
        .ev_state_proc = usbd_user_ev_handler
    };

    ret = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(ret);

    ret = nrf_drv_clock_init();
    APP_ERROR_CHECK(ret);

    nrf_drv_clock_lfclk_request(NULL);

    while(!nrf_drv_clock_lfclk_is_running())
    {
        /* Just waiting */
    }

    ret = app_timer_init();
    APP_ERROR_CHECK(ret);


    init_bsp();

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

    ret = esb_init();
    APP_ERROR_CHECK(ret);

    ret = nrf_esb_start_rx();
    APP_ERROR_CHECK(ret);

    while (true)
    {
        while (app_usbd_event_queue_process())
        {
            /* Nothing to do */
        }
        //hid_send_in_report_if_state_changed();

        if (processing_rf_frame != 0) {
            //we check current channel here, which isn't reliable as the frame from fifo could have been received on a
            // different one, but who cares
            uint32_t ch = 0;
            nrf_esb_get_rf_channel(&ch);

            while (nrf_esb_read_rx_payload(&rx_payload) == NRF_SUCCESS) {
                static uint8_t assumed_addrlen = 5;
                static uint8_t report[REPORT_IN_MAXSIZE];
                static bool crcmatch = false;

                for (uint8_t i=0; i<rx_payload.length; i++) {
                    report[i+3] = rx_payload.data[i];
                }
                report[0] = rx_payload.pipe;
                report[1] = (uint8_t) ch;
                report[2] = rx_payload.length;

                // if processing takes too long RF frames are discarded
                // the shift value in the following for loop controls how often a received
                // frame is shifted for CRC check. If this value is too large, frames are dropped,
                // if it is too low, chance for detecting valid frames decreases.
                // The validate_esb_frame function has an early out, if determined ESB frame length
                // exceeds 32 byte, which avoids unnecessary CRC16 calculations.
                crcmatch = false;
                for (uint8_t shift=0; shift<32; shift++) {
                    if (validate_esb_frame(&report[3], assumed_addrlen)) {
                        report[0] |= 0x40;
                        crcmatch = true;
                        break;
                    }
                    array_shl(&report[3], report[2], 1);
                }

                if (crcmatch) {
                    uint8_t esb_len = report[3+assumed_addrlen] >> 2;

                    bsp_board_led_invert(BSP_BOARD_LED_2); // toggle led 2 to indicate frame with valid checksum


                    //correct RF frame length if CRC match
                    report[2] = esb_len;

                    //byte allign payload (throw away no_ack bit of PCF, keep the other 8 bits)
                    array_shl(&report[3+assumed_addrlen+1], esb_len, 1);

                    //zero out rest of report
                    for (uint8_t i= 3 + assumed_addrlen + 1 + esb_len + 2; i<REPORT_IN_MAXSIZE; i++) {
                        report[i] = 0x00;
                    }
                }

                if (report_frames_without_crc_match || crcmatch) {
                    app_usbd_hid_generic_in_report_set(&m_app_hid_generic, report, sizeof(report));
                }
            }
            processing_rf_frame = 0; // free to receive more
        }

        UNUSED_RETURN_VALUE(NRF_LOG_PROCESS());
        /* Sleep CPU only if there was no interrupt since last loop processing */
        __WFE();
    }
}
