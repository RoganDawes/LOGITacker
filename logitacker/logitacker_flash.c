#include "fds.h"
#include "helper.h"
#include "logitacker_flash.h"
#include "sdk_common.h"
#include "fds.h"
#include "logitacker_script_engine.h"

#define NRF_LOG_MODULE_NAME LOGITACKER_FLASH
#include "nrf_log.h"



NRF_LOG_MODULE_REGISTER();

static bool m_fds_initialized = false;

static void wait_for_fds_ready(void)
{
    while (!m_fds_initialized) __WFE();
}

static void fds_callback(fds_evt_t const * p_evt)
{
    // runs in thread mode
    //helper_log_priority("fds_evt_handler");
    switch (p_evt->id)
    {
        case FDS_EVT_INIT:
            NRF_LOG_INFO("FDS_EVENT_INIT");
            if (p_evt->result == NRF_SUCCESS)
            {
                m_fds_initialized = true;
            }
            break;

        case FDS_EVT_WRITE:
        {
            NRF_LOG_DEBUG("FDS_EVENT_WRITE");
            if (p_evt->result == NRF_SUCCESS)
            {
                NRF_LOG_DEBUG("Record ID:\t0x%04x",  p_evt->write.record_id);
                NRF_LOG_DEBUG("File ID:\t0x%04x",    p_evt->write.file_id);
                NRF_LOG_DEBUG("Record key:\t0x%04x", p_evt->write.record_key);
            }
        } break;

        case FDS_EVT_DEL_RECORD:
        {
            NRF_LOG_DEBUG("FDS_EVENT_DEL_RECORD");
            if (p_evt->result == NRF_SUCCESS)
            {
                NRF_LOG_DEBUG("Record ID:\t0x%04x",  p_evt->del.record_id);
                NRF_LOG_DEBUG("File ID:\t0x%04x",    p_evt->del.file_id);
                NRF_LOG_DEBUG("Record key:\t0x%04x", p_evt->del.record_key);
            }
            //m_delete_all.pending = false;
        } break;

        default:
            break;
    }
}

uint32_t logitacker_flash_init() {
    uint32_t ret = fds_register(fds_callback);
    if (ret != NRF_SUCCESS) {
        NRF_LOG_ERROR("failed to initialize flash-storage, event handler registration failed: %d", ret);
        return ret;
    }
    NRF_LOG_INFO("fds_register main callback succeeded")

    ret = fds_register(logitacker_script_engine_fds_event_handler);
    if (ret != NRF_SUCCESS) {
        NRF_LOG_ERROR("failed to register FDS event handler for script processor: %d", ret);
        return ret;
    }
    NRF_LOG_INFO("fds_register script callback succeeded")

    ret = fds_init();
    if (ret != NRF_SUCCESS) {
        NRF_LOG_ERROR("failed to initialize flash-storage: %d", ret);
        return ret;
    }
    NRF_LOG_INFO("fds_init")

    wait_for_fds_ready();
    NRF_LOG_INFO("flash-storage initialized");

    if (fds_gc() == NRF_SUCCESS) NRF_LOG_INFO("garbage collection done");
    return NRF_SUCCESS;
}


uint32_t logitacker_flash_get_record_desc_for_device(fds_record_desc_t * p_record_desc, logitacker_devices_unifying_device_rf_address_t const rf_address);
uint32_t logitacker_flash_get_record_desc_for_dongle(fds_record_desc_t * p_record_desc, logitacker_devices_unifying_device_rf_addr_base_t const dongle_base_addr);


uint32_t logitacker_flash_store_device(logitacker_devices_unifying_device_t * p_device) {
    fds_record_t record;
    fds_record_desc_t record_desc;

    record.file_id = LOGITACKER_FLASH_FILE_ID_DEVICES;
    record.key = LOGITACKER_FLASH_RECORD_KEY_DEVICES;
    record.data.p_data = p_device;
    record.data.length_words = LOGITACKER_FLASH_RECORD_SIZE_UNIFYING_DEVICE;

    if (logitacker_flash_get_record_desc_for_device(&record_desc, p_device->rf_address) == NRF_SUCCESS) {
        NRF_LOG_INFO("device which should be stored to flash exists, updating ...")
        return fds_record_update(&record_desc, &record);
    }

    return fds_record_write(&record_desc, &record);
}

uint32_t logitacker_flash_list_stored_devices() {
    fds_find_token_t ftok;
    fds_record_desc_t record_desc;
    fds_flash_record_t flash_record;
    memset(&ftok, 0x00, sizeof(fds_find_token_t));

    char addr_str[LOGITACKER_DEVICE_ADDR_STR_LEN];
    NRF_LOG_INFO("Devices on flash");
    while(fds_record_find(LOGITACKER_FLASH_FILE_ID_DEVICES, LOGITACKER_FLASH_RECORD_KEY_DEVICES, &record_desc, &ftok) == NRF_SUCCESS) {
        if (fds_record_open(&record_desc, &flash_record) != NRF_SUCCESS) {
            NRF_LOG_WARNING("Failed to open record");
            continue; // go on with next
        }

        logitacker_devices_unifying_device_t const * p_device = flash_record.p_data;
        helper_addr_to_hex_str(addr_str, LOGITACKER_DEVICE_ADDR_LEN, p_device->rf_address);
        NRF_LOG_INFO("Stored device %s", nrf_log_push(addr_str));

        if (fds_record_close(&record_desc) != NRF_SUCCESS) {
            NRF_LOG_WARNING("Failed to close record");
        }
    }

    return NRF_SUCCESS;
}

uint32_t logitacker_flash_delete_device(logitacker_devices_unifying_device_rf_address_t const rf_address) {
    fds_find_token_t ftok;
    fds_record_desc_t record_desc;
    memset(&ftok, 0x00, sizeof(fds_find_token_t));

    if (logitacker_flash_get_record_desc_for_device(&record_desc, rf_address) == NRF_SUCCESS) {
        char addr_str[LOGITACKER_DEVICE_ADDR_STR_LEN];
        helper_addr_to_hex_str(addr_str, LOGITACKER_DEVICE_ADDR_LEN, rf_address);
        NRF_LOG_INFO("Deleting %s ...", nrf_log_push(addr_str));

        return fds_record_delete(&record_desc);
    }

    return NRF_ERROR_NOT_FOUND;
}

uint32_t logitacker_flash_get_device(logitacker_devices_unifying_device_t * p_device, logitacker_devices_unifying_device_rf_address_t const rf_address) {
    fds_record_desc_t record_desc;
    fds_flash_record_t flash_record;

    if (logitacker_flash_get_record_desc_for_device(&record_desc, rf_address) == NRF_SUCCESS) {
        char addr_str[LOGITACKER_DEVICE_ADDR_STR_LEN];
        uint32_t res = fds_record_open(&record_desc, &flash_record);
        if (res != NRF_SUCCESS) {
            NRF_LOG_WARNING("Failed to open record");
            return res;
        }

        memcpy(p_device, flash_record.p_data, sizeof(logitacker_devices_unifying_device_t));
        p_device->p_dongle = NULL; // we haven't got a dongle struct, yet
        helper_addr_to_hex_str(addr_str, LOGITACKER_DEVICE_ADDR_LEN, rf_address);
        NRF_LOG_INFO("Found %s ...", nrf_log_push(addr_str));

        if (fds_record_close(&record_desc) != NRF_SUCCESS) {
            NRF_LOG_WARNING("Failed to close record");
        }

        return NRF_SUCCESS;
    }

    return NRF_ERROR_NOT_FOUND;
}

uint32_t logitacker_flash_get_record_desc_for_device(fds_record_desc_t * p_record_desc, logitacker_devices_unifying_device_rf_address_t const rf_address) {
    fds_find_token_t ftok;
    fds_flash_record_t flash_record;
    memset(&ftok, 0x00, sizeof(fds_find_token_t));

    bool found = false;

    char addr_str[LOGITACKER_DEVICE_ADDR_STR_LEN];
    while(fds_record_find(LOGITACKER_FLASH_FILE_ID_DEVICES, LOGITACKER_FLASH_RECORD_KEY_DEVICES, p_record_desc, &ftok) == NRF_SUCCESS) {
        if (fds_record_open(p_record_desc, &flash_record) != NRF_SUCCESS) {
            NRF_LOG_WARNING("Failed to open record");
            continue; // go on with next
        }

        logitacker_devices_unifying_device_t const * p_device_tmp = flash_record.p_data;
        if (memcmp(p_device_tmp->rf_address, rf_address, sizeof(logitacker_devices_unifying_device_rf_address_t)) == 0) {
            helper_addr_to_hex_str(addr_str, LOGITACKER_DEVICE_ADDR_LEN, rf_address);
            //NRF_LOG_INFO("Found %s ...", nrf_log_push(addr_str));
            found = true;
        }


        if (fds_record_close(p_record_desc) != NRF_SUCCESS) {
            NRF_LOG_WARNING("Failed to close record");
        }

        if (found) return NRF_SUCCESS;
    }

    return NRF_ERROR_NOT_FOUND;
}



uint32_t logitacker_flash_get_next_device_for_dongle(logitacker_devices_unifying_device_t * p_device, fds_find_token_t * p_find_token, logitacker_devices_unifying_dongle_t * p_dongle) {
    fds_flash_record_t flash_record;
    fds_record_desc_t record_desc;
    bool found = false;

    logitacker_devices_unifying_device_rf_address_t tmp_addr = {0};
    helper_base_and_prefix_to_addr(tmp_addr, p_dongle->base_addr, 0x00, 5);

    char addr_str[LOGITACKER_DEVICE_ADDR_STR_LEN];
    while(fds_record_find(LOGITACKER_FLASH_FILE_ID_DEVICES, LOGITACKER_FLASH_RECORD_KEY_DEVICES, &record_desc, p_find_token) == NRF_SUCCESS) {
        if (fds_record_open(&record_desc, &flash_record) != NRF_SUCCESS) {
            NRF_LOG_WARNING("Failed to open record");
            continue; // go on with next
        }

        logitacker_devices_unifying_device_t const * p_device_tmp = flash_record.p_data;
        if (memcmp(p_device_tmp->rf_address, tmp_addr, 4) == 0) { // only compare first for address bytes (base address)
            helper_addr_to_hex_str(addr_str, LOGITACKER_DEVICE_ADDR_LEN, p_device_tmp->rf_address);
            NRF_LOG_INFO("Found device for dongle %s ...", nrf_log_push(addr_str));
            memcpy(p_device, p_device_tmp, sizeof(logitacker_devices_unifying_device_t));
            found = true;
        }


        if (fds_record_close(&record_desc) != NRF_SUCCESS) {
            NRF_LOG_WARNING("Failed to close record");
        }

        if (found) return NRF_SUCCESS;
    }

    return NRF_ERROR_NOT_FOUND;

}

// note: these functions only handle dongle data, not the device data referenced by pointers
uint32_t logitacker_flash_store_dongle(logitacker_devices_unifying_dongle_t * p_dongle) {
    fds_record_t record;
    fds_record_desc_t record_desc;

    record.file_id = LOGITACKER_FLASH_FILE_ID_DONGLES;
    record.key = LOGITACKER_FLASH_RECORD_KEY_DONGLES;
    record.data.p_data = p_dongle;
    record.data.length_words = LOGITACKER_FLASH_RECORD_SIZE_UNIFYING_DONGLE;

    if (logitacker_flash_get_record_desc_for_dongle(&record_desc, p_dongle->base_addr) == NRF_SUCCESS) {
        NRF_LOG_INFO("dongle which should be stored to flash exists, updating ...")
        return fds_record_update(&record_desc, &record);
    }

    return fds_record_write(&record_desc, &record);
}

uint32_t logitacker_flash_delete_dongle(logitacker_devices_unifying_device_rf_addr_base_t base_addr) {
    fds_find_token_t ftok;
    fds_record_desc_t record_desc;
    memset(&ftok, 0x00, sizeof(fds_find_token_t));

    if (logitacker_flash_get_record_desc_for_dongle(&record_desc, base_addr) == NRF_SUCCESS) {
        char base_addr_str[LOGITACKER_DEVICE_ADDR_STR_LEN];
        helper_addr_to_hex_str(base_addr_str, 4, base_addr);
        NRF_LOG_INFO("Deleting dongle with base addr %s ...", nrf_log_push(base_addr_str));

        return fds_record_delete(&record_desc);
    }

    return NRF_ERROR_NOT_FOUND;

}

uint32_t logitacker_flash_get_dongle(logitacker_devices_unifying_dongle_t * p_dongle, logitacker_devices_unifying_device_rf_addr_base_t const base_addr) {
    fds_record_desc_t record_desc;
    fds_flash_record_t flash_record;

    char base_addr_str[LOGITACKER_DEVICE_ADDR_STR_LEN];
    helper_addr_to_hex_str(base_addr_str, 4, base_addr);
    if (logitacker_flash_get_record_desc_for_dongle(&record_desc, base_addr) == NRF_SUCCESS) {
        uint32_t res = fds_record_open(&record_desc, &flash_record);
        if (res != NRF_SUCCESS) {
            NRF_LOG_WARNING("Failed to open record");
            return res;
        }

        memcpy(p_dongle, flash_record.p_data, sizeof(logitacker_devices_unifying_dongle_t));
        p_dongle->num_connected_devices = 0; // we haven't got the device structs, yet
        for (int i = 0; i < LOGITACKER_DEVICES_MAX_DEVICES_PER_DONGLE; i++) p_dongle->p_connected_devices[i] = NULL;


        NRF_LOG_INFO("Found dongle %s ...", nrf_log_push(base_addr_str));

        if (fds_record_close(&record_desc) != NRF_SUCCESS) {
            NRF_LOG_WARNING("Failed to close record");
        }

        return NRF_SUCCESS;
    } else {
        NRF_LOG_ERROR("dongle %s not found on flash", nrf_log_push(base_addr_str));
    }

    return NRF_ERROR_NOT_FOUND;

}

uint32_t logitacker_flash_list_stored_dongles() {
    fds_find_token_t ftok;
    fds_record_desc_t record_desc;
    fds_flash_record_t flash_record;
    memset(&ftok, 0x00, sizeof(fds_find_token_t));

    char base_addr_str[LOGITACKER_DEVICE_ADDR_STR_LEN];
    NRF_LOG_INFO("Dongles on flash:");
    while(fds_record_find(LOGITACKER_FLASH_FILE_ID_DONGLES, LOGITACKER_FLASH_RECORD_KEY_DONGLES, &record_desc, &ftok) == NRF_SUCCESS) {
        if (fds_record_open(&record_desc, &flash_record) != NRF_SUCCESS) {
            NRF_LOG_WARNING("Failed to open record");
            continue; // go on with next
        }

        logitacker_devices_unifying_dongle_t const * p_device = flash_record.p_data;
        helper_addr_to_hex_str(base_addr_str, 4, p_device->base_addr);
        NRF_LOG_INFO("Stored dongle %s", nrf_log_push(base_addr_str));

        if (fds_record_close(&record_desc) != NRF_SUCCESS) {
            NRF_LOG_WARNING("Failed to close record");
        }
    }

    return NRF_SUCCESS;
}

uint32_t logitacker_flash_get_dongle_for_device(logitacker_devices_unifying_dongle_t * p_dongle, logitacker_devices_unifying_device_t * p_device) {
    logitacker_devices_unifying_device_rf_addr_base_t base_addr;
    uint8_t prefix;
    helper_addr_to_base_and_prefix(base_addr, &prefix, p_device->rf_address, sizeof(logitacker_devices_unifying_device_rf_address_t));
    return logitacker_flash_get_dongle(p_dongle, base_addr);
}


uint32_t logitacker_flash_get_record_desc_for_dongle(fds_record_desc_t * p_record_desc, logitacker_devices_unifying_device_rf_addr_base_t const dongle_base_addr) {
    fds_find_token_t ftok;
    fds_flash_record_t flash_record;
    memset(&ftok, 0x00, sizeof(fds_find_token_t));

    bool found = false;


    while(fds_record_find(LOGITACKER_FLASH_FILE_ID_DONGLES, LOGITACKER_FLASH_RECORD_KEY_DONGLES, p_record_desc, &ftok) == NRF_SUCCESS) {
        if (fds_record_open(p_record_desc, &flash_record) != NRF_SUCCESS) {
            NRF_LOG_WARNING("Failed to open record");
            continue; // go on with next
        }

        logitacker_devices_unifying_dongle_t const * p_dongle_tmp = flash_record.p_data;
        if (memcmp(p_dongle_tmp->base_addr, dongle_base_addr, sizeof(logitacker_devices_unifying_device_rf_addr_base_t)) == 0) {
            found = true;
        }


        if (fds_record_close(p_record_desc) != NRF_SUCCESS) {
            NRF_LOG_WARNING("Failed to close record");
        }

        if (found) return NRF_SUCCESS;
    }

    return NRF_ERROR_NOT_FOUND;
}

