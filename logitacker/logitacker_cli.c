#include <ctype.h>
#include <logitacker_tx_payload_provider.h>
#include <logitacker_tx_pay_provider_string_to_keys.h>
#include <logitacker_processor_passive_enum.h>
#include <logitacker_options.h>
#include "logitacker_keyboard_map.h"
#include "nrf_cli.h"
#include "nrf_log.h"
#include "sdk_common.h"
#include "logitacker.h"
#include "logitacker_devices.h"
#include "helper.h"
#include "unifying.h"

#define CLI_EXAMPLE_MAX_CMD_CNT (20u)
#define CLI_EXAMPLE_MAX_CMD_LEN (33u)
#define CLI_EXAMPLE_VALUE_BIGGER_THAN_STACK     (20000u)

static void cmd_test(nrf_cli_t const * p_cli, size_t argc, char **argv)
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
        logitacker_injection_string(LANGUAGE_LAYOUT_DE, "Hello World!");

        return;
    } else {
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "device address needed, format has to be xx:xx:xx:xx:xx\r\n");
        return;

    }

}

static void cmd_inject(nrf_cli_t const * p_cli, size_t argc, char **argv)
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

static void cmd_inject_string(nrf_cli_t const * p_cli, size_t argc, char **argv)
{
    //let's inject a 5s delay upfront
    logitacker_injection_delay(5000);

    for (int i=1; i<argc;i++) {
        logitacker_injection_string(LANGUAGE_LAYOUT_DE, argv[i]);
        logitacker_injection_string(LANGUAGE_LAYOUT_DE, " ");
    }
}


static void cmd_discover_onhit(nrf_cli_t const * p_cli, size_t argc, char **argv)
{
        /* Extra defined dummy option */
    static const nrf_cli_getopt_option_t opt[] = {
        NRF_CLI_OPT("continue","c", "stay in discovery mode when RF address found"),
        NRF_CLI_OPT("passive_enum", "p","start passive enumeration when RF address found"),
        NRF_CLI_OPT("active_enum", "a","start active enumeration when RF address found")
        
    };

    if ((argc == 1) || nrf_cli_help_requested(p_cli))
    {
        nrf_cli_help_print(p_cli, opt, ARRAY_SIZE(opt));
        return;
    }

    if (argc != 2)
    {
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "%s: bad parameter count\r\n", argv[0]);
        return;
    }

    if (!strcmp(argv[1], "continue") || !strcmp(argv[1], "c"))
    {
        g_logitacker_global_config.discovery_on_new_address_action = LOGITACKER_DISCOVERY_ON_NEW_ADDRESS_DO_NOTHING;
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "on-hit action: continue\r\n");
        return;
    } else if (!strcmp(argv[1], "passive_enum") || !strcmp(argv[1], "p")) {
        g_logitacker_global_config.discovery_on_new_address_action = LOGITACKER_DISCOVERY_ON_NEW_ADDRESS_SWITCH_PASSIVE_ENUMERATION;
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "on-hit action: start passive enumeration of new RF address\r\n");
        return;
    } else if (!strcmp(argv[1], "active_enum") || !strcmp(argv[1], "a")) {
        g_logitacker_global_config.discovery_on_new_address_action = LOGITACKER_DISCOVERY_ON_NEW_ADDRESS_SWITCH_ACTIVE_ENUMERATION;
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "on-hit action: start active enumeration of new RF address\r\n");
        return;
    } else {
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "No mode change\r\n");
    }

}

static void cmd_pairing_sniff(nrf_cli_t const * p_cli, size_t argc, char **argv)
{
    logitacker_enter_mode_pairing_sniff();
    /*
    for (size_t i = 1; i < argc; i++)
    {
        nrf_cli_fprintf(p_cli, NRF_CLI_NORMAL, "%s ", argv[i]);
    }
    nrf_cli_fprintf(p_cli, NRF_CLI_NORMAL, "\r\n");
    */
}

static void cmd_pairing_run(nrf_cli_t const * p_cli, size_t argc, char **argv)
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

static void cmd_pairing(nrf_cli_t const * p_cli, size_t argc, char **argv)
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

static void cmd_discover_run(nrf_cli_t const * p_cli, size_t argc, char **argv)
{
    logitacker_enter_mode_discovery();
    /*
    for (size_t i = 1; i < argc; i++)
    {
        nrf_cli_fprintf(p_cli, NRF_CLI_NORMAL, "%s ", argv[i]);
    }
    nrf_cli_fprintf(p_cli, NRF_CLI_NORMAL, "\r\n");
    */
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

static char addr_str_buff[LOGITACKER_DEVICE_ADDR_STR_LEN] = {0};
static uint8_t tmp_addr[LOGITACKER_DEVICE_ADDR_LEN];
static void cmd_devices(nrf_cli_t const * p_cli, size_t argc, char **argv) {
    for (int i=0; i<LOGITACKER_DEVICES_MAX_LIST_ENTRIES; i++) {
        logitacker_device_set_t *p_device_set = logitacker_device_set_list_get(i);
        if (p_device_set != NULL) {

            for (int prefix_index=0; prefix_index < p_device_set->num_device_prefixes; prefix_index++) {
                uint8_t prefix = p_device_set->device_prefixes[prefix_index];
                logitacker_device_capabilities_t * p_caps = &p_device_set->capabilities[prefix_index];
                logitacker_device_frame_counter_t * p_counters = &p_device_set->frame_counters[prefix_index];

                helper_base_and_prefix_to_addr(tmp_addr, p_device_set->base_addr, prefix, 5);
                helper_addr_to_hex_str(addr_str_buff, LOGITACKER_DEVICE_ADDR_LEN, tmp_addr);

                nrf_cli_vt100_color_t outcol = NRF_CLI_VT100_COLOR_DEFAULT;
                if (p_device_set->is_logitech) outcol = NRF_CLI_VT100_COLOR_BLUE;
                if (p_caps->vuln_forced_pairing) outcol = NRF_CLI_VT100_COLOR_YELLOW;
                if (p_caps->vuln_plain_injection) outcol = NRF_CLI_VT100_COLOR_GREEN;
                if (p_caps->key_known) outcol = NRF_CLI_VT100_COLOR_RED;
                nrf_cli_fprintf(p_cli, outcol, "%s (activity %d, Logitech: %d, plain keystroke injection %d, forced pairing %d, link key known %d)\r\n", addr_str_buff, p_counters->overal, p_device_set->is_logitech, p_caps->vuln_plain_injection, p_caps->vuln_forced_pairing, p_caps->key_known);
            }
        }
    }
    
}

static void cmd_options(nrf_cli_t const * p_cli, size_t argc, char **argv)
{
    if ((argc == 1) || nrf_cli_help_requested(p_cli))
    {
        nrf_cli_help_print(p_cli, NULL, 0);

        nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "\r\ncurrent options\r\n===============\r\n", g_logitacker_global_config.pass_through_keyboard ? "on" : "off");
        nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "\tkeyboard pass-through: %s\r\n", g_logitacker_global_config.pass_through_keyboard ? "on" : "off");
        nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "\tmouse pass-through: %s\r\n", g_logitacker_global_config.pass_through_mouse ? "on" : "off");

        return;
    }

    if (argc != 2)
    {
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "%s: bad parameter count\r\n", argv[0]);
        return;
    }


}

static void cmd_options_pass_keyboard(nrf_cli_t const * p_cli, size_t argc, char **argv)
{
    if (argc > 1)
    {
        if (strcmp(argv[1], "off") == 0 || strcmp(argv[1], "OFF") == 0) g_logitacker_global_config.pass_through_keyboard = false;
        else if (strcmp(argv[1], "on") == 0 || strcmp(argv[1], "ON") == 0) g_logitacker_global_config.pass_through_keyboard = true;
    }

    nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "keyboard pass-through: %s\r\n", g_logitacker_global_config.pass_through_keyboard ? "on" : "off");
}

static void cmd_options_pass_mouse(nrf_cli_t const * p_cli, size_t argc, char **argv)
{
    if (argc > 1)
    {
        if (strcmp(argv[1], "off") == 0 || strcmp(argv[1], "OFF") == 0) g_logitacker_global_config.pass_through_mouse = false;
        else if (strcmp(argv[1], "on") == 0 || strcmp(argv[1], "ON") == 0) g_logitacker_global_config.pass_through_mouse = true;

    }

    nrf_cli_fprintf(p_cli, NRF_CLI_DEFAULT, "mouse pass-through: %s\r\n", g_logitacker_global_config.pass_through_mouse ? "on" : "off");
}


NRF_CLI_CMD_REGISTER(test, NULL, "Debug command to test code", cmd_test);


NRF_CLI_CREATE_STATIC_SUBCMD_SET(m_sub_options)
{
    NRF_CLI_CMD(pass-keyboard,   NULL, "pass-through keystrokes to USB keyboard", cmd_options_pass_keyboard),
    NRF_CLI_CMD(pass-mouse,   NULL, "pass-through mouse moves to USB mouse", cmd_options_pass_mouse),
    NRF_CLI_SUBCMD_SET_END
};
NRF_CLI_CMD_REGISTER(options, &m_sub_options, "options", cmd_options);

NRF_CLI_CREATE_STATIC_SUBCMD_SET(m_sub_discover)
{
    NRF_CLI_CMD(run,   NULL, "Enter discovery mode.", cmd_discover_run),
    NRF_CLI_CMD(onhit, NULL, "Set behavior on discovered address.", cmd_discover_onhit),
    NRF_CLI_SUBCMD_SET_END
};
NRF_CLI_CMD_REGISTER(discover, &m_sub_discover, "discover", cmd_discover);

NRF_CLI_CREATE_STATIC_SUBCMD_SET(m_sub_pairing)
{
    NRF_CLI_CMD(sniff,   NULL, "Sniff pairing.", cmd_pairing_sniff),
    NRF_CLI_CMD(run,   NULL, "Sniff pairing.", cmd_pairing_run),
    NRF_CLI_SUBCMD_SET_END
};
NRF_CLI_CMD_REGISTER(pairing, &m_sub_pairing, "discover", cmd_pairing);

NRF_CLI_CREATE_STATIC_SUBCMD_SET(m_sub_inject)
{
    NRF_CLI_CMD(string,   NULL, "inject given string", cmd_inject_string),
    NRF_CLI_SUBCMD_SET_END
};
NRF_CLI_CMD_REGISTER(inject, &m_sub_inject, "injection", cmd_inject);

NRF_CLI_CMD_REGISTER(devices, NULL, "List discovered devices", cmd_devices);

