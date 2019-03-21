#include <string.h> // for memcpy
#include "nrf.h"
#include "flash_device_info.h"
#include "fds.h"
#include "unifying.h"

uint32_t restoreDeviceInfoFromFlash(uint16_t deviceRecordIndex, device_info_t *deviceInfo) {
    ret_code_t ret;
    uint16_t recordIdx = FLASH_RECORD_PREFIX_DEVICES_DEVICE_INFO | deviceRecordIndex;

    //Try to load first state record from flash, create if not existing
    fds_record_desc_t desc = {0}; //used as ref to current record
    fds_find_token_t  tok  = {0}; //used for find functions if records with same ID exist and we want to find next one

    ret = fds_record_find(FLASH_FILE_DEVICES, recordIdx, &desc, &tok);
    if (ret != FDS_SUCCESS) { return ret; }

    /* A config file is in flash. Let's update it. */
    fds_flash_record_t record_state = {0};

    // Open the record and read its contents. */
    ret = fds_record_open(&desc, &record_state);
    if (ret != FDS_SUCCESS) { return ret; }

    // restore state from flash into state (copy)
    memcpy(deviceInfo, record_state.p_data, sizeof(device_info_t));

    return fds_record_close(&desc);
}

uint32_t restoreDeviceWhitenedReportsFromFlash(uint16_t deviceRecordIndex, whitened_replay_frames_t *reports) {
    ret_code_t ret;
    uint16_t recordIdx = FLASH_RECORD_PREFIX_DEVICES_DEVICE_WHITENED_KEY_REPORTS | deviceRecordIndex;

    //Try to load first state record from flash, create if not existing
    fds_record_desc_t desc = {0}; //used as ref to current record
    fds_find_token_t  tok  = {0}; //used for find functions if records with same ID exist and we want to find next one

    ret = fds_record_find(FLASH_FILE_DEVICES, recordIdx, &desc, &tok);
    if (ret != FDS_SUCCESS) { return ret; }

    /* A config file is in flash. Let's update it. */
    fds_flash_record_t record_state = {0};

    // Open the record and read its contents. */
    ret = fds_record_open(&desc, &record_state);
    if (ret != FDS_SUCCESS) { return ret; }

    // restore state from flash into state (copy)
    memcpy(reports, record_state.p_data, sizeof(whitened_replay_frames_t));

    return fds_record_close(&desc);
}

uint32_t updateDeviceInfoOnFlash(uint16_t deviceRecordIndex, device_info_t *deviceInfo) {
    ret_code_t ret;
    uint16_t recordIdx = FLASH_RECORD_PREFIX_DEVICES_DEVICE_INFO | deviceRecordIndex;
    
    // flash record for device info
    fds_record_t flash_record = {
        .file_id           = FLASH_FILE_DEVICES,
        .key               = recordIdx,
        .data.p_data       = deviceInfo,
        // The length of a record is always expressed in 4-byte units (words).
        .data.length_words = (sizeof(*deviceInfo) + 3) / sizeof(uint32_t),
    };
    

    //Try to load first state record from flash, create if not existing
    fds_record_desc_t desc = {0}; //used as ref to current record
    fds_find_token_t  tok  = {0}; //used for find functions if records with same ID exist and we want to find next one

    ret = fds_record_find(FLASH_FILE_DEVICES, recordIdx, &desc, &tok);
    if (ret == FDS_SUCCESS) { 
        // Write the updated record to flash.
        return fds_record_update(&desc, &flash_record);
    } else {
        // no found, create new flash record
        return fds_record_write(&desc, &flash_record);
    }
}

uint32_t updateDeviceWhitenedReportsOnFlash(uint16_t deviceRecordIndex, whitened_replay_frames_t *reports) {
    ret_code_t ret;
    uint16_t recordIdx = FLASH_RECORD_PREFIX_DEVICES_DEVICE_WHITENED_KEY_REPORTS | deviceRecordIndex;

    // flash record for whitened reports
    fds_record_t flash_record = {
        .file_id           = FLASH_FILE_DEVICES,
        .key               = recordIdx,
        .data.p_data       = reports,
        // The length of a record is always expressed in 4-byte units (words).
        .data.length_words = (sizeof(*reports) + 3) / sizeof(uint32_t),
    };
    
    //Try to load first state record from flash, create if not existing
    fds_record_desc_t desc = {0}; //used as ref to current record
    fds_find_token_t  tok  = {0}; //used for find functions if records with same ID exist and we want to find next one

    ret = fds_record_find(FLASH_FILE_DEVICES, recordIdx, &desc, &tok);
    if (ret == FDS_SUCCESS) { 
        // Write the updated record to flash.
        return fds_record_update(&desc, &flash_record);
    } else {
        // no found, create new flash record
        return fds_record_write(&desc, &flash_record);
    }
}

uint8_t unifying_calculate_checksum(uint8_t * p_array, uint8_t paylen) {
/*
	chksum := byte(0xff)
	for i := 0; i < len(payload)-1; i++ {
		chksum = (chksum - payload[i]) & 0xff
	}
	chksum = (chksum + 1) & 0xff

	payload[len(payload)-1] = chksum

*/

    uint8_t checksum = 0x00;
    for (int i = 0; i < paylen; i++) {
        checksum -= p_array[i];
    }
    //checksum++;

    return checksum;
}

bool unifying_validate_payload(uint8_t * p_array, uint8_t paylen) {
    if (paylen < 1) return false;
    uint8_t chksum = unifying_calculate_checksum(p_array, paylen);
    p_array[0] = chksum;

    return true;
}

bool unifying_payload_update_checksum(uint8_t * p_array, uint8_t paylen) {
    if (paylen < 1) return false;
    uint8_t chksum = unifying_calculate_checksum(p_array, paylen-1);
    p_array[paylen-1] = chksum;

    return true;
}

