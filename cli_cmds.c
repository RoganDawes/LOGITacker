#include <ctype.h>
#include "nrf_cli.h"
#include "nrf_log.h"
#include "sdk_common.h"

#define CLI_EXAMPLE_MAX_CMD_CNT (20u)
#define CLI_EXAMPLE_MAX_CMD_LEN (33u)
#define CLI_EXAMPLE_VALUE_BIGGER_THAN_STACK     (20000u)

uint32_t m_counter;
bool     m_counter_active = false;

/* Command handlers */
static void cmd_print_param(nrf_cli_t const * p_cli, size_t argc, char **argv)
{
    for (size_t i = 1; i < argc; i++)
    {
        nrf_cli_fprintf(p_cli, NRF_CLI_NORMAL, "argv[%d] = %s\r\n", i, argv[i]);
    }
}

static void cmd_print_all(nrf_cli_t const * p_cli, size_t argc, char **argv)
{
    for (size_t i = 1; i < argc; i++)
    {
        nrf_cli_fprintf(p_cli, NRF_CLI_NORMAL, "%s ", argv[i]);
    }
    nrf_cli_fprintf(p_cli, NRF_CLI_NORMAL, "\r\n");
}

static void cmd_print(nrf_cli_t const * p_cli, size_t argc, char **argv)
{
    ASSERT(p_cli);
    ASSERT(p_cli->p_ctx && p_cli->p_iface && p_cli->p_name);

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

static void cmd_python(nrf_cli_t const * p_cli, size_t argc, char **argv)
{
    UNUSED_PARAMETER(argc);
    UNUSED_PARAMETER(argv);

    nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "Nice joke ;)\r\n");
}

static void cmd_counter_start(nrf_cli_t const * p_cli, size_t argc, char **argv)
{
    if (argc != 1)
    {
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "%s: bad parameter count\r\n", argv[0]);
        return;
    }

    m_counter_active = true;
}

static void cmd_counter_stop(nrf_cli_t const * p_cli, size_t argc, char **argv)
{
    if (argc != 1)
    {
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "%s: bad parameter count\r\n", argv[0]);
        return;
    }

    m_counter_active = false;
}

static void cmd_counter_reset(nrf_cli_t const * p_cli, size_t argc, char **argv)
{
    if (argc != 1)
    {
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "%s: bad parameter count\r\n", argv[0]);
        return;
    }

    m_counter = 0;
}

static void cmd_counter(nrf_cli_t const * p_cli, size_t argc, char **argv)
{
    ASSERT(p_cli);
    ASSERT(p_cli->p_ctx && p_cli->p_iface && p_cli->p_name);

    /* Extra defined dummy option */
    static const nrf_cli_getopt_option_t opt[] = {
        NRF_CLI_OPT(
            "--test",
            "-t",
            "dummy option help string"
        )
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

    if (!strcmp(argv[1], "-t") || !strcmp(argv[1], "--test"))
    {
        nrf_cli_fprintf(p_cli, NRF_CLI_NORMAL, "Dummy test option.\r\n");
        return;
    }

    /* subcommands have their own handlers and they are not processed here */
    nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "%s: unknown parameter: %s\r\n", argv[0], argv[1]);
}

static void cmd_nordic(nrf_cli_t const * p_cli, size_t argc, char **argv)
{
    UNUSED_PARAMETER(argc);
    UNUSED_PARAMETER(argv);

    if (nrf_cli_help_requested(p_cli))
    {
        nrf_cli_help_print(p_cli, NULL, 0);
        return;
    }

    nrf_cli_fprintf(p_cli, NRF_CLI_OPTION,
                    "\r\n"
                    "            .co:.                   'xo,          \r\n"
                    "         .,collllc,.             'ckOOo::,..      \r\n"
                    "      .:ooooollllllll:'.     .;dOOOOOOo:::;;;'.   \r\n"
                    "   'okxddoooollllllllllll;'ckOOOOOOOOOo:::;;;,,,' \r\n"
                    "   OOOkxdoooolllllllllllllllldxOOOOOOOo:::;;;,,,'.\r\n"
                    "   OOOOOOkdoolllllllllllllllllllldxOOOo:::;;;,,,'.\r\n"
                    "   OOOOOOOOOkxollllllllllllllllllcccldl:::;;;,,,'.\r\n"
                    "   OOOOOOOOOOOOOxdollllllllllllllccccc::::;;;,,,'.\r\n"
                    "   OOOOOOOOOOOOOOOOkxdlllllllllllccccc::::;;;,,,'.\r\n"
                    "   kOOOOOOOOOOOOOOOOOOOkdolllllllccccc::::;;;,,,'.\r\n"
                    "   kOOOOOOOOOOOOOOOOOOOOOOOxdllllccccc::::;;;,,,'.\r\n"
                    "   kOOOOOOOOOOOOOOOOOOOOOOOOOOkxolcccc::::;;;,,,'.\r\n"
                    "   kOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOkdlc::::;;;,,,'.\r\n"
                    "   xOOOOOOOOOOOxdkOOOOOOOOOOOOOOOOOOOOxoc:;;;,,,'.\r\n"
                    "   xOOOOOOOOOOOdc::ldkOOOOOOOOOOOOOOOOOOOkdc;,,,''\r\n"
                    "   xOOOOOOOOOOOdc::;;,;cdkOOOOOOOOOOOOOOOOOOOxl;''\r\n"
                    "   .lkOOOOOOOOOdc::;;,,''..;oOOOOOOOOOOOOOOOOOOOx'\r\n"
                    "      .;oOOOOOOdc::;;,.       .:xOOOOOOOOOOOOd;.  \r\n"
                    "          .:xOOdc:,.              'ckOOOOkl'      \r\n"
                    "             .od'                    'xk,         \r\n"
                    "\r\n");

    nrf_cli_fprintf(p_cli,NRF_CLI_NORMAL,
                    "                Nordic Semiconductor              \r\n\r\n");
}



/**
 * @brief Command set array
 * */
NRF_CLI_CMD_REGISTER(nordic, NULL, "Print Nordic Semiconductor logo.", cmd_nordic);

NRF_CLI_CREATE_STATIC_SUBCMD_SET(m_sub_print)
{
    NRF_CLI_CMD(all,   NULL, "Print all entered parameters.", cmd_print_all),
    NRF_CLI_CMD(param, NULL, "Print each parameter in new line.", cmd_print_param),
    NRF_CLI_SUBCMD_SET_END
};
NRF_CLI_CMD_REGISTER(print, &m_sub_print, "print", cmd_print);

NRF_CLI_CMD_REGISTER(python, NULL, "python", cmd_python);

NRF_CLI_CREATE_STATIC_SUBCMD_SET(m_sub_counter)
{
    NRF_CLI_CMD(reset,  NULL, "Reset seconds counter.",  cmd_counter_reset),
    NRF_CLI_CMD(start,  NULL, "Start seconds counter.",  cmd_counter_start),
    NRF_CLI_CMD(stop,   NULL, "Stop seconds counter.",   cmd_counter_stop),
    NRF_CLI_SUBCMD_SET_END
};
NRF_CLI_CMD_REGISTER(counter, &m_sub_counter, "Display seconds on terminal screen", cmd_counter);

