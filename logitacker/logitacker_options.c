#include "fds.h"
#include "sdk_common.h"
#include "logitacker_options.h"
#include "logitacker_flash.h"

#define NRF_LOG_MODULE_NAME LOGITACKER_OPTIONS
#include "nrf_log.h"

NRF_LOG_MODULE_REGISTER();

logitacker_global_config_t g_logitacker_global_config = {0};
logitacker_global_state_t g_logitacker_global_runtime_state = {0};

void logitacker_options_init_state() {
    g_logitacker_global_runtime_state = LOGITACKER_STATE_DEFAULTS;
}

uint32_t logitacker_options_update_flash(void) {
    fds_record_t record;
    fds_record_desc_t record_desc;
    fds_find_token_t find_token;
    memset(&find_token, 0, sizeof(find_token)); //assure token is zeroed out before usage

    // only search for file_id-key-combination once (not account for multiple entries)
    ret_code_t res = fds_record_find(LOGITACKER_FLASH_FILE_ID_GLOBAL_OPTIONS, LOGITACKER_FLASH_KEY_GLOBAL_OPTIONS_LOGITACKER, &record_desc, &find_token);
    if (res != NRF_SUCCESS) {
        NRF_LOG_WARNING("failed to find Flash Data Storage record for global options: %d", res);
        return res;
    }

    record.file_id = LOGITACKER_FLASH_FILE_ID_GLOBAL_OPTIONS;
    record.key = LOGITACKER_FLASH_KEY_GLOBAL_OPTIONS_LOGITACKER;
    record.data.p_data = &g_logitacker_global_config;
    record.data.length_words = (sizeof(g_logitacker_global_config) + 3) / 4;


    res = fds_record_update(&record_desc, &record);
    if (res != NRF_SUCCESS) {
        NRF_LOG_WARNING("update: failed to update Flash Data Storage record for global options: %d", res);
        return res;
    }



    return res == NRF_SUCCESS ? NRF_SUCCESS : res;

}

uint32_t logitacker_options_store_to_flash(void) {
    fds_record_t record;
    fds_record_desc_t record_desc;

    // try to update first
    ret_code_t res = logitacker_options_update_flash();
    if (res == NRF_SUCCESS) {
        NRF_LOG_INFO("global options updated on Flash Data Storage");
        return res;
    }

    record.file_id = LOGITACKER_FLASH_FILE_ID_GLOBAL_OPTIONS;
    record.key = LOGITACKER_FLASH_KEY_GLOBAL_OPTIONS_LOGITACKER;
    record.data.p_data = &g_logitacker_global_config;
    record.data.length_words = (sizeof(g_logitacker_global_config) + 3) / 4;

    res = fds_record_write(&record_desc, &record);
    if (res != NRF_SUCCESS) {
        NRF_LOG_ERROR("failed to write global options to Flash Data Storage");
    }

    NRF_LOG_INFO("global options stored to Flash Data Storage");
    return res;
}

uint32_t logitacker_options_restore_from_flash(void) {
    fds_flash_record_t flash_record;
    fds_record_desc_t record_desc;
    fds_find_token_t find_token;
    memset(&find_token, 0, sizeof(find_token)); //assure token is zeroed out before usage

    bool restore_failed = false;
    // only search for file_id-key-combination once (not account for multiple entries)
    ret_code_t res = fds_record_find(LOGITACKER_FLASH_FILE_ID_GLOBAL_OPTIONS, LOGITACKER_FLASH_KEY_GLOBAL_OPTIONS_LOGITACKER, &record_desc, &find_token);
    if (res != NRF_SUCCESS) {
        NRF_LOG_WARNING("restore: failed to find Flash Data Storage record for global options: %d", res);
        restore_failed = true;
        goto finish;
    }

    res = fds_record_open(&record_desc, &flash_record);
    if (res != NRF_SUCCESS) {
        NRF_LOG_WARNING("restore: failed to open Flash Data Storage record for global options: %d", res);
        restore_failed = true;
        goto finish;
    }

    memcpy(&g_logitacker_global_config, flash_record.p_data, sizeof(g_logitacker_global_config));

    res = fds_record_close(&record_desc);
    if (res != NRF_SUCCESS) {
        NRF_LOG_WARNING("restore: failed to close Flash Data Storage record for global options: %d", res);
    }

    g_logitacker_global_config.stats.boot_count++;

    finish:
    if (restore_failed) {
        NRF_LOG_INFO("no stored options on flash, using default ones");
        g_logitacker_global_config = LOGITACKER_OPTIONS_DEFAULTS;
    }
    return NRF_SUCCESS;
}

/*
void logitacker_options_print_stats() {
    NRF_LOG_INFO("stats:\n=====");
    NRF_LOG_INFO("\tboot count:            %d", g_logitacker_global_config.stats.boot_count);
}

void logitacker_options_print() {
    NRF_LOG_INFO("global options:\n==============");
    NRF_LOG_INFO("\tdiscovery on new address action              :            %d", g_logitacker_global_config.discovery_on_new_address);
    NRF_LOG_INFO("\tpass-trough sniffed keyboard data to USB HID :            %s", g_logitacker_global_config.passive_enum_pass_through_keyboard ? "on" : "off");
    NRF_LOG_INFO("\tpass-trough sniffed mouse data to USB HID    :            %s", g_logitacker_global_config.passive_enum_pass_through_mouse ? "on" : "off");
}
 */


void logitacker_options_print(nrf_cli_t const * p_cli)
{

        char * discover_on_hit_str = "unknown";

        switch (g_logitacker_global_config.discovery_on_new_address) {
            case OPTION_DISCOVERY_ON_NEW_ADDRESS_CONTINUE:
                discover_on_hit_str = "continue in discover mode after a device address has been discovered";
                break;
            case OPTION_DISCOVERY_ON_NEW_ADDRESS_SWITCH_ACTIVE_ENUMERATION:
                discover_on_hit_str = "enter active enumeration mode after a device address has been discovered";
                break;
            case OPTION_DISCOVERY_ON_NEW_ADDRESS_SWITCH_PASSIVE_ENUMERATION:
                discover_on_hit_str = "enter passive enumeration mode after a device address has been discovered";
                break;
            case OPTION_DISCOVERY_ON_NEW_ADDRESS_SWITCH_AUTO_INJECTION:
                discover_on_hit_str = "(blindly) inject keystrokes for newly discovered address";
                break;
        }

        char * pair_sniff_success_action_str = "unknown";
        switch (g_logitacker_global_config.pair_sniff_on_success) {
            case OPTION_PAIR_SNIFF_ON_SUCCESS_CONTINUE:
                pair_sniff_success_action_str = "continue sniff pairing after successfully sniffed pairing";
                break;
            case OPTION_PAIR_SNIFF_ON_SUCCESS_SWITCH_ACTIVE_ENUMERATION:
                pair_sniff_success_action_str = "start active enumeration mode after successfully sniffed pairing";
                break;
            case OPTION_PAIR_SNIFF_ON_SUCCESS_SWITCH_PASSIVE_ENUMERATION:
                pair_sniff_success_action_str = "start passive enumeration mode after successfully sniffed pairing";
                break;
            case OPTION_PAIR_SNIFF_ON_SUCCESS_SWITCH_DISCOVERY:
                pair_sniff_success_action_str = "enter device discover mode after successfully sniffed pairing";
                break;
            case OPTION_PAIR_SNIFF_ON_SUCCESS_SWITCH_AUTO_INJECTION:
                pair_sniff_success_action_str = "inject keystrokes after successfully sniffed pairing";
                break;
        }

        char * injection_success_action_str = "unknown";
        switch (g_logitacker_global_config.inject_on_success) {
            case OPTION_AFTER_INJECT_CONTINUE:
                injection_success_action_str = "stay in injection mode after successful injection";
                break;
            case OPTION_AFTER_INJECT_SWITCH_DISCOVERY:
                injection_success_action_str = "enter discover mode after successful injection";
                break;
            case OPTION_AFTER_INJECT_SWITCH_ACTIVE_ENUMERATION:
                injection_success_action_str = "enter active enumeration mode after successful injection";
                break;
            case OPTION_AFTER_INJECT_SWITCH_PASSIVE_ENUMERATION:
                injection_success_action_str = "enter passive enumeration mode after successful injection";
                break;
        }

        char * injection_fail_action_str = "unknown";
        switch (g_logitacker_global_config.inject_on_fail) {
            case OPTION_AFTER_INJECT_CONTINUE:
                injection_fail_action_str = "stay in injection mode after failed injection";
                break;
            case OPTION_AFTER_INJECT_SWITCH_DISCOVERY:
                injection_fail_action_str = "enter discover mode after failed injection";
                break;
            case OPTION_AFTER_INJECT_SWITCH_ACTIVE_ENUMERATION:
                injection_fail_action_str = "enter active enumeration mode after failed injection";
                break;
            case OPTION_AFTER_INJECT_SWITCH_PASSIVE_ENUMERATION:
                injection_fail_action_str = "enter passive enumeration mode after failed injection";
                break;
        }

        char * injection_lan_str = "unknown";
        switch (g_logitacker_global_config.injection_language) {
            case LANGUAGE_LAYOUT_DE:
                injection_lan_str = "de";
                break;
            case LANGUAGE_LAYOUT_US:
                injection_lan_str = "us";
                break;
            case LANGUAGE_LAYOUT_DA:
                injection_lan_str = "da";
                break;
            case LANGUAGE_LAYOUT_FR:
                injection_lan_str = "fr";
                break;
        }

        char * workmode_str = "unknown";
        switch (g_logitacker_global_config.workmode) {
            case OPTION_LOGITACKER_WORKMODE_UNIFYING:
                workmode_str = "Unifying compatible";
                break;
            case OPTION_LOGITACKER_WORKMODE_LIGHTSPEED:
                workmode_str = "Logitech Lightspeed";
                break;
            case OPTION_LOGITACKER_WORKMODE_G700:
                workmode_str = "Logitech G700/G700s compatible";
                break;
        }

        char * bootmode_str = "unknown";
        switch (g_logitacker_global_config.bootmode) {
            case OPTION_LOGITACKER_BOOTMODE_USB_INJECT:
                bootmode_str = "USB keystroke injection";
                break;
            case OPTION_LOGITACKER_BOOTMODE_DISCOVER:
                bootmode_str = "Discover";
                break;
        }

        char * usbinjecttrigger_str = "unknown";
        switch (g_logitacker_global_config.usbinject_trigger) {
            case OPTION_LOGITACKER_USBINJECT_TRIGGER_ON_LEDUPDATE:
                usbinjecttrigger_str = "Start USB injection once keyboard LED state received (indicates driver ready, not on all OS)";
                break;
            case OPTION_LOGITACKER_USBINJECT_TRIGGER_ON_POWERUP:
                usbinjecttrigger_str = "Start USB injection once powered up (less accurate, works on all OS)";
                break;
        }



        nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "stats\r\n");
        nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "\tboot count                              : %d\r\n", g_logitacker_global_config.stats.boot_count);

        nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "\r\n");
        nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "global options\r\n");
        nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "\tboot mode                               : %s\r\n", bootmode_str);
        nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "\tworking mode                            : %s\r\n", workmode_str);
        nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "\tUSB injection mode (trigger)            : %s\r\n", usbinjecttrigger_str);

        nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "\r\n");
        nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "discover mode options\r\n");
        nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "\taction after RF address discovered      : %s\r\n", discover_on_hit_str);
        nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "\tpass RF frames to USB raw HID           : %s\r\n", g_logitacker_global_config.discover_pass_through_hidraw ? "on" : "off");
        nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "\tauto store plain injectable devices     : %s\r\n", g_logitacker_global_config.auto_store_plain_injectable ? "on" : "off");

        nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "\r\n");
        nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "passive-enumeration mode options\r\n");
        nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "\tpass key reports to USB keyboard        : %s\r\n", g_logitacker_global_config.passive_enum_pass_through_keyboard ? "on" : "off");
        nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "\tpass mouse reports to USB mouse         : %s\r\n", g_logitacker_global_config.passive_enum_pass_through_mouse ? "on" : "off");
        nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "\tpass all RF frames to USB raw HID       : %s\r\n", g_logitacker_global_config.passive_enum_pass_through_hidraw ? "on" : "off");

        nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "\r\n");
        nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "pair-sniff mode options\r\n");
        nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "\taction after sniffed pairing            : %s\r\n", pair_sniff_success_action_str);
        nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "\tauto store devices from sniffed pairing : %s\r\n", g_logitacker_global_config.auto_store_sniffed_pairing_devices ? "on" : "off");
        nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "\tpass RF frames to USB raw HID           : %s\r\n", g_logitacker_global_config.pair_sniff_pass_through_hidraw ? "on" : "off");

        nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "\r\n");
        nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "inject mode options\r\n");
        nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "\tkeyboard language layout                : %s\r\n", injection_lan_str);
        nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "\tdefault script                          : '%s'\r\n", strlen(g_logitacker_global_config.default_script) > 0 ? g_logitacker_global_config.default_script : "<none>");
        nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "\tmaximum auto-injects per device         : %d\r\n", g_logitacker_global_config.max_auto_injects_per_device);
        nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "\taction after successful injection       : %s\r\n", injection_success_action_str);
        nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "\taction after failed injection           : %s\r\n", injection_fail_action_str);

        return;
}
