#ifndef LOGITACKER_OPTIONS_H
#define LOGITACKER_OPTIONS_H


#include "logitacker.h"
#include "logitacker_script_engine.h"

typedef enum {
    OPTION_DISCOVERY_ON_NEW_ADDRESS_CONTINUE,   // continues in discovery mode, when new address has been found
    OPTION_DISCOVERY_ON_NEW_ADDRESS_SWITCH_PASSIVE_ENUMERATION,   // continues in passive enumeration mode when address found
    OPTION_DISCOVERY_ON_NEW_ADDRESS_SWITCH_ACTIVE_ENUMERATION,   // continues in active enumeration mode when address found
    OPTION_DISCOVERY_ON_NEW_ADDRESS_SWITCH_AUTO_INJECTION   // continues in active enumeration mode when address found
} option_discovery_on_new_address_t;

typedef enum {
    OPTION_PAIR_SNIFF_ON_SUCCESS_CONTINUE,   // continues in pair sniff mode
    OPTION_PAIR_SNIFF_ON_SUCCESS_SWITCH_PASSIVE_ENUMERATION,   // continues in passive enumeration mode when address found
    OPTION_PAIR_SNIFF_ON_SUCCESS_SWITCH_ACTIVE_ENUMERATION,   // continues in active enumeration mode when address found
    OPTION_PAIR_SNIFF_ON_SUCCESS_SWITCH_DISCOVERY,
    OPTION_PAIR_SNIFF_ON_SUCCESS_SWITCH_AUTO_INJECTION
} option_pair_sniff_on_success_t;

typedef enum {
    OPTION_AFTER_INJECT_CONTINUE,   // continues in injection mode (no re-execution)
    //OPTION_AFTER_INJECT_RETRY,
    OPTION_AFTER_INJECT_SWITCH_PASSIVE_ENUMERATION,
    OPTION_AFTER_INJECT_SWITCH_ACTIVE_ENUMERATION,
    OPTION_AFTER_INJECT_SWITCH_DISCOVERY,
} option_after_inject_t;

typedef enum {
    OPTION_LOGITACKER_WORKMODE_UNIFYING,   // compatible to Unifying and (encrypted) presentation clickers
    OPTION_LOGITACKER_WORKMODE_LIGHTSPEED,   // compatible to Logitech LIGHTSPEED
    OPTION_LOGITACKER_WORKMODE_G700,   // compatible to Logitech LIGHTSPEED
} option_logitacker_workmode_t;

typedef enum {
    OPTION_LOGITACKER_BOOTMODE_DISCOVER,
    OPTION_LOGITACKER_BOOTMODE_USB_INJECT,
} option_logitacker_bootmode_t;

typedef enum {
    OPTION_LOGITACKER_USBINJECT_TRIGGER_ON_POWERUP,
    OPTION_LOGITACKER_USBINJECT_TRIGGER_ON_LEDUPDATE,
} option_logitacker_usbinject_trigger_t;

typedef struct {
    uint32_t boot_count;
} logitacker_options_stats_t;

typedef struct {
    bool passive_enum_pass_through_hidraw;
    bool discover_pass_through_hidraw;
    bool pair_sniff_pass_through_hidraw;

    bool passive_enum_pass_through_keyboard;
    bool passive_enum_pass_through_mouse;
    option_discovery_on_new_address_t discovery_on_new_address; //not only state, persistent config
    option_pair_sniff_on_success_t pair_sniff_on_success; //not only state, persistent config

    option_after_inject_t inject_on_success;
    option_after_inject_t inject_on_fail;

    bool auto_store_plain_injectable;
    bool auto_store_sniffed_pairing_devices;

    logitacker_options_stats_t stats;

    logitacker_keyboard_map_lang_t injection_language;
    char default_script[LOGITACKER_SCRIPT_ENGINE_SCRIPT_NAME_MAX_LEN];

    int max_auto_injects_per_device;

    option_logitacker_workmode_t workmode;
    option_logitacker_bootmode_t bootmode;
    option_logitacker_usbinject_trigger_t usbinject_trigger;
} logitacker_global_config_t;

typedef struct {
    bool usb_inject_script_triggered;
    uint8_t usb_led_out_report_count;
//    uint8_t covert_channel_tx_delay;
//    uint8_t covert_channel_data_marker;
} logitacker_global_state_t;

const static logitacker_global_state_t LOGITACKER_STATE_DEFAULTS = {
        .usb_led_out_report_count = false,
        .usb_inject_script_triggered = false,
//        .covert_channel_tx_delay = COVERT_CHANNEL_TX_DELAY_MS_UNIFYING,
//        .covert_channel_data_marker = COVERT_CHANNEL_DATA_MARKER,
};


const static logitacker_global_config_t LOGITACKER_OPTIONS_DEFAULTS = {
    .discovery_on_new_address = OPTION_DISCOVERY_ON_NEW_ADDRESS_CONTINUE,
    .discover_pass_through_hidraw = false,

    .pair_sniff_on_success = OPTION_PAIR_SNIFF_ON_SUCCESS_SWITCH_PASSIVE_ENUMERATION,
    .inject_on_success = OPTION_AFTER_INJECT_CONTINUE,
    .inject_on_fail = OPTION_AFTER_INJECT_CONTINUE,

    .passive_enum_pass_through_mouse = false,
    .passive_enum_pass_through_keyboard = false,
    .passive_enum_pass_through_hidraw = false,
    .auto_store_sniffed_pairing_devices = true,
    .auto_store_plain_injectable = true,
    .injection_language = LANGUAGE_LAYOUT_US, //default layout (if nothing set) is US
    .default_script = "",
    .max_auto_injects_per_device = 5,
    .stats = {
        .boot_count = 0,
    },
    .workmode = OPTION_LOGITACKER_WORKMODE_UNIFYING,
    .bootmode = OPTION_LOGITACKER_BOOTMODE_DISCOVER,
    .usbinject_trigger = OPTION_LOGITACKER_USBINJECT_TRIGGER_ON_POWERUP,
};


extern logitacker_global_config_t g_logitacker_global_config;
extern logitacker_global_state_t g_logitacker_global_runtime_state;

uint32_t logitacker_options_store_to_flash(void);
uint32_t logitacker_options_restore_from_flash(void);
void logitacker_options_print(nrf_cli_t const * p_cli);
void logitacker_options_init_state();

#endif //LOGITACKER_OPTIONS_H
