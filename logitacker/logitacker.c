#include "nrf_cli.h"
#include "logitacker.h"
#include "logitacker_bsp.h"
#include "logitacker_radio.h"
#include "logitacker_devices.h"
#include "nrf.h"
#include "nrf_esb_illegalmod.h"
#include "bsp.h"
#include "logitacker_unifying.h"
#include "helper.h"
#include "app_timer.h"
#include "logitacker_pairing_parser.h"
#include "logitacker_unifying_crypto.h"
#include "logitacker_keyboard_map.h"
#include "logitacker_processor.h"
#include "logitacker_processor_active_enum.h"
#include "logitacker_processor_passive_enum.h"
#include "logitacker_processor_prx.h"
#include "logitacker_processor_pair_device.h"
#include "logitacker_processor_inject.h"
#include "logitacker_usb.h"
#include "logitacker_options.h"
#include "utf.h"
#include "logitacker_flash.h"
#include "logitacker_processor_discover.h"
#include "logitacker_processor_pair_sniff.h"
#include "logitacker_script_engine.h"

#define NRF_LOG_MODULE_NAME LOGITACKER

#include "nrf_log.h"
#include "logitacker_processor_covert_channel.h"

NRF_LOG_MODULE_REGISTER();

APP_TIMER_DEF(m_timer_next_tx_action);

static logitacker_processor_t *p_processor = NULL;


typedef struct {
    bool initialized;
    logitacker_mode_t mainstate;
} logitacker_state_t;


logitacker_state_t m_state_local;

static bool m_main_event_handler_bsp_long_pushed;

void main_event_handler_timer_next_action(void *p_context);

void main_event_handler_esb(nrf_esb_evt_t *p_event);

void main_event_handler_radio(radio_evt_t const *p_event);

static void main_event_handler_bsp(bsp_event_t ev);

/* main/master event handler */
void main_usbd_hid_keyboard_event_handler(app_usbd_class_inst_t const *p_inst, app_usbd_hid_user_event_t event) {
    if (p_processor != NULL && p_processor->p_usb_hid_keyboard_event_handler != NULL)
        (*p_processor->p_usb_hid_keyboard_event_handler)(p_processor, p_inst, event);
}


void main_event_handler_timer_next_action(void *p_context) {
    if (p_processor != NULL && p_processor->p_timer_handler != NULL)
        (*p_processor->p_timer_handler)(p_processor,
                                        p_context); // call timer handler function of p_processor and hand in p_processor (self) as first arg
}

void main_event_handler_esb(nrf_esb_evt_t *p_event) {
    switch (p_event->evt_id) {
        case NRF_ESB_EVENT_TX_SUCCESS:
            NRF_LOG_DEBUG("ESB EVENT HANDLER MAIN TX_SUCCESS");
            break;
        case NRF_ESB_EVENT_TX_SUCCESS_ACK_PAY:
            NRF_LOG_DEBUG("ESB EVENT HANDLER MAIN TX_SUCCESS_ACK_PAY");
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

    if (p_processor != NULL && p_processor->p_esb_handler != NULL)
        (*p_processor->p_esb_handler)(p_processor,
                                      p_event); // call ESB handler function of p_processor and hand in p_processor (self) as first arg
}

void main_event_handler_radio(radio_evt_t const *p_event) {
    //helper_log_priority("UNIFYING_event_handler");
    switch (p_event->evt_id) {
        case RADIO_EVENT_NO_RX_TIMEOUT: {
            NRF_LOG_DEBUG("RADIO EVENT HANDLER MAIN: RX TIMEOUT");
            break;
        }
        case RADIO_EVENT_CHANNEL_CHANGED_FIRST_INDEX: {
            NRF_LOG_DEBUG("RADIO EVENT HANDLER MAIN: CHANNEL CHANGED FIRST INDEX");
            break;
        }
        case RADIO_EVENT_CHANNEL_CHANGED: {
            NRF_LOG_DEBUG("RADIO EVENT HANDLER MAIN: CHANNEL CHANGED (index %d, channel freq %d)",
                          p_event->channel_index, p_event->channel);
            break;
        }
    }

    if (p_processor != NULL && p_processor->p_radio_handler != NULL)
        (*p_processor->p_radio_handler)(p_processor,
                                        p_event); // call ESB handler function of p_processor and hand in p_processor (self) as first arg
}

static void main_event_handler_bsp(bsp_event_t ev) {
    // runs in interrupt mode
    //helper_log_priority("bsp_event_callback");
    //uint32_t ret;
    switch ((unsigned int) ev) {
        case CONCAT_2(BSP_EVENT_KEY_, BTN_TRIGGER_ACTION):
            //Toggle radio back to promiscous mode
            NRF_LOG_INFO("ACTION button pushed");
            bsp_board_led_on(LED_B);
            break;

        case BSP_USER_EVENT_LONG_PRESS_0:
            m_main_event_handler_bsp_long_pushed = true;
            NRF_LOG_INFO("ACTION BUTTON PUSHED LONG");
            break;

        case BSP_USER_EVENT_RELEASE_0:
            if (m_main_event_handler_bsp_long_pushed) {
                m_main_event_handler_bsp_long_pushed = false;
                break; // don't act if there was already a long press event
            }
            NRF_LOG_INFO("ACTION BUTTON RELEASED");
            break;

        default:
            break; // no implementation needed
    }

    if (p_processor != NULL && p_processor->p_bsp_handler != NULL)
        (*p_processor->p_bsp_handler)(p_processor,
                                      ev); // call BSP handler function of p_processor and hand in p_processor (self) as first arg
}


// Transfers execution to active_enum_process_sub_event
void logitacker_enter_mode_passive_enum(uint8_t *rf_address) {
    if (p_processor != NULL && p_processor->p_deinit_func != NULL) (*p_processor->p_deinit_func)(p_processor);
    p_processor = new_processor_passive_enum(rf_address);
    p_processor->p_init_func(p_processor);

    m_state_local.mainstate = LOGITACKER_MODE_PASSIVE_ENUMERATION;
    sprintf(g_logitacker_cli_name, "LOGITacker (passive enum) $ ");
}

void logitacker_enter_mode_prx(uint8_t *rf_address) {
    if (p_processor != NULL && p_processor->p_deinit_func != NULL) (*p_processor->p_deinit_func)(p_processor);
    p_processor = new_processor_prx(rf_address);
    p_processor->p_init_func(p_processor);

    m_state_local.mainstate = LOGITACKER_MODE_PRX;
    sprintf(g_logitacker_cli_name, "LOGITacker (prx) $ ");
}


void logitacker_enter_mode_pair_sniff() {
    if (p_processor != NULL && p_processor->p_deinit_func != NULL) (*p_processor->p_deinit_func)(p_processor);

    p_processor = new_processor_pair_sniff(m_timer_next_tx_action);
    p_processor->p_init_func(p_processor);

    m_state_local.mainstate = LOGITACKER_MODE_SNIFF_PAIRING;
    sprintf(g_logitacker_cli_name, "LOGITacker (sniff pairing) $ ");
}


void logitacker_enter_mode_active_enum(uint8_t *rf_address) {
    if (p_processor != NULL && p_processor->p_deinit_func != NULL) (*p_processor->p_deinit_func)(p_processor);

    p_processor = new_processor_active_enum(rf_address, m_timer_next_tx_action);
    p_processor->p_init_func(p_processor);

    m_state_local.mainstate = LOGITACKER_MODE_ACTIVE_ENUMERATION;
    sprintf(g_logitacker_cli_name, "LOGITacker (active enum) $ ");
}

void logitacker_enter_mode_covert_channel(uint8_t *rf_address, nrf_cli_t const *p_cli) {
    if (p_processor != NULL && p_processor->p_deinit_func != NULL) (*p_processor->p_deinit_func)(p_processor);

    p_processor = new_processor_covert_channel(rf_address, m_timer_next_tx_action, p_cli);
    p_processor->p_init_func(p_processor);

    m_state_local.mainstate = LOGITACKER_MODE_COVERT_CHANNEL;
    //sprintf(g_logitacker_cli_name, "LOGITacker (covert channel) $ ");
    sprintf(g_logitacker_cli_name, " "); // empty prompt, use the one from bound shell
}

static uint8_t temp_dev_id = 1;

void logitacker_enter_mode_pair_device(uint8_t const *rf_address) {
    if (p_processor != NULL && p_processor->p_deinit_func != NULL) (*p_processor->p_deinit_func)(p_processor);

    char dev_name[] = "LOGITacker";
    logitacker_pairing_info_t pi = {
            .device_name_len = sizeof(dev_name),
            .device_usability_info = LOGITACKER_DEVICE_USABILITY_INFO_PS_LOCATION_ON_THE_TOP_EDGE,
            .device_nonce = {0xde, 0xad, 0xbe, 0xef},
            .device_report_types = LOGITACKER_DEVICE_REPORT_TYPES_KEYBOARD |
                                   LOGITACKER_DEVICE_REPORT_TYPES_POWER_KEYS |
                                   LOGITACKER_DEVICE_REPORT_TYPES_MULTIMEDIA |
                                   //                    LOGITACKER_DEVICE_REPORT_TYPES_MEDIA_CENTER |
                                   LOGITACKER_DEVICE_REPORT_TYPES_MOUSE |
                                   //                    LOGITACKER_DEVICE_REPORT_TYPES_SHORT_HIDPP |
                                   //                    LOGITACKER_DEVICE_REPORT_TYPES_LONG_HIDPP |
                                   LOGITACKER_DEVICE_REPORT_TYPES_KEYBOARD_LED,
            .device_serial = {0x2d, 0x9a, 0x9f, temp_dev_id++},
            .device_caps = LOGITACKER_DEVICE_CAPS_UNIFYING_COMPATIBLE |
                           LOGITACKER_DEVICE_CAPS_LINK_ENCRYPTION, // use link encryption, to account for MouseJack patched firmwares
            .device_type = LOGITACKER_DEVICE_UNIFYING_TYPE_KEYBOARD, // use keyboard, for latest receiver firmwares character keys [a-zA-Z] are blacklisted for non-keyboard devices
            .device_wpid = {0x13, 0x37}, // random
    };
    memcpy(pi.device_name, dev_name, pi.device_name_len);

    p_processor = new_processor_pair_device(rf_address, &pi, m_timer_next_tx_action);
    p_processor->p_init_func(p_processor);

    m_state_local.mainstate = LOGITACKER_MODE_PAIR_DEVICE;
    sprintf(g_logitacker_cli_name, "LOGITacker (pair device) $ ");

}

void logitacker_enter_mode_injection(uint8_t const *rf_address) {
    if (p_processor != NULL && p_processor->p_deinit_func != NULL) (*p_processor->p_deinit_func)(p_processor);

    p_processor = new_processor_inject(rf_address, m_timer_next_tx_action);
    p_processor->p_init_func(p_processor);

    m_state_local.mainstate = LOGITACKER_MODE_INJECT;
    sprintf(g_logitacker_cli_name, "LOGITacker (injection) $ ");
}

void logitacker_enter_mode_discovery() {
    if (p_processor != NULL && p_processor->p_deinit_func != NULL) (*p_processor->p_deinit_func)(p_processor);

    NRF_LOG_INFO("Entering discover mode");

    p_processor = new_processor_discover();
    p_processor->p_init_func(p_processor); //call init function

    m_state_local.mainstate = LOGITACKER_MODE_DISCOVERY;
    sprintf(g_logitacker_cli_name, "LOGITacker (discover) $ ");

}

void logitacker_quit_mode_discovery() {
// even tough this next line shouldn't be necessary...
    if (p_processor != NULL && p_processor->p_deinit_func != NULL) (*p_processor->p_deinit_func)(p_processor);
       NRF_LOG_INFO("Quitting discovery mode");

       p_processor = NULL;
       m_state_local.mainstate = LOGITACKER_MODE_IDLE;

    sprintf(g_logitacker_cli_name, "LOGITacker $ ");
}

void logitacker_injection_start_execution(bool execute) {
    if (m_state_local.mainstate != LOGITACKER_MODE_INJECT) {
        NRF_LOG_ERROR("Can't inject while not in injection mode");
        return;
    }

    logitacker_processor_inject_start_execution(p_processor, execute);
    if (execute) {
        NRF_LOG_INFO("Injection processing resumed");
    } else {
        NRF_LOG_INFO("Injection processing paused");
    }
}

uint32_t logitacker_covert_channel_push_data(covert_channel_payload_data_t const *p_tx_data) {
    if (m_state_local.mainstate != LOGITACKER_MODE_COVERT_CHANNEL) {
        NRF_LOG_ERROR("Can't push data, not in covert channel mode");
        return NRF_ERROR_FORBIDDEN;
    }

    return logitacker_processor_covert_channel_push_tx_data(p_processor, p_tx_data);
}

void clocks_start(void) {
    NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
    NRF_CLOCK->TASKS_HFCLKSTART = 1;

    while (NRF_CLOCK->EVENTS_HFCLKSTARTED == 0);
}

uint8_t rf_addr_usb[5] = {0};

uint32_t logitacker_init() {
    logitacker_options_init_state();
    sprintf(g_logitacker_cli_name, "LOGITacker $ ");
    logitacker_flash_init();
    logitacker_options_restore_from_flash(); // try to restore options from flash (updates stats like boot count)
    logitacker_options_store_to_flash(); // store back updated options


    clocks_start(); // HF clock needed by ESB radio part

    app_timer_create(&m_timer_next_tx_action, APP_TIMER_MODE_SINGLE_SHOT, main_event_handler_timer_next_action);

    //m_state_local.substate_discovery.on_new_address_action = OPTION_DISCOVERY_ON_NEW_ADDRESS_SWITCH_PASSIVE_ENUMERATION;
    logitacker_bsp_init(main_event_handler_bsp);
    logitacker_usb_init();
    logitacker_radio_init(main_event_handler_esb, main_event_handler_radio);

    // load default injection script
    if (strlen(g_logitacker_global_config.default_script) > 0)
        logitacker_script_engine_load_script_from_flash(g_logitacker_global_config.default_script);

    switch (g_logitacker_global_config.bootmode) {
        case OPTION_LOGITACKER_BOOTMODE_DISCOVER: {
            logitacker_enter_mode_discovery();
            break;
        }
        case OPTION_LOGITACKER_BOOTMODE_USB_INJECT: {
            //enter inject mode for address 00:00:00:00:00 == USB
            logitacker_enter_mode_injection(rf_addr_usb);
            // execute script if it should fire on boot
            if (g_logitacker_global_config.usbinject_trigger == OPTION_LOGITACKER_USBINJECT_TRIGGER_ON_POWERUP) {
                //execute injection
                logitacker_injection_start_execution(true);
            }

            break;
        }
        default: {
            logitacker_enter_mode_discovery(); //default for now
            break;
        }
    }


    return NRF_SUCCESS;
}
