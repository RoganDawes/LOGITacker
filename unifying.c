#include <string.h> // for memcpy
#include "nrf.h"
#include "flash_device_info.h"
#include "fds.h"
#include "unifying.h"
#include "nrf_esb_illegalmod.h"
#include "nrf_log.h"
#include "nrf_delay.h"
#include "radio.h"
#include "app_timer.h"
#include "timestamp.h"
#include "app_scheduler.h"

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


uint32_t timestamp_last = 0;
uint32_t timestamp = 0;
uint32_t timestamp_delta = 0;
static unifying_rf_record_t pipe_record_buf[NRF_ESB_PIPE_COUNT][UNIFYING_MAX_STORED_REPORTS_PER_PIPE];
static uint8_t pipe_record_buf_pos[NRF_ESB_PIPE_COUNT];

bool addRecord(nrf_esb_payload_t frame) {
    NRF_LOG_INFO("addRecord");
    bool result = false;
    if (frame.length < 5) return false; //ToDo: with error
    
    uint8_t pos = pipe_record_buf_pos[frame.pipe];
    unifying_rf_record_t* p_record = &pipe_record_buf[frame.pipe][pos];
    
    p_record->length = frame.length;
    p_record->reportType = frame.data[1] & UNIFYING_RF_REPORT_TYPE_MSK;
    memcpy(p_record->data, frame.data, frame.length);


    timestamp_last = timestamp;
    timestamp = timestamp_get();
    timestamp_delta = timestamp - timestamp_last;
    p_record->pre_delay_ms = timestamp_delta;
    if (pos == 0) p_record->pre_delay_ms = 0; //no delay for first record

NRF_LOG_INFO("Report stored at timer count %d, last %d, diff %d", timestamp, timestamp_last, timestamp_delta);


    pos++;
    if (pos >= UNIFYING_MAX_STORED_REPORTS_PER_PIPE) {
        result = true;
        pos = 0;
    }
    pipe_record_buf_pos[frame.pipe] = pos;

    return result;
}

void unifying_replay_records_scheduler_handler(void *p_event_data, uint16_t event_size) {
    unifying_replay_records(*((uint8_t*) p_event_data));
}

void unifying_frame_classify(nrf_esb_payload_t frame) { 
    bool logKeepAliveEmpty = false;

    //filter out frames < 5 byte length (likely ACKs)
    if (frame.length < 5) return;

    uint8_t reportType = frame.data[1]; // byte 1 is rf report type, byte 0 is device prefix
    bool keepAliveSet = (reportType & 0x40) != 0;
    reportType &= UNIFYING_RF_REPORT_TYPE_MSK; //mask report type
    uint32_t counter;

    //filter out Unifying keep alive
    switch (reportType) {
        case UNIFYING_RF_REPORT_PLAIN_KEYBOARD:
            NRF_LOG_INFO("Unencrypted keyboard");
            return;
        case UNIFYING_RF_REPORT_PLAIN_MOUSE:
            NRF_LOG_INFO("Unencrypted mouse"); 
            return;
        case UNIFYING_RF_REPORT_PLAIN_MULTIMEDIA:
            NRF_LOG_INFO("Unencrypted multimedia key");
            return;
        case UNIFYING_RF_REPORT_PLAIN_SYSTEM_CTL:
            NRF_LOG_INFO("Unencrypted system control key");
            return;
        case UNIFYING_RF_REPORT_LED:
            NRF_LOG_INFO("LED (outbound)");
            return;
        case UNIFYING_RF_REPORT_SET_KEEP_ALIVE:
            NRF_LOG_INFO("Set keep-alive");
            return;
        case UNIFYING_RF_REPORT_HIDPP_SHORT:
            NRF_LOG_INFO("HID++ short");
            return;
        case UNIFYING_RF_REPORT_HIDPP_LONG:
            NRF_LOG_INFO("HID++ long");
            return;
        case UNIFYING_RF_REPORT_ENCRYPTED_KEYBOARD:
            counter = frame.data[10] << 24 | frame.data[11] << 16 | frame.data[12] << 8 | frame.data[13];

            NRF_LOG_INFO("Encrypted keyboard, counter %08x", counter);
            bool full_capture = addRecord(frame);
            if (full_capture) {
                NRF_LOG_INFO("scheduling replay");
                app_sched_event_put(&frame.pipe, sizeof(frame.pipe), unifying_replay_records_scheduler_handler);
                //unifying_replay_records(frame.pipe);
            }
            return;
        case UNIFYING_RF_REPORT_PAIRING:
            NRF_LOG_INFO("Pairing");
            return;
        case 0x00:
            if (keepAliveSet && logKeepAliveEmpty) NRF_LOG_INFO("Empty keep alive");
            return;
        default:
            NRF_LOG_INFO("Unknown frame type %02x, keep alive %s", frame.data[1], keepAliveSet ? "set" : "not set");
            return;
    }
}


void unifying_replay_records(uint8_t pipe_num) {
    NRF_LOG_INFO("Replaying %d frames", UNIFYING_MAX_STORED_REPORTS_PER_PIPE);
    static nrf_esb_payload_t tx_payload;
    uint8_t current_pos = pipe_record_buf_pos[pipe_num];

    
    unifying_rf_record_t* p_record = &pipe_record_buf[pipe_num][current_pos];
p_record->pre_delay_ms = 5000;
    NRF_LOG_INFO("... frame %d in %d ms", current_pos, p_record->pre_delay_ms);
    memcpy(tx_payload.data, p_record->data, p_record->length);
    tx_payload.length = p_record->length;
    tx_payload.pipe = pipe_num;
    tx_payload.noack = false;
            
    //unifying_payload_update_checksum(tx_payload.data, tx_payload.length); 

    radioTransmitDelayed(&tx_payload, 5000);

    
}

/*
void unifying_replay_records(uint8_t pipe_num) {
    NRF_LOG_INFO("Replaying %d frames", UNIFYING_MAX_STORED_REPORTS_PER_PIPE);
    static nrf_esb_payload_t tx_payload; // = NRF_ESB_CREATE_PAYLOAD(0, 0x01, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00);

    uint8_t start_pos = pipe_record_buf_pos[pipe_num];

    for (uint8_t pos=start_pos; pos < (start_pos+UNIFYING_MAX_STORED_REPORTS_PER_PIPE); pos++) {
        uint8_t current_pos = pos < UNIFYING_MAX_STORED_REPORTS_PER_PIPE ? pos : pos - UNIFYING_MAX_STORED_REPORTS_PER_PIPE;
        unifying_rf_record_t* p_record = &pipe_record_buf[pipe_num][current_pos];
        NRF_LOG_INFO("... frame %d in %d ms", current_pos, p_record->pre_delay_ms);
        memcpy(tx_payload.data, p_record->data, p_record->length);
        tx_payload.length = p_record->length;
        tx_payload.pipe = pipe_num;
        tx_payload.noack = false;
        
        nrf_delay_ms(p_record->pre_delay_ms); //ToDo: MIN delay is 8 ms
        //unifying_payload_update_checksum(tx_payload.data, tx_payload.length); 
        if (radioTransmit(&tx_payload, true)) {
            //NRF_LOG_INFO("TX success");
        } else {
            //NRF_LOG_INFO("TX fail");
        }

        
    }
}
*/