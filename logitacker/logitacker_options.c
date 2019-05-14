#include "fds.h"
#include "sdk_common.h"
#include "logitacker_options.h"
#include "logitacker_flash.h"

#define NRF_LOG_MODULE_NAME LOGITACKER_OPTIONS
#include "nrf_log.h"

NRF_LOG_MODULE_REGISTER();

logitacker_global_config_t g_logitacker_global_config = {
        .discovery_on_new_address_action = LOGITACKER_DISCOVERY_ON_NEW_ADDRESS_DO_NOTHING,
        .pass_through_mouse = false,
        .pass_through_keyboard = false,
};


uint32_t logitacker_options_update_flash(void) {
    fds_record_t record;
    fds_record_desc_t record_desc;
    fds_find_token_t find_token;
    memset(&find_token, 0, sizeof(find_token)); //assure token is zeroed out before usage

    // only search for file_id-key-combination once (not account for multiple entries)
    ret_code_t res = fds_record_find(LOGITACKER_FLASH_FILE_ID_GLOBAL_OPTIONS, LOGITACKER_FLASH_KEY_GLOBAL_OPTIONS_LOGITACKER, &record_desc, &find_token);
    if (res != FDS_SUCCESS) {
        NRF_LOG_WARNING("failed to find Flash Data Storage record for global options: %d", res);
        return res;
    }

    record.file_id = LOGITACKER_FLASH_FILE_ID_GLOBAL_OPTIONS;
    record.key = LOGITACKER_FLASH_KEY_GLOBAL_OPTIONS_LOGITACKER;
    record.data.p_data = &g_logitacker_global_config;
    record.data.length_words = (sizeof(g_logitacker_global_config) + 3) / 4;


    res = fds_record_update(&record_desc, &record);
    if (res != FDS_SUCCESS) {
        NRF_LOG_WARNING("update: failed to update Flash Data Storage record for global options: %d", res);
        return res;
    }



    return res == FDS_SUCCESS ? NRF_SUCCESS : res;

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
    if (res != FDS_SUCCESS) {
        NRF_LOG_WARNING("restore: failed to find Flash Data Storage record for global options: %d", res);
        restore_failed = true;
        goto finish;
    }

    res = fds_record_open(&record_desc, &flash_record);
    if (res != FDS_SUCCESS) {
        NRF_LOG_WARNING("restore: failed to open Flash Data Storage record for global options: %d", res);
        restore_failed = true;
        goto finish;
    }

    memcpy(&g_logitacker_global_config, flash_record.p_data, sizeof(g_logitacker_global_config));

    res = fds_record_close(&record_desc);
    if (res != FDS_SUCCESS) {
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

void logitacker_options_print_stats() {
    NRF_LOG_INFO("stats:\n=====");
    NRF_LOG_INFO("\tboot count:            %d", g_logitacker_global_config.stats.boot_count);
}

void logitacker_options_print() {
    NRF_LOG_INFO("global options:\n==============");
    NRF_LOG_INFO("\tdiscovery on new address action              :            %d", g_logitacker_global_config.discovery_on_new_address_action);
    NRF_LOG_INFO("\tpass-trough sniffed keyboard data to USB HID :            %s", g_logitacker_global_config.pass_through_keyboard ? "on" : "off");
    NRF_LOG_INFO("\tpass-trough sniffed mouse data to USB HID    :            %s", g_logitacker_global_config.pass_through_mouse ? "on" : "off");
}