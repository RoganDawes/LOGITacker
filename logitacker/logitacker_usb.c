#include "app_usbd_hid_kbd.h"
#include "app_usbd_hid_kbd_desc.h"
#include "app_usbd_hid_mouse.h"
#include "logitacker_usb.h"
#include "app_usbd_hid_generic.h"
#include "logitacker_bsp.h"
#include "nrf_cli_cdc_acm.h"
#include "logitacker_radio.h"
#include "ringbuf.h"

#define NRF_LOG_MODULE_NAME LOGITACKER_USB

#include "nrf_log.h"
#include "logitacker_script_engine.h"
#include "logitacker_options.h"


NRF_LOG_MODULE_REGISTER();

uint8_t tmp_in_rep_buf[4][LOGITACKER_USB_HID_GENERIC_IN_REPORT_MAXSIZE];
int tmp_in_rep_buf_current;

//static ret_code_t idle_handle(app_usbd_class_inst_t const * p_inst, uint8_t report_id);
static void usbd_device_event_handler(app_usbd_event_type_t event);

static void usbd_hid_generic_event_handler(app_usbd_class_inst_t const *p_inst, app_usbd_hid_user_event_t event);

static void usbd_hid_keyboard_event_handler(app_usbd_class_inst_t const *p_inst, app_usbd_hid_user_event_t event);

static void usbd_hid_mouse_event_handler(app_usbd_class_inst_t const *p_inst, app_usbd_hid_user_event_t event);

uint32_t logitacker_usb_read_hidraw_output_report();

// created HID report descriptor with vendor define output / input report of max size in raw_desc
APP_USBD_HID_GENERIC_SUBCLASS_REPORT_DESC(raw_desc, APP_USBD_HID_RAW_REPORT_DSC_SIZE(
        LOGITACKER_USB_HID_GENERIC_OUT_REPORT_MAXSIZE));
static const app_usbd_hid_subclass_desc_t *hid_report_descriptors_raw_device[] = {&raw_desc};

// report descriptors for keyboard
APP_USBD_HID_GENERIC_SUBCLASS_REPORT_DESC(kbd_desc, APP_USBD_HID_KBD_REPORT_DSC());
static const app_usbd_hid_subclass_desc_t *hid_report_descriptors_keyboard_device[] = {&kbd_desc};

// report descriptors for mouse (16 buttons, 12 bit x-/y-axis, 8bit wheel, AC Pan)
APP_USBD_HID_GENERIC_SUBCLASS_REPORT_DESC(mouse_desc, APP_USBD_HID_MOUSE_12BIT_REPORT_DSC());
static const app_usbd_hid_subclass_desc_t *hid_report_descriptors_mouse_device[] = {&mouse_desc};

/* USB HID INTERFACES */
// setup generic HID interface
APP_USBD_HID_GENERIC_GLOBAL_DEF(m_app_hid_generic,
                                LOGITACKER_USB_HID_GENERIC_INTERFACE,
                                usbd_hid_generic_event_handler,
                                LOGITACKER_USB_HID_GENERIC_INTERFACE_ENDPOINT_LIST(),
                                hid_report_descriptors_raw_device,
                                LOGITACKER_USB_HID_GENERIC_REPORT_IN_QUEUE_SIZE,
                                LOGITACKER_USB_HID_GENERIC_OUT_REPORT_MAXSIZE,
                                APP_USBD_HID_SUBCLASS_BOOT,
                                APP_USBD_HID_PROTO_GENERIC);

// setup HID keyboard interface (as generic interface)

APP_USBD_HID_GENERIC_GLOBAL_DEF(m_app_hid_keyboard,
                                LOGITACKER_USB_HID_KEYBOARD_INTERFACE,
                                usbd_hid_keyboard_event_handler,
                                LOGITACKER_USB_HID_KEYBOARD_INTERFACE_ENDPOINT_LIST(),
                                hid_report_descriptors_keyboard_device,
                                LOGITACKER_USB_HID_KEYBOARD_REPORT_IN_QUEUE_SIZE,
                                LOGITACKER_USB_HID_KEYBOARD_OUT_REPORT_MAXSIZE,
                                APP_USBD_HID_SUBCLASS_BOOT,
                                APP_USBD_HID_PROTO_KEYBOARD);

/*
APP_USBD_HID_KBD_GLOBAL_DEF(m_app_hid_keyboard,
                            LOGITACKER_USB_HID_KEYBOARD_INTERFACE,
                            LOGITACKER_USB_HID_KEYBOARD_EPIN,
                            usbd_hid_keyboard_event_handler,
                            APP_USBD_HID_SUBCLASS_BOOT);
*/


// setup HID mouse interface
APP_USBD_HID_GENERIC_GLOBAL_DEF(m_app_hid_mouse,
                                LOGITACKER_USB_HID_MOUSE_INTERFACE,
                                usbd_hid_mouse_event_handler,
                                LOGITACKER_USB_HID_MOUSE_INTERFACE_ENDPOINT_LIST(),
                                hid_report_descriptors_mouse_device,
                                LOGITACKER_USB_HID_MOUSE_REPORT_IN_QUEUE_SIZE,
                                1,
                                APP_USBD_HID_SUBCLASS_BOOT,
                                APP_USBD_HID_PROTO_MOUSE);

/*
APP_USBD_HID_MOUSE_GLOBAL_DEF(m_app_hid_mouse,
                              LOGITACKER_USB_HID_MOUSE_INTERFACE,
                              LOGITACKER_USB_HID_MOUSE_EPIN,
                              LOGITACKER_USB_HID_MOUSE_BUTTON_COUNT,
                              usbd_hid_mouse_event_handler,
                              APP_USBD_HID_SUBCLASS_BOOT);
*/



static uint8_t m_keyboard_hid_input_report[LOGITACKER_USB_HID_KEYBOARD_IN_REPORT_MAXSIZE];
static uint8_t m_mouse_hid_input_report[LOGITACKER_USB_HID_MOUSE_IN_REPORT_MAXSIZE];

// app_usbd configuration
static const app_usbd_config_t usbd_config = {
        .ev_state_proc = usbd_device_event_handler       // user defined event processor for USB device state change
};


static void usbd_device_event_handler(app_usbd_event_type_t event) {
    //runs in thread mode

    switch (event) {
        case APP_USBD_EVT_DRV_SOF:
            break;
        case APP_USBD_EVT_DRV_RESET:
            break;
        case APP_USBD_EVT_DRV_SUSPEND:
            app_usbd_suspend_req(); // Allow the library to put the peripheral into sleep mode
            bsp_board_led_off(LED_R);
            break;
        case APP_USBD_EVT_DRV_RESUME:
            bsp_board_led_on(LED_R);
            break;
        case APP_USBD_EVT_STARTED:
            bsp_board_led_on(LED_R);
            break;
        case APP_USBD_EVT_STOPPED:
            app_usbd_disable();
            bsp_board_led_off(LED_R);
            break;
        case APP_USBD_EVT_POWER_DETECTED:
            NRF_LOG_INFO("USB power detected");
            if (!nrf_drv_usbd_is_enabled()) {
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


static void usbd_hid_generic_event_handler(app_usbd_class_inst_t const *p_inst, app_usbd_hid_user_event_t event) {
    //helper_log_priority("usbd_hid_generic_event_handler");
    switch (event) {
        case APP_USBD_HID_USER_EVT_OUT_REPORT_READY: {
            logitacker_usb_read_hidraw_output_report();
            /*
            NRF_LOG_INFO("hid generic raw evt: APP_USBD_HID_USER_EVT_OUT_REPORT_READY");
            size_t out_rep_size = LOGITACKER_USB_HID_GENERIC_OUT_REPORT_MAXSIZE;
            const uint8_t* out_rep = app_usbd_hid_generic_out_report_get(&m_app_hid_generic, &out_rep_size);
            NRF_LOG_HEXDUMP_INFO(out_rep, out_rep_size);
//            logitacker_usb_write_generic_input_report(out_rep, out_rep_size);

//            memcpy(&hid_out_report, out_rep, LOGITACKER_USB_HID_GENERIC_OUT_REPORT_MAXSIZE);
//            processing_hid_out_report = true;
             */
            break;
        }
        case APP_USBD_HID_USER_EVT_IN_REPORT_DONE: {
            NRF_LOG_DEBUG("HID raw report sent successfully")

            break;
        }
        case APP_USBD_HID_USER_EVT_SET_BOOT_PROTO: {
            UNUSED_RETURN_VALUE(hid_generic_clear_buffer(p_inst));
            NRF_LOG_INFO("SET_BOOT_PROTO");
            break;
        }
        case APP_USBD_HID_USER_EVT_SET_REPORT_PROTO: {
            UNUSED_RETURN_VALUE(hid_generic_clear_buffer(p_inst));
            NRF_LOG_INFO("SET_REPORT_PROTO");
            break;
        }
        default:
            break;
    }
}

//static bool m_append_key_release;
static bool currently_sending_keyboard_input;

static void usbd_hid_keyboard_event_handler(app_usbd_class_inst_t const *p_inst, app_usbd_hid_user_event_t event) {
    switch (event) {
        case APP_USBD_HID_USER_EVT_OUT_REPORT_READY: {
            size_t out_rep_size = LOGITACKER_USB_HID_KEYBOARD_OUT_REPORT_MAXSIZE;
            const uint8_t *out_rep = app_usbd_hid_generic_out_report_get(&m_app_hid_keyboard, &out_rep_size);
            NRF_LOG_INFO("hid kbd evt: APP_USBD_HID_USER_EVT_OUT_REPORT_READY");
            NRF_LOG_HEXDUMP_INFO(out_rep, out_rep_size);

            g_logitacker_global_runtime_state.usb_led_out_report_count++; //indicate that the initial USB report was received
            g_logitacker_global_runtime_state.usb_led_out_report_count &= 0x7f; //avoid overrun of counter

            // if bootmode is USB injection && inject trigger is LED_update && injection wasn't executed --> inject now
/*
            if (g_logitacker_global_config.bootmode == OPTION_LOGITACKER_BOOTMODE_USB_INJECT &&
                g_logitacker_global_config.usbinject_trigger == OPTION_LOGITACKER_USBINJECT_TRIGGER_ON_LEDUPDATE &&
                !g_logitacker_global_runtime_state.usb_inject_script_triggered &&
                g_logitacker_global_runtime_state.usb_led_out_report_count > 0 &&
                g_logitacker_global_runtime_state.usb_led_out_report_count < 3) {

                //logitacker_script_engine_append_task_type_string("event");
                logitacker_enter_mode_injection((uint8_t[5]) {0, 0, 0, 0, 0});
                logitacker_injection_start_execution(true);
                g_logitacker_global_runtime_state.usb_inject_script_triggered = true;
                bsp_board_led_on(LED_G);
            }
*/
            if (g_logitacker_global_config.bootmode == OPTION_LOGITACKER_BOOTMODE_USB_INJECT &&
                g_logitacker_global_config.usbinject_trigger == OPTION_LOGITACKER_USBINJECT_TRIGGER_ON_LEDUPDATE) {

                //logitacker_script_engine_append_task_type_string("event");
                logitacker_enter_mode_injection((uint8_t[5]) {0, 0, 0, 0, 0});
                logitacker_injection_start_execution(true);
            }

            /*
            m_keyboard_hid_input_report[2] = 4 + out_rep[1]; // move LED state + 4 (HID_KEY_A) to keyboard input report
            logitacker_usb_write_keyboard_input_report(m_keyboard_hid_input_report);
            m_append_key_release = true;
             */
            break;
        }
        case APP_USBD_HID_USER_EVT_IN_REPORT_DONE: {
            currently_sending_keyboard_input = false;
            /*
            if (m_append_key_release) {
                memset(m_keyboard_hid_input_report,0,LOGITACKER_USB_HID_KEYBOARD_IN_REPORT_MAXSIZE);
                logitacker_usb_write_keyboard_input_report(m_keyboard_hid_input_report); // write key release
                m_append_key_release = false;
            }
            */
            NRF_LOG_DEBUG("hid kbd evt: APP_USBD_HID_USER_EVT_IN_REPORT_DONE");
            break;
        }
        case APP_USBD_HID_USER_EVT_SET_BOOT_PROTO: {
            UNUSED_RETURN_VALUE(hid_generic_clear_buffer(p_inst));
            NRF_LOG_INFO("hid kbd evt: APP_USBD_HID_USER_EVT_SET_BOOT_PROTO");
            break;
        }
        case APP_USBD_HID_USER_EVT_SET_REPORT_PROTO: {
            UNUSED_RETURN_VALUE(hid_generic_clear_buffer(p_inst));
            NRF_LOG_INFO("hid kbd evt: APP_USBD_HID_USER_EVT_SET_REPORT_PROTO");
            break;
        }
        default:
            break;
    }

    main_usbd_hid_keyboard_event_handler(p_inst, event); //pass-through to logitacker.c to use in processors
}

static bool currently_sending_mouse_input;

static void usbd_hid_mouse_event_handler(app_usbd_class_inst_t const *p_inst, app_usbd_hid_user_event_t event) {
    switch (event) {
        case APP_USBD_HID_USER_EVT_OUT_REPORT_READY: {
            NRF_LOG_INFO("hid mouse evt: APP_USBD_HID_USER_EVT_OUT_REPORT_READY");
            break;
        }
        case APP_USBD_HID_USER_EVT_IN_REPORT_DONE: {
            currently_sending_mouse_input = false;
            NRF_LOG_INFO("hid mouse evt: APP_USBD_HID_USER_EVT_IN_REPORT_DONE");
            break;
        }
        case APP_USBD_HID_USER_EVT_SET_BOOT_PROTO: {
            UNUSED_RETURN_VALUE(hid_generic_clear_buffer(p_inst));
            NRF_LOG_INFO("hid mouse evt: APP_USBD_HID_USER_EVT_SET_BOOT_PROTO");
            break;
        }
        case APP_USBD_HID_USER_EVT_SET_REPORT_PROTO: {
            UNUSED_RETURN_VALUE(hid_generic_clear_buffer(p_inst));
            NRF_LOG_INFO("hid mouse evt: APP_USBD_HID_USER_EVT_SET_REPORT_PROTO");
            break;
        }
        default:
            break;
    }
}

uint32_t logitacker_usb_init() {
    uint32_t ret;
    ret = app_usbd_init(&usbd_config);
    VERIFY_SUCCESS(ret);

    // Note: configured using sdk_config for cli_cdc_acm
    //   #define NRF_CLI_CDC_ACM_COMM_INTERFACE 0
    //   #define NRF_CLI_CDC_ACM_COMM_EPIN NRF_DRV_USBD_EPIN2
    //   #define NRF_CLI_CDC_ACM_DATA_INTERFACE 1
    //   #define NRF_CLI_CDC_ACM_DATA_EPIN NRF_DRV_USBD_EPIN1
    //   #define NRF_CLI_CDC_ACM_DATA_EPOUT NRF_DRV_USBD_EPOUT1
    app_usbd_class_inst_t const *class_cdc_acm = app_usbd_cdc_acm_class_inst_get(&nrf_cli_cdc_acm);
    ret = app_usbd_class_append(class_cdc_acm);
    VERIFY_SUCCESS(ret);

    // Note: configured in logitacker_usb.h
    //   #define LOGITACKER_USB_HID_GENERIC_INTERFACE  2                        // HID generic class interface number.
    //   #define LOGITACKER_USB_HID_GENERIC_EPIN       NRF_DRV_USBD_EPIN3       // HID generic class endpoint number.
    app_usbd_class_inst_t const *class_inst_hid_generic = app_usbd_hid_generic_class_inst_get(&m_app_hid_generic);
    ret = app_usbd_class_append(class_inst_hid_generic);
    VERIFY_SUCCESS(ret);

    // Note: configured in logitacker_usb.h
    //   #define LOGITACKER_USB_HID_KEYBOARD_INTERFACE  3
    //   #define LOGITACKER_USB_HID_KEYBOARD_EPIN       NRF_DRV_USBD_EPIN4
    //app_usbd_class_inst_t const * class_inst_hid_keyboard = app_usbd_hid_kbd_class_inst_get(&m_app_hid_keyboard);
    app_usbd_class_inst_t const *class_inst_hid_keyboard = app_usbd_hid_generic_class_inst_get(&m_app_hid_keyboard);
    ret = app_usbd_class_append(class_inst_hid_keyboard);
    VERIFY_SUCCESS(ret);

    // Note: configured in logitacker_usb.h
    //   #define LOGITACKER_USB_HID_MOUSE_INTERFACE  4
    //   #define LOGITACKER_USB_HID_MOUSE_EPIN       NRF_DRV_USBD_EPIN5
    //app_usbd_class_inst_t const * class_inst_hid_mouse = app_usbd_hid_mouse_class_inst_get(&m_app_hid_mouse);
    app_usbd_class_inst_t const *class_inst_hid_mouse = app_usbd_hid_generic_class_inst_get(&m_app_hid_mouse);
    ret = app_usbd_class_append(class_inst_hid_mouse);
    VERIFY_SUCCESS(ret);


    if (USBD_POWER_DETECTION) {
        ret = app_usbd_power_events_enable();
        APP_ERROR_CHECK(ret);
    } else {
        NRF_LOG_INFO("No USB power detection enabled\r\nStarting USB now");

        app_usbd_enable();
        app_usbd_start();
    }


    return NRF_SUCCESS;
}


uint32_t logitacker_usb_write_keyboard_input_report(const void *p_buf) {
    VERIFY_FALSE(currently_sending_keyboard_input, NRF_ERROR_BUSY);
    currently_sending_keyboard_input = true;
    memcpy(m_keyboard_hid_input_report, p_buf, LOGITACKER_USB_HID_KEYBOARD_IN_REPORT_MAXSIZE);

    // write report
    return app_usbd_hid_generic_in_report_set(&m_app_hid_keyboard, m_keyboard_hid_input_report,
                                              LOGITACKER_USB_HID_KEYBOARD_IN_REPORT_MAXSIZE);
}

uint32_t logitacker_usb_write_mouse_input_report(const void *p_buf) {
    VERIFY_FALSE(currently_sending_mouse_input, NRF_ERROR_BUSY);
    currently_sending_mouse_input = true;
    memcpy(m_mouse_hid_input_report, p_buf, LOGITACKER_USB_HID_MOUSE_IN_REPORT_MAXSIZE);

    // write report
    NRF_LOG_HEXDUMP_INFO(m_mouse_hid_input_report, 8);
    return app_usbd_hid_generic_in_report_set(&m_app_hid_mouse, m_mouse_hid_input_report,
                                              LOGITACKER_USB_HID_MOUSE_IN_REPORT_MAXSIZE);
}

uint32_t logitacker_usb_read_hidraw_output_report() {
    size_t len = LOGITACKER_USB_HID_GENERIC_OUT_REPORT_MAXSIZE;
    const uint8_t *p_report = app_usbd_hid_generic_out_report_get(&m_app_hid_generic, &len);

    NRF_LOG_HEXDUMP_INFO(p_report, len)
    logitacker_usb_hidraw_report_type_t type = p_report[1];
    loigtacker_usb_hidraw_command_t command = p_report[2];

    NRF_LOG_INFO("type: %02x, command: %02x\n", type, command);

    switch (type) {
        case LOGITACKER_USB_HIDRAW_REPORT_TYPE_COMMAND: {
            switch (command) {
                case LOGITACKER_USB_HIDRAW_COMMAND_SCRIPT_CLEAR: {
                    logitacker_script_engine_flush_tasks();
                    break;
                }
                case LOGITACKER_USB_HIDRAW_COMMAND_SCRIPT_STRING: {
                    size_t slen = p_report[3];
                    char buf[61] = {0};
                    if (slen > 60) slen = 60;
                    memcpy(buf, &p_report[4], slen);
                    logitacker_script_engine_append_task_type_string(buf);
                    break;
                }
                case LOGITACKER_USB_HIDRAW_COMMAND_SCRIPT_PRESS: {
                    size_t slen = p_report[3];
                    char buf[61] = {0};
                    if (slen > 60) slen = 60;
                    memcpy(buf, &p_report[4], slen);
                    logitacker_script_engine_append_task_press_combo(buf);
                    break;
                }
                case LOGITACKER_USB_HIDRAW_COMMAND_SCRIPT_ALTSTRING: {
                    size_t slen = p_report[3];
                    char buf[61] = {0};
                    if (slen > 60) slen = 60;
                    memcpy(buf, &p_report[4], slen);
                    logitacker_script_engine_append_task_type_altstring(buf);
                    break;
                }
                case LOGITACKER_USB_HIDRAW_COMMAND_SCRIPT_DELAY: {
                    uint32_t delay;
                    memcpy(&delay, &p_report[3], 4);
                    logitacker_script_engine_append_task_delay(delay);
                    break;
                }
                default:
                    NRF_LOG_WARNING("Unhandled raw report command %02x\n", command)
            }
            break;
        }
        default:
            NRF_LOG_WARNING("Unhandled raw report type %02x\n", type)
    }

    return NRF_SUCCESS;
}

uint32_t
logitacker_usb_write_hidraw_input_report(logitacker_mode_t logitacker_mode, logitacker_usb_hidraw_report_type_t type,
                                         size_t length, const void *data) {

    //prepare data
    uint8_t *tmp_buf = tmp_in_rep_buf[tmp_in_rep_buf_current++];
    tmp_in_rep_buf_current &= 0x3; // mod 4
    memset(tmp_buf, 0, LOGITACKER_USB_HID_GENERIC_IN_REPORT_MAXSIZE);
    tmp_buf[0] = type;
    tmp_buf[1] = (uint8_t) logitacker_mode;
    memcpy(&tmp_buf[2], data, length);


    return app_usbd_hid_generic_in_report_set(&m_app_hid_generic, tmp_buf,
                                              LOGITACKER_USB_HID_GENERIC_IN_REPORT_MAXSIZE);
}

uint32_t logitacker_usb_write_hidraw_input_report_rf_frame(logitacker_mode_t logitacker_mode,
                                                           logitacker_devices_unifying_device_rf_address_t rf_address,
                                                           const nrf_esb_payload_t *p_frame) {
    VERIFY_PARAM_NOT_NULL(rf_address);
    VERIFY_PARAM_NOT_NULL(p_frame);

    nrf_esb_payload_t frame = {0};

    // ToDo: remove unneeded overhead, from redundant memcpy's
    if (p_frame->validated_promiscuous_frame) {
        // convert data
        logitacker_radio_convert_promiscuous_frame_to_default_frame(&frame, *p_frame);
    } else {
        memcpy(&frame, p_frame, sizeof(nrf_esb_payload_t));
    }

    uint8_t framelen = frame.length > 32 ? 32 : frame.length;
    logitacker_usb_hidraw_rf_frame_representation_t frame_hid_pay = {};
    memcpy(frame_hid_pay.rf_address, rf_address, 5);
    frame_hid_pay.payload_length = framelen;
    frame_hid_pay.rf_channel = frame.rx_channel;
    memcpy(frame_hid_pay.payload_data, frame.data, framelen);
    //NRF_LOG_HEXDUMP_INFO(frame_hid_pay.payload_data, frame_hid_pay.payload_length);
    frame_hid_pay.pid = frame.pid;
    frame_hid_pay.rssi = frame.rssi;

    return logitacker_usb_write_hidraw_input_report(logitacker_mode, LOGITACKER_USB_HIDRAW_REPORT_TYPE_RF_FRAME,
                                                    sizeof(logitacker_usb_hidraw_rf_frame_representation_t),
                                                    &frame_hid_pay);
}
