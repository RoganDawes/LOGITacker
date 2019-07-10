#include <libraries/fds/fds.h>
#include "ctype.h"
#include "logitacker_tx_payload_provider.h"
#include "logitacker_tx_pay_provider_string_to_keys.h"
#include "logitacker_processor_passive_enum.h"
#include "logitacker_options.h"
#include "logitacker_keyboard_map.h"
#include "nrf_cli.h"
#include "nrf_log.h"
#include "sdk_common.h"
#include "logitacker.h"
#include "logitacker_devices.h"
#include "helper.h"
#include "logitacker_unifying.h"
#include "logitacker_flash.h"
#include "logitacker_processor_inject.h"
#include "logitacker_script_engine.h"

static void cmd_devices_remove_all(nrf_cli_t const * p_cli, size_t argc, char **argv);
static void cmd_script_press(nrf_cli_t const *p_cli, size_t argc, char **argv);

#define STORED_DEVICES_AUTOCOMPLETE_LIST_MAX_ENTRIES 60
static char m_stored_device_addr_str_list[STORED_DEVICES_AUTOCOMPLETE_LIST_MAX_ENTRIES][LOGITACKER_DEVICE_ADDR_STR_LEN];
static int m_stored_device_addr_str_list_len = 0;
static char m_device_addr_str_list[LOGITACKER_DEVICES_DEVICE_LIST_MAX_ENTRIES][LOGITACKER_DEVICE_ADDR_STR_LEN];
static int m_device_addr_str_list_len = 0;
static char m_device_addr_str_list_first_entry[] = "all\x00";

#define STORED_SCRIPTS_AUTOCOMPLETE_LIST_MAX_ENTRIES 32
static char m_stored_script_names_str_list[STORED_SCRIPTS_AUTOCOMPLETE_LIST_MAX_ENTRIES][LOGITACKER_SCRIPT_ENGINE_SCRIPT_NAME_MAX_LEN];
static int m_stored_script_names_str_list_len = 0;

static void stored_devices_str_list_update() {
    m_stored_device_addr_str_list_len = 1;
    memcpy(&m_stored_device_addr_str_list[0], m_device_addr_str_list_first_entry, sizeof(m_device_addr_str_list_first_entry));

    fds_find_token_t ftok;
    fds_record_desc_t record_desc;
    fds_flash_record_t flash_record;
    memset(&ftok, 0x00, sizeof(fds_find_token_t));

    while(fds_record_find(LOGITACKER_FLASH_FILE_ID_DEVICES, LOGITACKER_FLASH_RECORD_KEY_DEVICES, &record_desc, &ftok) == FDS_SUCCESS &&
    m_stored_device_addr_str_list_len <= STORED_DEVICES_AUTOCOMPLETE_LIST_MAX_ENTRIES) {
        if (fds_record_open(&record_desc, &flash_record) != FDS_SUCCESS) {
            NRF_LOG_WARNING("Failed to open record");
            continue; // go on with next
        }

        logitacker_devices_unifying_device_t const * p_device = flash_record.p_data;
        helper_addr_to_hex_str(m_stored_device_addr_str_list[m_stored_device_addr_str_list_len], LOGITACKER_DEVICE_ADDR_LEN, p_device->rf_address);
        m_stored_device_addr_str_list_len++;


        if (fds_record_close(&record_desc) != FDS_SUCCESS) {
            NRF_LOG_WARNING("Failed to close record");
        }
    }

}

static void stored_script_names_str_list_update() {
    fds_find_token_t ftoken;
    memset(&ftoken, 0x00, sizeof(fds_find_token_t));
    fds_flash_record_t flash_record;
    fds_record_desc_t fds_record_desc;

    m_stored_script_names_str_list_len = 0;
    while(m_stored_script_names_str_list_len < STORED_SCRIPTS_AUTOCOMPLETE_LIST_MAX_ENTRIES && fds_record_find(LOGITACKER_FLASH_FILE_ID_STORED_SCRIPTS_INFO, LOGITACKER_FLASH_RECORD_KEY_STORED_SCRIPTS_INFO, &fds_record_desc, &ftoken) == FDS_SUCCESS) {
        if (fds_record_open(&fds_record_desc, &flash_record) != FDS_SUCCESS) {
            NRF_LOG_WARNING("failed to open record");
            continue; // go on with next
        }

        stored_script_fds_info_t const * p_stored_tasks_fds_info_tmp = flash_record.p_data;

        int slen = strlen(p_stored_tasks_fds_info_tmp->script_name);
        slen = slen >= LOGITACKER_SCRIPT_ENGINE_SCRIPT_NAME_MAX_LEN ? LOGITACKER_SCRIPT_ENGINE_SCRIPT_NAME_MAX_LEN-1 : slen;
        memcpy(m_stored_script_names_str_list[m_stored_script_names_str_list_len], p_stored_tasks_fds_info_tmp->script_name, slen);

        if (fds_record_close(&fds_record_desc) != FDS_SUCCESS) {
            NRF_LOG_WARNING("failed to close record");
        }

        m_stored_script_names_str_list_len++;
    }
}

static void device_address_str_list_update() {
    m_device_addr_str_list_len = 1;
    memcpy(&m_device_addr_str_list[0], m_device_addr_str_list_first_entry, sizeof(m_device_addr_str_list_first_entry));

    logitacker_devices_list_iterator_t iter = {0};
    logitacker_devices_unifying_device_t * p_device;

    while (logitacker_devices_get_next_device(&p_device, &iter) == NRF_SUCCESS) {
        helper_addr_to_hex_str(m_device_addr_str_list[m_device_addr_str_list_len], LOGITACKER_DEVICE_ADDR_LEN, p_device->rf_address);
        m_device_addr_str_list_len++;
    }

}

// dynamic creation of stored script name list
static void dynamic_script_name(size_t idx, nrf_cli_static_entry_t *p_static)
{
    // Must be sorted alphabetically to ensure correct CLI completion.
    p_static->handler  = NULL;
    p_static->p_subcmd = NULL;
    p_static->p_help   = "Connect with address.";

    if (idx == 0) stored_script_names_str_list_update();

    NRF_LOG_INFO("script list len %d", m_stored_script_names_str_list_len);

    if (idx >= m_stored_script_names_str_list_len) {
        p_static->p_syntax = NULL;
        return;
    }

    p_static->p_syntax = m_stored_script_names_str_list[idx];
}

static void dynamic_device_addr_list_ram_with_all(size_t idx, nrf_cli_static_entry_t *p_static)
{
    // Must be sorted alphabetically to ensure correct CLI completion.
    p_static->handler  = NULL;
    p_static->p_subcmd = NULL;
    p_static->p_help   = "Connect with address.";


    if (idx == 0) {
        device_address_str_list_update(); // update list if idx 0 is requested
        p_static->p_syntax = m_device_addr_str_list[0];
        p_static->handler = cmd_devices_remove_all;
        p_static->p_help = "remove all devices";
    } else if (idx < m_device_addr_str_list_len) {
        p_static->p_syntax = m_device_addr_str_list[idx];
    } else {
        p_static->p_syntax = NULL;
    }
}

// dynamic creation of command addresses
static void dynamic_device_addr_list_ram(size_t idx, nrf_cli_static_entry_t *p_static)
{
    // Must be sorted alphabetically to ensure correct CLI completion.
    p_static->handler  = NULL;
    p_static->p_subcmd = NULL;
    p_static->p_help   = "Connect with address.";

    if (idx == 0) device_address_str_list_update();

    if (idx < m_device_addr_str_list_len-1) {
        p_static->p_syntax = m_device_addr_str_list[idx+1]; //ignore first entry
    } else {
        p_static->p_syntax = NULL;
    }
}
/*
// dynamic creation of command addresses
static void dynamic_device_addr_list_stored_with_all(size_t idx, nrf_cli_static_entry_t *p_static)
{
    // Must be sorted alphabetically to ensure correct CLI completion.
    p_static->handler  = NULL;
    p_static->p_subcmd = NULL;
    p_static->p_help   = "Connect with address.";


    if (idx == 0) {
        stored_devices_str_list_update(); // update list if idx 0 is requested
        p_static->p_syntax = m_device_addr_str_list[0];
        p_static->handler = NULL;
        p_static->p_help = "remove all devices";
    } else if (idx < m_stored_device_addr_str_list_len) {
        p_static->p_syntax = m_stored_device_addr_str_list[idx];
    } else {
        p_static->p_syntax = NULL;
    }
}
 */

// dynamic creation of command addresses
static void dynamic_device_addr_list_stored(size_t idx, nrf_cli_static_entry_t *p_static)
{
    // Must be sorted alphabetically to ensure correct CLI completion.
    p_static->handler  = NULL;
    p_static->p_subcmd = NULL;
    p_static->p_help   = "Connect with address.";

    if (idx == 0) stored_devices_str_list_update();

    if (idx < m_stored_device_addr_str_list_len-1) {
        p_static->p_syntax = m_stored_device_addr_str_list[idx+1]; //ignore first entry
    } else {
        p_static->p_syntax = NULL;
    }
}



static void print_logitacker_device_info(nrf_cli_t const * p_cli, const logitacker_devices_unifying_device_t * p_device) {
    if (p_device == NULL) {
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "empty device pointer");
        return;
    }
    logitacker_devices_unifying_dongle_t * p_dongle = p_device->p_dongle;
    if (p_dongle == NULL) {
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "empty dongle pointer");
        return;
    }

    char tmp_addr_str[16];
    helper_addr_to_hex_str(tmp_addr_str, LOGITACKER_DEVICE_ADDR_LEN, p_device->rf_address);


    bool dev_is_logitech = p_dongle->classification == DONGLE_CLASSIFICATION_IS_LOGITECH;
    nrf_cli_vt100_color_t outcol = NRF_CLI_VT100_COLOR_DEFAULT;
    if (dev_is_logitech) outcol = NRF_CLI_VT100_COLOR_BLUE;
    if (p_device->vuln_forced_pairing) outcol = NRF_CLI_VT100_COLOR_YELLOW;
    if (p_device->vuln_plain_injection) outcol = NRF_CLI_VT100_COLOR_GREEN;
    if (p_device->key_known) outcol = NRF_CLI_VT100_COLOR_RED;
    nrf_cli_fprintf(p_cli, outcol, "%s %s, keyboard: %s (%s, %s), mouse: %s\r\n",
        nrf_log_push(tmp_addr_str),
        p_dongle->classification == DONGLE_CLASSIFICATION_IS_LOGITECH ? "Logitech device" : "unknown device",
        (p_device->report_types & LOGITACKER_DEVICE_REPORT_TYPES_KEYBOARD) > 0 ?  "yes" : "no",
        (p_device->caps & LOGITACKER_DEVICE_CAPS_LINK_ENCRYPTION) > 0 ?  "encrypted" : "not encrypted",
        p_device->key_known ?  "key ḱnown" : "key unknown",
        (p_device->report_types & LOGITACKER_DEVICE_REPORT_TYPES_MOUSE) > 0 ?  "yes" : "no"
        );

}


#ifdef CLI_TEST_COMMANDS
static void cmd_test_a(nrf_cli_t const * p_cli, size_t argc, char **argv)
{
/*
    if (argc > 1)
    {
        nrf_cli_fprintf(p_cli, NRF_CLI_VT100_COLOR_DEFAULT, "parameter count %d\r\n", argc);

        //parse arg 1 as address
        uint8_t addr[5];
        if (helper_hex_str_to_addr(addr, 5, argv[1]) != NRF_SUCCESS) {
            nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "invalid address parameter, format has to be xx:xx:xx:xx:xx\r\n");
            return;
        }

        char tmp_addr_str[16];
        helper_addr_to_hex_str(tmp_addr_str, 5, addr);
        nrf_cli_fprintf(p_cli, NRF_CLI_VT100_COLOR_GREEN, "Trying to send keystrokes using address %s\r\n", tmp_addr_str);



        //logitacker_keyboard_map_test();
        logitacker_enter_mode_injection(addr);
        logitacker_injection_string(LANGUAGE_LAYOUT_DE, "Hello World!");

        return;
    } else {
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "device address needed, format has to be xx:xx:xx:xx:xx\r\n");
        return;

    }
*/
    logitacker_devices_log_stats();

    nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "sizeof(logitacker_devices_unifying_device_rf_address_t)   : %d\r\n", sizeof(logitacker_devices_unifying_device_rf_address_t));
    nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "sizeof(logitacker_devices_unifying_device_rf_addr_base_t) : %d\r\n", sizeof(logitacker_devices_unifying_device_rf_addr_base_t));
    nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "sizeof(logitacker_devices_unifying_device_t)              : %d\r\n", sizeof(logitacker_devices_unifying_device_t));
    nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "sizeof(logitacker_devices_unifying_dongle_t)              : %d\r\n", sizeof(logitacker_devices_unifying_dongle_t));

    logitacker_devices_unifying_device_t * p_device1 = NULL;
    logitacker_devices_unifying_device_t * p_device2 = NULL;
    logitacker_devices_unifying_device_t * p_device3 = NULL;
    logitacker_devices_unifying_device_rf_address_t addr1 = {0x00, 0x01, 0x02, 0x03, 0x04};
    logitacker_devices_unifying_device_rf_address_t addr2 = {0x01, 0x02, 0x03, 0x04, 0x05};
    logitacker_devices_unifying_device_rf_address_t addr3 = {0x01, 0x02, 0x03, 0x04, 0x02};


    logitacker_devices_create_device(&p_device1, addr1);
    logitacker_devices_create_device(&p_device2, addr2);
    logitacker_devices_create_device(&p_device3, addr3);

/*
    logitacker_flash_store_device(p_device1);
    logitacker_flash_store_device(p_device2);
    logitacker_flash_store_device(p_device3);
*/
    logitacker_flash_list_stored_devices();

/*
    logitacker_flash_store_dongle(p_device1->p_dongle);
    logitacker_flash_store_dongle(p_device2->p_dongle);
    logitacker_flash_store_dongle(p_device3->p_dongle);
*/
    logitacker_flash_list_stored_dongles();



    logitacker_flash_delete_device(p_device1->rf_address);
    logitacker_flash_delete_device(p_device2->rf_address);
    logitacker_flash_delete_device(p_device3->rf_address);
    logitacker_flash_delete_dongle(p_device1->p_dongle->base_addr);
    logitacker_flash_delete_dongle(p_device2->p_dongle->base_addr);
    logitacker_flash_delete_dongle(p_device3->p_dongle->base_addr);

    logitacker_devices_store_dongle_to_flash(p_device2->p_dongle->base_addr);

    fds_stat_t fds_stats;
    fds_stat(&fds_stats);
    nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "CLIS STATS\r\n-------------------\r\n");
    nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "pages available: %d\r\n", fds_stats.pages_available);
    nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "open records   : %d\r\n", fds_stats.open_records);
    nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "valid records  : %d\r\n", fds_stats.valid_records);
    nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "dirty records  : %d\r\n", fds_stats.dirty_records);
    nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "words reserved : %d\r\n", fds_stats.words_reserved);
    nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "words used     : %d\r\n", fds_stats.words_used);
    nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "largest contig : %d\r\n", fds_stats.largest_contig);
    nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "freeable words : %d\r\n", fds_stats.freeable_words);
    nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "corruption     : %s\r\n", fds_stats.corruption ? "true" : "false");

    fds_gc();
}

static void cmd_test_b(nrf_cli_t const * p_cli, size_t argc, char **argv) {

}

static void cmd_test_c(nrf_cli_t const * p_cli, size_t argc, char **argv) {
    fds_find_token_t ft;
    memset(&ft, 0x00, sizeof(fds_find_token_t));
    fds_record_desc_t rd;
    while (fds_record_iterate(&rd,&ft) == FDS_SUCCESS) {
        NRF_LOG_INFO("Deleting record...")
        fds_record_delete(&rd);
    }
    fds_gc();
}
#endif

static void cmd_version(nrf_cli_t const * p_cli, size_t argc, char **argv) {
    nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "LOGITacker by MaMe82\r\n");
    nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "Version: %s\r\n", VERSION_STRING);

}

static void cmd_erase_flash(nrf_cli_t const * p_cli, size_t argc, char **argv) {
    fds_find_token_t ft;
    memset(&ft, 0x00, sizeof(fds_find_token_t));
    fds_record_desc_t rd;
    while (fds_record_iterate(&rd,&ft) == FDS_SUCCESS) {
        NRF_LOG_INFO("Deleting record...")
        fds_record_delete(&rd);
    }
    fds_gc();
}

static void cmd_inject(nrf_cli_t const * p_cli, size_t argc, char **argv) {
    nrf_cli_help_print(p_cli, NULL, 0);
}

static void cmd_inject_target(nrf_cli_t const * p_cli, size_t argc, char **argv)
{
    if (argc > 1)
    {
        nrf_cli_fprintf(p_cli, NRF_CLI_VT100_COLOR_DEFAULT, "parameter count %d\r\n", argc);

        //parse arg 1 as address
        uint8_t addr[5];
        if (helper_hex_str_to_addr(addr, 5, argv[1]) != NRF_SUCCESS) {
            nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "invalid address parameter, format has to be xx:xx:xx:xx:xx\r\n");
            return;
        }

        char tmp_addr_str[16];
        helper_addr_to_hex_str(tmp_addr_str, 5, addr);
        nrf_cli_fprintf(p_cli, NRF_CLI_VT100_COLOR_GREEN, "Trying to send keystrokes using address %s\r\n", tmp_addr_str);



        //logitacker_keyboard_map_test();
        logitacker_enter_mode_injection(addr);
        return;
    } else {
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "device address needed, format has to be xx:xx:xx:xx:xx\r\n");
        return;

    }

}

static void cmd_script_store(nrf_cli_t const *p_cli, size_t argc, char **argv)
{
    if (argc == 2)
    {
        if (logitacker_script_engine_store_current_script_to_flash(argv[1])) {
            NRF_LOG_INFO("storing script succeeded");
            return;
        }
        NRF_LOG_INFO("Storing script failed");
        return;
    } else {
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "store needs a scriptname as first argument\r\n");
        return;

    }

}

static void cmd_script_load(nrf_cli_t const *p_cli, size_t argc, char **argv)
{
    if (argc == 2)
    {
        if (logitacker_script_engine_load_script_from_flash(argv[1])) {
            NRF_LOG_INFO("loading script succeeded");
            return;
        }
        NRF_LOG_INFO("loading script failed");
        return;
    } else {
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "load needs a script name as first argument\r\n");
        return;

    }

}

static void cmd_script_remove(nrf_cli_t const *p_cli, size_t argc, char **argv)
{
    if (argc == 2)
    {
        if (logitacker_script_engine_delete_script_from_flash(argv[1])) {
            NRF_LOG_INFO("deleting script succeeded");
            return;
        }
        NRF_LOG_INFO("deleting script failed");
        return;
    } else {
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "delete needs a script name as first argument\r\n");
        return;

    }
}

static void cmd_script_list(nrf_cli_t const *p_cli, size_t argc, char **argv) {
    logitacker_script_engine_list_scripts_from_flash(p_cli);
}

/*
static void cmd_inject_pause(nrf_cli_t const * p_cli, size_t argc, char **argv) {
    logitacker_injection_start_execution(false);
}
*/

static void cmd_inject_execute(nrf_cli_t const *p_cli, size_t argc, char **argv) {
    logitacker_injection_start_execution(true);
}

static void cmd_script_clear(nrf_cli_t const *p_cli, size_t argc, char **argv) {
    logitacker_script_engine_flush_tasks();
    NRF_LOG_INFO("script tasks cleared");
}

static void cmd_script_undo(nrf_cli_t const *p_cli, size_t argc, char **argv) {
    logitacker_script_engine_remove_last_task();
    NRF_LOG_INFO("removed last task from script");
}

static void cmd_inject_onsuccess_continue(nrf_cli_t const *p_cli, size_t argc, char **argv) {
    g_logitacker_global_config.inject_on_success = OPTION_AFTER_INJECT_CONTINUE;
}

static void cmd_inject_onsuccess_activeenum(nrf_cli_t const *p_cli, size_t argc, char **argv) {
    g_logitacker_global_config.inject_on_success = OPTION_AFTER_INJECT_SWITCH_ACTIVE_ENUMERATION;
}

static void cmd_inject_onsuccess_passiveenum(nrf_cli_t const *p_cli, size_t argc, char **argv) {
    g_logitacker_global_config.inject_on_success = OPTION_AFTER_INJECT_SWITCH_PASSIVE_ENUMERATION;
}

static void cmd_inject_onsuccess_discover(nrf_cli_t const *p_cli, size_t argc, char **argv) {
    g_logitacker_global_config.inject_on_success = OPTION_AFTER_INJECT_SWITCH_DISCOVERY;
}

static void cmd_inject_onfail_continue(nrf_cli_t const *p_cli, size_t argc, char **argv) {
    g_logitacker_global_config.inject_on_fail = OPTION_AFTER_INJECT_CONTINUE;
}

static void cmd_inject_onfail_activeenum(nrf_cli_t const *p_cli, size_t argc, char **argv) {
    g_logitacker_global_config.inject_on_fail = OPTION_AFTER_INJECT_SWITCH_ACTIVE_ENUMERATION;
}

static void cmd_inject_onfail_passiveenum(nrf_cli_t const *p_cli, size_t argc, char **argv) {
    g_logitacker_global_config.inject_on_fail = OPTION_AFTER_INJECT_SWITCH_PASSIVE_ENUMERATION;
}

static void cmd_inject_onfail_discover(nrf_cli_t const *p_cli, size_t argc, char **argv) {
    g_logitacker_global_config.inject_on_fail = OPTION_AFTER_INJECT_SWITCH_DISCOVERY;
}

static void cmd_options_inject_lang(nrf_cli_t const *p_cli, size_t argc, char **argv) {
    if (argc == 2)
    {
        logitacker_script_engine_set_language_layout(logitacker_keyboard_map_lang_from_str(argv[1]));

        return;
    } else {
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "need language layout name as first argument (f.e. us, de)\r\n");

        return;

    }

}

static void cmd_script_show(nrf_cli_t const *p_cli, size_t argc, char **argv) {
    logitacker_script_engine_print_current_tasks(p_cli);
}

static void cmd_script_string(nrf_cli_t const *p_cli, size_t argc, char **argv)
{
    char press_str[NRF_CLI_CMD_BUFF_SIZE] = {0};
    int str_buf_remaining = sizeof(press_str)-1; //keep one byte for terminating 0x00
    for (int i=1; i<argc && str_buf_remaining>0; i++) {
        if (i>1) strcat(press_str, " ");
        str_buf_remaining--;
        int len = strlen(argv[i]);
        if (len > str_buf_remaining) len = str_buf_remaining;
        strncat(press_str, argv[i], len);
        str_buf_remaining -= len;
    }

    logitacker_script_engine_append_task_type_string(press_str);
}

static void cmd_script_altstring(nrf_cli_t const *p_cli, size_t argc, char **argv)
{
    char press_str[NRF_CLI_CMD_BUFF_SIZE] = {0};
    int str_buf_remaining = sizeof(press_str)-1; //keep one byte for terminating 0x00
    for (int i=1; i<argc && str_buf_remaining>0; i++) {
        if (i>1) strcat(press_str, " ");
        str_buf_remaining--;
        int len = strlen(argv[i]);
        if (len > str_buf_remaining) len = str_buf_remaining;
        strncat(press_str, argv[i], len);
        str_buf_remaining -= len;
    }

    logitacker_script_engine_append_task_type_altstring(press_str);
}

static void cmd_script_delay(nrf_cli_t const *p_cli, size_t argc, char **argv)
{
    if (argc > 1) {
        uint32_t delay_ms;
        if (sscanf(argv[1], "%lu", &delay_ms) != 1) {
            nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "invalid delay, argument has to be unsigned int\r\n");
        };

        logitacker_script_engine_append_task_delay(delay_ms);
        return;
    }

    nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "invalid delay, argument has to be unsigned int\r\n");
}

static void cmd_script_press(nrf_cli_t const *p_cli, size_t argc, char **argv) {
    char press_str[NRF_CLI_CMD_BUFF_SIZE] = {0};
    int str_buf_remaining = sizeof(press_str)-1; //keep one byte for terminating 0x00
    for (int i=1; i<argc && str_buf_remaining>0; i++) {
        if (i>1) strcat(press_str, " ");
        str_buf_remaining--;
        int len = strlen(argv[i]);
        if (len > str_buf_remaining) len = str_buf_remaining;
        strncat(press_str, argv[i], len);
        str_buf_remaining -= len;
    }

    NRF_LOG_INFO("parsing '%s' to HID key combo report:", nrf_log_push(press_str));

    hid_keyboard_report_t tmp_report;
    logitacker_keyboard_map_combo_str_to_hid_report(press_str, &tmp_report, LANGUAGE_LAYOUT_DE);
    NRF_LOG_HEXDUMP_INFO(&tmp_report, sizeof(hid_keyboard_report_t));

    logitacker_script_engine_append_task_press_combo(press_str);
}


static void cmd_discover_onhit_activeenum(nrf_cli_t const * p_cli, size_t argc, char **argv)
{
    g_logitacker_global_config.discovery_on_new_address = OPTION_DISCOVERY_ON_NEW_ADDRESS_SWITCH_ACTIVE_ENUMERATION;
    nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "on-hit action: start active enumeration of new RF address\r\n");
    return;
}

static void cmd_discover_onhit_passiveenum(nrf_cli_t const * p_cli, size_t argc, char **argv)
{
    g_logitacker_global_config.discovery_on_new_address = OPTION_DISCOVERY_ON_NEW_ADDRESS_SWITCH_PASSIVE_ENUMERATION;
    nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "on-hit action: start passive enumeration of new RF address\r\n");
    return;
}

static void cmd_discover_onhit_autoinject(nrf_cli_t const * p_cli, size_t argc, char **argv)
{
    g_logitacker_global_config.discovery_on_new_address = OPTION_DISCOVERY_ON_NEW_ADDRESS_SWITCH_AUTO_INJECTION;
    nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "on-hit action: enter injection mode and execute injection\r\n");
    return;
}

static void cmd_discover_onhit_continue(nrf_cli_t const * p_cli, size_t argc, char **argv)
{
    g_logitacker_global_config.discovery_on_new_address = OPTION_DISCOVERY_ON_NEW_ADDRESS_CONTINUE;
    nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "on-hit action: continue\r\n");
}

static void cmd_pair_sniff_onsuccess_continue(nrf_cli_t const * p_cli, size_t argc, char **argv) {
    g_logitacker_global_config.pair_sniff_on_success = OPTION_PAIR_SNIFF_ON_SUCCESS_CONTINUE;
    nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "on-success action: continue\r\n");
}

static void cmd_pair_sniff_onsuccess_discover(nrf_cli_t const * p_cli, size_t argc, char **argv) {
    g_logitacker_global_config.pair_sniff_on_success = OPTION_PAIR_SNIFF_ON_SUCCESS_SWITCH_DISCOVERY;
    nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "on-success action: enter discover mode\r\n");
}

static void cmd_pair_sniff_onsuccess_passiveenum(nrf_cli_t const * p_cli, size_t argc, char **argv) {
    g_logitacker_global_config.pair_sniff_on_success = OPTION_PAIR_SNIFF_ON_SUCCESS_SWITCH_PASSIVE_ENUMERATION;
    nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "on-success action: enter passive enumeration mode\r\n");
}

static void cmd_pair_sniff_onsuccess_activeenum(nrf_cli_t const * p_cli, size_t argc, char **argv) {
    g_logitacker_global_config.pair_sniff_on_success = OPTION_PAIR_SNIFF_ON_SUCCESS_SWITCH_ACTIVE_ENUMERATION;
    nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "on-success action: enter active enumeration mode\r\n");
}

static void cmd_pair_sniff_onsuccess_autoinject(nrf_cli_t const * p_cli, size_t argc, char **argv) {
    g_logitacker_global_config.pair_sniff_on_success = OPTION_PAIR_SNIFF_ON_SUCCESS_SWITCH_AUTO_INJECTION;
    nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "on-success action: enter injection mode and execute injection\r\n");
}

static void cmd_pair_sniff_run(nrf_cli_t const *p_cli, size_t argc, char **argv) {
    logitacker_enter_mode_pair_sniff();
}

static void cmd_pair_device_run(nrf_cli_t const *p_cli, size_t argc, char **argv)
{
    if (argc > 1)
    {
        nrf_cli_fprintf(p_cli, NRF_CLI_VT100_COLOR_DEFAULT, "parameter count %d\r\n", argc);

        //parse arg 1 as address
        uint8_t addr[5];
        if (helper_hex_str_to_addr(addr, 5, argv[1]) != NRF_SUCCESS) {
            nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "invalid address parameter, format has to be xx:xx:xx:xx:xx\r\n");
            return;
        }

        char tmp_addr_str[16];
        helper_addr_to_hex_str(tmp_addr_str, 5, addr);
        nrf_cli_fprintf(p_cli, NRF_CLI_VT100_COLOR_GREEN, "Trying to force pair using address %s\r\n", tmp_addr_str);
        logitacker_enter_mode_pair_device(addr);
        return;
    }

    nrf_cli_fprintf(p_cli, NRF_CLI_VT100_COLOR_GREEN, "Trying to pair using Unifying global pairing address\r\n");
    logitacker_enter_mode_pair_device(UNIFYING_GLOBAL_PAIRING_ADDRESS);
}

static void cmd_help(nrf_cli_t const *p_cli, size_t argc, char **argv)
{
    nrf_cli_help_print(p_cli, NULL, 0);
}

static void cmd_pair(nrf_cli_t const *p_cli, size_t argc, char **argv)
{
    if ((argc == 1) || nrf_cli_help_requested(p_cli))
    {
        nrf_cli_help_print(p_cli, NULL, 0);
        return;
    }

    if (argc != 2)
    {
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "%s: bad parameter count\r\n", argv[0]);
        return;
    }

    nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "%s: unknown parameter: %s\r\n", argv[0], argv[1]);
}

static void cmd_discover_run(nrf_cli_t const * p_cli, size_t argc, char **argv) {
    logitacker_enter_mode_discovery();
}

static void cmd_discover(nrf_cli_t const * p_cli, size_t argc, char **argv)
{
    if ((argc == 1) || nrf_cli_help_requested(p_cli))
    {
        nrf_cli_help_print(p_cli, NULL, 0);
        return;
    }

    if (argc != 2)
    {
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "%s: bad parameter count\r\n", argv[0]);
        return;
    }

    nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "%s: unknown parameter: %s\r\n", argv[0], argv[1]);
}

static void cmd_devices(nrf_cli_t const * p_cli, size_t argc, char **argv) {
    logitacker_devices_list_iterator_t iter = {0};
    logitacker_devices_unifying_dongle_t * p_dongle = NULL;
    logitacker_devices_unifying_device_t * p_device = NULL;
    while (logitacker_devices_get_next_dongle(&p_dongle, &iter) == NRF_SUCCESS) {
        if (p_dongle != NULL) {

            for (int device_index=0; device_index < p_dongle->num_connected_devices; device_index++) {
                p_device = p_dongle->p_connected_devices[device_index];

                print_logitacker_device_info(p_cli, p_device);
            }
        }
    }
}

static void cmd_devices_remove(nrf_cli_t const * p_cli, size_t argc, char **argv) {
    if (argc > 1)
    {

        //parse arg 1 as address
        uint8_t addr[5];
        if (helper_hex_str_to_addr(addr, 5, argv[1]) != NRF_SUCCESS) {
            nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "invalid address parameter, format has to be xx:xx:xx:xx:xx\r\n");
            return;
        }

        char tmp_addr_str[16];
        helper_addr_to_hex_str(tmp_addr_str, 5, addr);
        nrf_cli_fprintf(p_cli, NRF_CLI_VT100_COLOR_GREEN, "Deleting device %s\r\n", tmp_addr_str);
        logitacker_devices_del_device(addr);
        return;
    }
}

static void cmd_devices_add(nrf_cli_t const * p_cli, size_t argc, char **argv) {
    logitacker_devices_unifying_device_t * p_device;
    if (argc > 1)
    {
        nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "adding device %s as plain injectable keyboard\r\n");

        //parse arg 1 as address
        uint8_t addr[5];
        if (helper_hex_str_to_addr(addr, 5, argv[1]) != NRF_SUCCESS) {
            nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "invalid address parameter, format has to be xx:xx:xx:xx:xx\r\n");
            return;
        }

        char tmp_addr_str[16];
        helper_addr_to_hex_str(tmp_addr_str, 5, addr);
        nrf_cli_fprintf(p_cli, NRF_CLI_VT100_COLOR_GREEN, "Adding device %s\r\n", tmp_addr_str);


        if (logitacker_devices_create_device(&p_device, addr) != NRF_SUCCESS) {
            nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "Error adding device %s\r\n", tmp_addr_str);
            return;
        }

        // add keyboard capabilities
        p_device->report_types |= LOGITACKER_DEVICE_REPORT_TYPES_KEYBOARD | LOGITACKER_DEVICE_REPORT_TYPES_MEDIA_CENTER | LOGITACKER_DEVICE_REPORT_TYPES_MULTIMEDIA | LOGITACKER_DEVICE_REPORT_TYPES_POWER_KEYS;

        if (argc > 2) {
            uint8_t key[16] = {0};
            if (helper_hex_str_to_bytes(key, 16, argv[2]) != NRF_SUCCESS) {
                nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "invalid key parameter, format has to be xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\r\n");
                return;
            }

            // add key and mark device as link-encryption enabled
            memcpy(p_device->key, key, 16);
            p_device->key_known = true;
            p_device->caps |= LOGITACKER_DEVICE_CAPS_LINK_ENCRYPTION;

            nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "added key to device\r\n");
        }
    } else {
        nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "syntax to add a device manually:\r\n");
        nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "    devices add <RF-address> [AES key]\r\n");
        nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "example device no encryption   : devices add de:ad:be:ef:01\r\n");
        nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "example device with encryption : devices add de:ad:be:ef:02 023601e63268c8d37988847af1ae40a1\r\n");
    };
}

static void cmd_devices_storage_save(nrf_cli_t const *p_cli, size_t argc, char **argv) {
    if (argc > 1)
    {

        //parse arg 1 as address
        uint8_t addr[5];
        if (helper_hex_str_to_addr(addr, 5, argv[1]) != NRF_SUCCESS) {
            nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "invalid address parameter, format has to be xx:xx:xx:xx:xx\r\n");
            return;
        }

        char tmp_addr_str[16];
        helper_addr_to_hex_str(tmp_addr_str, 5, addr);
        nrf_cli_fprintf(p_cli, NRF_CLI_VT100_COLOR_GREEN, "Storing device %s to flash\r\n", tmp_addr_str);
        logitacker_devices_store_ram_device_to_flash(addr);
        return;
    }
}

// dynamic creation of command addresses
static void cmd_devices_storage_remove(nrf_cli_t const *p_cli, size_t argc, char **argv) {
    if (argc > 1)
    {
        //parse arg 1 as address
        uint8_t addr[5];
        if (helper_hex_str_to_addr(addr, 5, argv[1]) != NRF_SUCCESS) {
            nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "invalid address parameter, format has to be xx:xx:xx:xx:xx\r\n");
            return;
        }

        char tmp_addr_str[16];
        helper_addr_to_hex_str(tmp_addr_str, 5, addr);
        nrf_cli_fprintf(p_cli, NRF_CLI_VT100_COLOR_GREEN, "Deleting device %s from flash\r\n", tmp_addr_str);
        logitacker_devices_remove_device_from_flash(addr);
        return;
    }

}

static void cmd_devices_storage_load(nrf_cli_t const *p_cli, size_t argc, char **argv) {
    if (argc > 1)
    {
        //parse arg 1 as address
        uint8_t addr[5];
        if (helper_hex_str_to_addr(addr, 5, argv[1]) != NRF_SUCCESS) {
            nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "invalid address parameter, format has to be xx:xx:xx:xx:xx\r\n");
            return;
        }

        char tmp_addr_str[16];
        helper_addr_to_hex_str(tmp_addr_str, 5, addr);
        nrf_cli_fprintf(p_cli, NRF_CLI_VT100_COLOR_GREEN, "Restoring device %s from flash\r\n", tmp_addr_str);
        logitacker_devices_unifying_device_t * p_dummy_device;
        logitacker_devices_restore_device_from_flash(&p_dummy_device, addr);
        return;
    }
}

static void cmd_devices_storage_list(nrf_cli_t const *p_cli, size_t argc, char **argv) {
    fds_find_token_t ftok;
    fds_record_desc_t record_desc;
    fds_flash_record_t flash_record;
    memset(&ftok, 0x00, sizeof(fds_find_token_t));

    logitacker_devices_unifying_device_t tmp_device;
    logitacker_devices_unifying_dongle_t tmp_dongle;

    nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "list of devices stored on flash:\r\n");
    NRF_LOG_INFO("Devices on flash");
    while(fds_record_find(LOGITACKER_FLASH_FILE_ID_DEVICES, LOGITACKER_FLASH_RECORD_KEY_DEVICES, &record_desc, &ftok) == FDS_SUCCESS) {
        if (fds_record_open(&record_desc, &flash_record) != FDS_SUCCESS) {
            NRF_LOG_WARNING("Failed to open record");
            continue; // go on with next
        }

        logitacker_devices_unifying_device_t const * p_device = flash_record.p_data;

        //we need a writable copy of device to assign dongle data
        memcpy(&tmp_device, p_device, sizeof(logitacker_devices_unifying_device_t));



        if (fds_record_close(&record_desc) != FDS_SUCCESS) {
            NRF_LOG_WARNING("Failed to close record")
        }

        // load stored dongle (without dongle data, classification like "is logitech" would be missing)
        if (logitacker_flash_get_dongle_for_device(&tmp_dongle, &tmp_device) == NRF_SUCCESS) tmp_device.p_dongle = &tmp_dongle;
        print_logitacker_device_info(p_cli, &tmp_device);

    }
}


static void cmd_devices_remove_all(nrf_cli_t const * p_cli, size_t argc, char **argv) {
    logitacker_devices_del_all();
}

static void cmd_options_show(nrf_cli_t const *p_cli, size_t argc, char **argv) {
    logitacker_options_print(p_cli);
}

static void cmd_options_inject_maxautoinjectsperdevice(nrf_cli_t const *p_cli, size_t argc, char **argv) {
    if (argc > 1) {
        int count;
        if (sscanf(argv[1], "%d", &count) != 1) {
            nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "invalid argument, auto inject count has to be a integer number, but '%s' was given\r\n", argv[1]);
        } else {
            g_logitacker_global_config.max_auto_injects_per_device = count;
        }
    } else {
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "invalid argument, auto inject count has to be a integer number\r\n");
        nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "current setting of maximum per device auto-injects: %d\r\n", g_logitacker_global_config.max_auto_injects_per_device);
    }
}

static void cmd_options_store(nrf_cli_t const * p_cli, size_t argc, char **argv) {
    logitacker_options_store_to_flash();
}

static void cmd_options_erase(nrf_cli_t const * p_cli, size_t argc, char **argv) {
    fds_file_delete(LOGITACKER_FLASH_FILE_ID_GLOBAL_OPTIONS);
}

static void cmd_options_inject_defaultscript(nrf_cli_t const *p_cli, size_t argc, char **argv) {
    if (argc > 1) {
        size_t len = strlen(argv[1]);
        if (len > LOGITACKER_SCRIPT_ENGINE_SCRIPT_NAME_MAX_LEN-1) len = LOGITACKER_SCRIPT_ENGINE_SCRIPT_NAME_MAX_LEN-1;
        memcpy(g_logitacker_global_config.default_script, argv[1], len);
        g_logitacker_global_config.default_script[len] = 0x00;
        return;
    }

    nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "default injection script: '%s'\r\n", g_logitacker_global_config.default_script);
}

static void cmd_options_inject_defaultscript_clear(nrf_cli_t const *p_cli, size_t argc, char **argv) {
    g_logitacker_global_config.default_script[0] = 0x00;
}

static void cmd_options_pairsniff_autostoredevice(nrf_cli_t const *p_cli, size_t argc, char **argv)
{
    if (argc > 1)
    {
        if (strcmp(argv[1], "off") == 0 || strcmp(argv[1], "OFF") == 0) g_logitacker_global_config.auto_store_sniffed_pairing_devices = false;
        else if (strcmp(argv[1], "on") == 0 || strcmp(argv[1], "ON") == 0) g_logitacker_global_config.auto_store_sniffed_pairing_devices = true;
    }

    nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "auto-store devices after pairing has been sniffed successfully: %s\r\n", g_logitacker_global_config.auto_store_sniffed_pairing_devices ? "on" : "off");
}

static void cmd_options_discover_autostoreplain(nrf_cli_t const *p_cli, size_t argc, char **argv)
{
    if (argc > 1)
    {
        if (strcmp(argv[1], "off") == 0 || strcmp(argv[1], "OFF") == 0) g_logitacker_global_config.auto_store_plain_injectable = false;
        else if (strcmp(argv[1], "on") == 0 || strcmp(argv[1], "ON") == 0) g_logitacker_global_config.auto_store_plain_injectable = true;
    }

    nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "auto-store discovered devices, if they allow plain injection: %s\r\n", g_logitacker_global_config.auto_store_plain_injectable ? "on" : "off");
}

static void cmd_options_passiveenum_pass_keyboard(nrf_cli_t const *p_cli, size_t argc, char **argv)
{
    if (argc > 1)
    {
        if (strcmp(argv[1], "off") == 0 || strcmp(argv[1], "OFF") == 0) g_logitacker_global_config.passive_enum_pass_through_keyboard = false;
        else if (strcmp(argv[1], "on") == 0 || strcmp(argv[1], "ON") == 0) g_logitacker_global_config.passive_enum_pass_through_keyboard = true;
    }

    nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "passive-enum USB keyboard pass-through: %s\r\n", g_logitacker_global_config.passive_enum_pass_through_keyboard ? "on" : "off");
}

static void cmd_options_passiveenum_pass_mouse(nrf_cli_t const *p_cli, size_t argc, char **argv)
{
    if (argc > 1)
    {
        if (strcmp(argv[1], "off") == 0 || strcmp(argv[1], "OFF") == 0) g_logitacker_global_config.passive_enum_pass_through_mouse = false;
        else if (strcmp(argv[1], "on") == 0 || strcmp(argv[1], "ON") == 0) g_logitacker_global_config.passive_enum_pass_through_mouse = true;

    }

    nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "passive-enum USB mouse pass-through: %s\r\n", g_logitacker_global_config.passive_enum_pass_through_mouse ? "on" : "off");
}

static void cmd_options_discover_pass_raw(nrf_cli_t const *p_cli, size_t argc, char **argv)
{
    if (argc > 1)
    {
        if (strcmp(argv[1], "off") == 0 || strcmp(argv[1], "OFF") == 0) g_logitacker_global_config.discover_pass_through_hidraw = false;
        else if (strcmp(argv[1], "on") == 0 || strcmp(argv[1], "ON") == 0) g_logitacker_global_config.discover_pass_through_hidraw = true;

    }

    nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "discover raw USB pass-through: %s\r\n", g_logitacker_global_config.discover_pass_through_hidraw ? "on" : "off");
}

static void cmd_options_passiveenum_pass_raw(nrf_cli_t const *p_cli, size_t argc, char **argv)
{
    if (argc > 1)
    {
        if (strcmp(argv[1], "off") == 0 || strcmp(argv[1], "OFF") == 0) g_logitacker_global_config.passive_enum_pass_through_hidraw = false;
        else if (strcmp(argv[1], "on") == 0 || strcmp(argv[1], "ON") == 0) g_logitacker_global_config.passive_enum_pass_through_hidraw = true;

    }

    nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "passive-enum raw USB pass-through: %s\r\n", g_logitacker_global_config.passive_enum_pass_through_hidraw ? "on" : "off");
}

static void cmd_enum_passive(nrf_cli_t const * p_cli, size_t argc, char **argv) {
    if (argc > 1)
    {

        //parse arg 1 as address
        uint8_t addr[5];
        if (helper_hex_str_to_addr(addr, 5, argv[1]) != NRF_SUCCESS) {
            nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "invalid address parameter, format has to be xx:xx:xx:xx:xx\r\n");
            return;
        }

        char tmp_addr_str[16];
        helper_addr_to_hex_str(tmp_addr_str, 5, addr);
        nrf_cli_fprintf(p_cli, NRF_CLI_VT100_COLOR_GREEN, "Starting passive enumeration for device %s\r\n", tmp_addr_str);
        logitacker_enter_mode_passive_enum(addr);
        return;
    } else {
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "invalid address parameter, format has to be xx:xx:xx:xx:xx\r\n");
    }
}

static void cmd_enum_active(nrf_cli_t const * p_cli, size_t argc, char **argv) {
    if (argc > 1)
    {

        //parse arg 1 as address
        uint8_t addr[5];
        if (helper_hex_str_to_addr(addr, 5, argv[1]) != NRF_SUCCESS) {
            nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "invalid address parameter, format has to be xx:xx:xx:xx:xx\r\n");
            return;
        }

        char tmp_addr_str[16];
        helper_addr_to_hex_str(tmp_addr_str, 5, addr);
        nrf_cli_fprintf(p_cli, NRF_CLI_VT100_COLOR_GREEN, "Starting active enumeration for device %s\r\n", tmp_addr_str);
        logitacker_enter_mode_active_enum(addr);
        return;
    } else {
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "invalid address parameter, format has to be xx:xx:xx:xx:xx\r\n");
    }
}


#ifdef CLI_TEST_COMMANDS
NRF_CLI_CREATE_STATIC_SUBCMD_SET(m_sub_test)
        {
                NRF_CLI_CMD(a, NULL, "test a", cmd_test_a),
                NRF_CLI_CMD(b, NULL, "test b", cmd_test_b),
                NRF_CLI_CMD(c, NULL, "test b", cmd_test_c),
                NRF_CLI_SUBCMD_SET_END
        };
NRF_CLI_CMD_REGISTER(test, &m_sub_test, "Debug command to test code", NULL);
#endif



NRF_CLI_CREATE_STATIC_SUBCMD_SET(m_sub_discover)
{
    NRF_CLI_CMD(run,   NULL, "Enter discover mode.", cmd_discover_run),
    NRF_CLI_SUBCMD_SET_END
};
NRF_CLI_CMD_REGISTER(discover, &m_sub_discover, "discover", cmd_discover);

//pair_sniff_run level 3
NRF_CLI_CREATE_DYNAMIC_CMD(m_sub_pair_device_addresses, dynamic_device_addr_list_ram);

//pair_sniff level 2
NRF_CLI_CREATE_STATIC_SUBCMD_SET(m_sub_pair_sniff)
{
        NRF_CLI_CMD(run, NULL, "Sniff pairing.", cmd_pair_sniff_run),
        NRF_CLI_SUBCMD_SET_END
};

NRF_CLI_CREATE_STATIC_SUBCMD_SET(m_sub_pair_device)
{
        NRF_CLI_CMD(run, &m_sub_pair_device_addresses, "Pair device on given address (if no address given, pairing address is used).", cmd_pair_device_run),
        NRF_CLI_SUBCMD_SET_END
};


NRF_CLI_CREATE_STATIC_SUBCMD_SET(m_sub_pair)
{
    NRF_CLI_CMD(sniff, &m_sub_pair_sniff, "Sniff pairing.", cmd_help),
    NRF_CLI_CMD(device, &m_sub_pair_device, "pair or forced pair a device to a dongle", NULL),
    NRF_CLI_SUBCMD_SET_END
};
NRF_CLI_CMD_REGISTER(pair, &m_sub_pair, "discover", cmd_pair);

//LEVEL 3
NRF_CLI_CREATE_DYNAMIC_CMD(m_sub_dynamic_script_name, dynamic_script_name);

NRF_CLI_CREATE_STATIC_SUBCMD_SET(m_sub_script)
{
        NRF_CLI_CMD(clear,   NULL, "clear current script (injection tasks)", cmd_script_clear),
        NRF_CLI_CMD(undo,   NULL, "delete last command from script (last injection task)", cmd_script_undo),
        NRF_CLI_CMD(show,   NULL, "show listing of current script", cmd_script_show),
        NRF_CLI_CMD(string,   NULL, "append 'string' command to script, which types out the text given as parameter", cmd_script_string),
        NRF_CLI_CMD(altstring,   NULL, "append 'altstring' command to script, which types out the text using NUMPAD", cmd_script_altstring),
        NRF_CLI_CMD(press,   NULL, "append 'press' command to script, which creates a key combination from the given parameters", cmd_script_press),
        NRF_CLI_CMD(delay,   NULL, "append 'delay' command to script, delays script execution by the amount of milliseconds given as parameter", cmd_script_delay),
        NRF_CLI_CMD(store,   NULL, "store script to flash", cmd_script_store),
        NRF_CLI_CMD(load,   &m_sub_dynamic_script_name, "load script from flash", cmd_script_load),
        NRF_CLI_CMD(list,   NULL, "list scripts stored on flash", cmd_script_list),
        NRF_CLI_CMD(remove,   &m_sub_dynamic_script_name, "delete script from flash", cmd_script_remove),
        NRF_CLI_SUBCMD_SET_END
};
NRF_CLI_CMD_REGISTER(script, &m_sub_script, "scripting for injection", cmd_inject);

//level 2
NRF_CLI_CREATE_DYNAMIC_CMD(m_sub_inject_target_addr, dynamic_device_addr_list_ram);

// level 1
NRF_CLI_CREATE_STATIC_SUBCMD_SET(m_sub_inject)
{
    NRF_CLI_CMD(target, &m_sub_inject_target_addr, "enter injection mode for given target RF address", cmd_inject_target),
    NRF_CLI_CMD(execute,   NULL, "run current script against injection target", cmd_inject_execute),
    NRF_CLI_SUBCMD_SET_END
};
NRF_CLI_CMD_REGISTER(inject, &m_sub_inject, "injection", cmd_inject);

//device level 3
NRF_CLI_CREATE_DYNAMIC_CMD(m_sub_device_storage_load_list, dynamic_device_addr_list_stored);
//NRF_CLI_CREATE_DYNAMIC_CMD(m_sub_device_store_delete_list, dynamic_device_addr_list_stored_with_all);
NRF_CLI_CREATE_DYNAMIC_CMD(m_sub_device_store_delete_list, dynamic_device_addr_list_stored); // don't offer delete all option for flash
NRF_CLI_CREATE_DYNAMIC_CMD(m_sub_device_store_save_list, dynamic_device_addr_list_ram);
//device level 2
NRF_CLI_CREATE_STATIC_SUBCMD_SET(m_sub_devices_storage)
{
    NRF_CLI_CMD(list, NULL, "list devices stored on flash", cmd_devices_storage_list),
    NRF_CLI_CMD(load, &m_sub_device_storage_load_list, "load a stored device", cmd_devices_storage_load),
    NRF_CLI_CMD(save, &m_sub_device_store_save_list, "store a device to flash", cmd_devices_storage_save),
    NRF_CLI_CMD(remove, &m_sub_device_store_delete_list, "delete a stored device from flash", cmd_devices_storage_remove),
    NRF_CLI_SUBCMD_SET_END
};

NRF_CLI_CREATE_DYNAMIC_CMD(m_sub_devices_remove_addr_collection, dynamic_device_addr_list_ram_with_all);
//devices level 1 (handles auto-complete from level 2 as parameter)
NRF_CLI_CREATE_STATIC_SUBCMD_SET(m_sub_devices)
{
        NRF_CLI_CMD(storage, &m_sub_devices_storage, "handle devices on flash", NULL),
        NRF_CLI_CMD(remove, &m_sub_devices_remove_addr_collection, "delete a device from list (RAM)", cmd_devices_remove),
        NRF_CLI_CMD(add, NULL, "manually add a device (RAM)", cmd_devices_add),
        NRF_CLI_SUBCMD_SET_END
};
NRF_CLI_CMD_REGISTER(devices, &m_sub_devices, "List discovered devices", cmd_devices);

NRF_CLI_CREATE_DYNAMIC_CMD(m_sub_enum_device_list, dynamic_device_addr_list_ram);

NRF_CLI_CMD_REGISTER(active_enum, &m_sub_enum_device_list, "start active enumeration of given device", cmd_enum_active);
NRF_CLI_CMD_REGISTER(passive_enum, &m_sub_enum_device_list, "start passive enumeration of given device", cmd_enum_passive);

NRF_CLI_CMD_REGISTER(erase_flash, NULL, "erase all data stored on flash", cmd_erase_flash);

NRF_CLI_CMD_REGISTER(version, NULL, "print version string", cmd_version);

//options

NRF_CLI_CREATE_STATIC_SUBCMD_SET(m_sub_options_on_off)
{
    NRF_CLI_CMD(on, NULL, "enable", NULL),
    NRF_CLI_CMD(off, NULL, "disable", NULL),
    NRF_CLI_SUBCMD_SET_END
};

NRF_CLI_CREATE_STATIC_SUBCMD_SET(m_sub_options_inject_defaultscript)
{
    NRF_CLI_CMD(clear, NULL, "clear default script", cmd_options_inject_defaultscript_clear),
    NRF_CLI_SUBCMD_SET_END
};

// options passive-enum
NRF_CLI_CREATE_STATIC_SUBCMD_SET(m_sub_options_passiveenum)
{
    NRF_CLI_CMD(pass-through-keyboard, &m_sub_options_on_off, "pass received keyboard reports to LOGITacker's USB keyboard interface", cmd_options_passiveenum_pass_keyboard),
    NRF_CLI_CMD(pass-through-mouse, &m_sub_options_on_off, "pass received mouse reports to LOGITacker's USB mouse interface", cmd_options_passiveenum_pass_mouse),
    NRF_CLI_CMD(pass-through-raw, &m_sub_options_on_off, "pass all received RF reports to LOGITacker's USB hidraw interface", cmd_options_passiveenum_pass_raw),
    NRF_CLI_SUBCMD_SET_END
};

// options discover onhit
NRF_CLI_CREATE_STATIC_SUBCMD_SET(m_sub_options_discover_onhit)
{
    NRF_CLI_CMD(continue,   NULL, "stay in discover mode.", cmd_discover_onhit_continue),
    NRF_CLI_CMD(active-enum, NULL, "enter active enumeration mode", cmd_discover_onhit_activeenum),
    NRF_CLI_CMD(passive-enum, NULL, "enter active enumeration mode", cmd_discover_onhit_passiveenum),
    NRF_CLI_CMD(auto-inject, NULL, "enter injection mode and execute injection", cmd_discover_onhit_autoinject),
    NRF_CLI_SUBCMD_SET_END
};

// options discover
NRF_CLI_CREATE_STATIC_SUBCMD_SET(m_sub_options_discover)
{
    NRF_CLI_CMD(pass-through-raw, &m_sub_options_on_off, "pass all received promiscuous RF reports to LOGITacker's USB hidraw interface", cmd_options_discover_pass_raw),
    NRF_CLI_CMD(onhit, &m_sub_options_discover_onhit, "select action to take when device a RF address is discovered", cmd_help),
    NRF_CLI_CMD(auto-store-plain-injectable, &m_sub_options_on_off, "automatically store discovered devices to flash if they allow plain injection", cmd_options_discover_autostoreplain),
    NRF_CLI_SUBCMD_SET_END
};

//options pair-sniff onsuccess
NRF_CLI_CREATE_STATIC_SUBCMD_SET(m_sub_options_pairsniff_onsuccess)
{
        NRF_CLI_CMD(continue, NULL, "continue to sniff pairing attempts", cmd_pair_sniff_onsuccess_continue),
        NRF_CLI_CMD(passive-enum, NULL, "start passive enumeration of newly paired device", cmd_pair_sniff_onsuccess_passiveenum),
        NRF_CLI_CMD(active-enum, NULL, "start passive enumeration of newly paired device", cmd_pair_sniff_onsuccess_activeenum),
        NRF_CLI_CMD(discover, NULL, "enter discover mode", cmd_pair_sniff_onsuccess_discover),
        NRF_CLI_CMD(auto-inject, NULL, "enter injection mode and execute injection", cmd_pair_sniff_onsuccess_autoinject),

        NRF_CLI_SUBCMD_SET_END
};


// options pair-sniff
NRF_CLI_CREATE_STATIC_SUBCMD_SET(m_sub_options_pairsniff)
{
    // ToDo: raw pass-through for pair sniff
    //NRF_CLI_CMD(pass-through-raw, &m_sub_options_on_off, "pass all pairing RF reports to LOGITacker's USB hidraw interface (not implemented)", NULL),
    NRF_CLI_CMD(pass-through-raw, NULL, "pass all pairing RF reports to LOGITacker's USB hidraw interface (not implemented)", NULL),
    NRF_CLI_CMD(onsuccess, &m_sub_options_pairsniff_onsuccess, "select action after a successful pairing has been captured", cmd_help),
    NRF_CLI_CMD(auto-store-pair-sniffed-devices, &m_sub_options_on_off, "automatically store devices after pairing has been sniffed successfully", cmd_options_pairsniff_autostoredevice),
    NRF_CLI_SUBCMD_SET_END
};

// options inject onsuccess
NRF_CLI_CREATE_STATIC_SUBCMD_SET(m_sub_options_inject_onsuccess)
{
    NRF_CLI_CMD(continue,   NULL, "stay in inject mode.", cmd_inject_onsuccess_continue),
    NRF_CLI_CMD(active-enum, NULL, "enter active enumeration", cmd_inject_onsuccess_activeenum),
    NRF_CLI_CMD(passive-enum, NULL, "enter active enumeration", cmd_inject_onsuccess_passiveenum),
    NRF_CLI_CMD(discover, NULL, "enter discover mode", cmd_inject_onsuccess_discover),
    NRF_CLI_SUBCMD_SET_END
};
// options inject onfail
NRF_CLI_CREATE_STATIC_SUBCMD_SET(m_sub_options_inject_onfail)
{
    NRF_CLI_CMD(continue,   NULL, "stay in inject mode.", cmd_inject_onfail_continue),
    NRF_CLI_CMD(active-enum, NULL, "enter active enumeration", cmd_inject_onfail_activeenum),
    NRF_CLI_CMD(passive-enum, NULL, "enter active enumeration", cmd_inject_onfail_passiveenum),
    NRF_CLI_CMD(discover, NULL, "enter discover mode", cmd_inject_onfail_discover),
    NRF_CLI_SUBCMD_SET_END
};

// options inject
NRF_CLI_CREATE_STATIC_SUBCMD_SET(m_sub_options_inject)
{
    NRF_CLI_CMD(language, NULL, "set injection keyboard language layout", cmd_options_inject_lang),

    NRF_CLI_CMD(default-script, &m_sub_options_inject_defaultscript, "name of inject script which is loaded at boot", cmd_options_inject_defaultscript),
    NRF_CLI_CMD(clear-default-script, NULL, "clear script which should be loaded at boot", cmd_options_inject_defaultscript_clear),
    NRF_CLI_CMD(auto-inject-count,   NULL, "maximum number of auto-injects per device", cmd_options_inject_maxautoinjectsperdevice),

    NRF_CLI_CMD(onsuccess, &m_sub_options_inject_onsuccess, "action after successful injection", cmd_help),
    NRF_CLI_CMD(onfail, &m_sub_options_inject_onfail, "action after failed injection", cmd_help),


    NRF_CLI_SUBCMD_SET_END
};

// options
NRF_CLI_CREATE_STATIC_SUBCMD_SET(m_sub_options)
{
    NRF_CLI_CMD(show, NULL, "print current options", cmd_options_show),
    NRF_CLI_CMD(store,   NULL, "store current options to flash (persist reboot)", cmd_options_store),
    NRF_CLI_CMD(erase,   NULL, "erase options from flash (defaults after reboot)", cmd_options_erase),

    NRF_CLI_CMD(passive-enum, &m_sub_options_passiveenum, "options for passive-enum mode", cmd_help),

    NRF_CLI_CMD(discover, &m_sub_options_discover, "options for discover mode", cmd_help),

    NRF_CLI_CMD(pair-sniff, &m_sub_options_pairsniff, "options for pair sniff mode", cmd_help),

    NRF_CLI_CMD(inject, &m_sub_options_inject, "options for inject mode", cmd_help),


    //NRF_CLI_CMD(pass-keyboard,   NULL, "pass-through keystrokes to USB keyboard", cmd_options_passiveenum_pass_keyboard),
    //NRF_CLI_CMD(pass-mouse,   NULL, "pass-through mouse moves to USB mouse", cmd_options_passiveenum_pass_mouse),

    NRF_CLI_SUBCMD_SET_END
};
NRF_CLI_CMD_REGISTER(options, &m_sub_options, "options", cmd_help);
