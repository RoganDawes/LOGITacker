#include "logitacker_unifying.h"
#include "string.h" // for memcpy
#include "nrf.h"
#include "fds.h"
#include "nrf_esb_illegalmod.h"
#include "nrf_log.h"
#include "nrf_delay.h"
#include "logitacker_radio.h"
#include "app_timer.h"
#include "app_scheduler.h"


uint8_t logitacker_unifying_calculate_checksum(uint8_t *p_array, uint8_t paylen) {
    uint8_t checksum = 0x00;
    for (int i = 0; i < paylen; i++) {
        checksum -= p_array[i];
    }
    //checksum++;
    return checksum;
}


bool logitacker_unifying_payload_update_checksum(uint8_t *p_array, uint8_t paylen) {
    if (paylen < 1) return false;
    uint8_t chksum = logitacker_unifying_calculate_checksum(p_array, paylen - 1);
    p_array[paylen-1] = chksum;

    return true;
}

bool logiteacker_unifying_payload_validate_checksum(uint8_t *p_array, uint8_t paylen) {
    if (paylen < 1) return false;
    uint8_t chksum = logitacker_unifying_calculate_checksum(p_array, paylen - 1);
    return p_array[paylen-1] == chksum;
}

uint32_t logitacker_unifying_extract_counter_from_encrypted_keyboard_frame(nrf_esb_payload_t frame, uint32_t *p_counter) {
    // assure frame is encrypted keyboard
    if (frame.length != 22) return NRF_ERROR_INVALID_LENGTH;
    if ((frame.data[1] & 0x1f) != UNIFYING_RF_REPORT_ENCRYPTED_KEYBOARD) return NRF_ERROR_INVALID_DATA;
    *p_counter = frame.data[10] << 24 | frame.data[11] << 16 | frame.data[12] << 8 | frame.data[13];
    return NRF_SUCCESS;
}

void logitacker_unifying_frame_classify(nrf_esb_payload_t frame, uint8_t *p_outRFReportType, bool *p_outHasKeepAliveSet) {
    //filter out frames < 5 byte length (likely ACKs)
    if (frame.length < 5) {
        p_outRFReportType = UNIFYING_RF_REPORT_INVALID;
        p_outHasKeepAliveSet = false;
        return;
    }

    *p_outRFReportType = frame.data[1]; // byte 1 is rf report type, byte 0 is device prefix
    *p_outHasKeepAliveSet = (frame.data[1] & 0x40) != 0;
    *p_outRFReportType &= UNIFYING_RF_REPORT_TYPE_MSK; //mask report type

    return;
}

#define UNIFYING_CLASSIFY_LOG_PREFIX "Unifying RF frame: "
void logitacker_unifying_frame_classify_log(nrf_esb_payload_t frame) {
    bool logKeepAliveEmpty = true;

    //filter out frames < 5 byte length (likely ACKs)
    if (frame.length < 5) {
        NRF_LOG_INFO("Invalid Unifying RF frame (wrong length or empty ack)");
        return;
    }

    uint8_t reportType = frame.data[1]; // byte 1 is rf report type, byte 0 is device prefix
    bool keepAliveSet = (reportType & 0x40) != 0;
    reportType &= UNIFYING_RF_REPORT_TYPE_MSK; //mask report type
    uint32_t counter;



    //filter out Unifying keep alive
    switch (reportType) {
        case UNIFYING_RF_REPORT_PLAIN_KEYBOARD:
            NRF_LOG_INFO("%sUnencrypted keyboard", UNIFYING_CLASSIFY_LOG_PREFIX);
            return;
        case UNIFYING_RF_REPORT_PLAIN_MOUSE:
            NRF_LOG_INFO("%sUnencrypted mouse", UNIFYING_CLASSIFY_LOG_PREFIX);
            return;
        case UNIFYING_RF_REPORT_PLAIN_MULTIMEDIA:
            NRF_LOG_INFO("%sUnencrypted multimedia key", UNIFYING_CLASSIFY_LOG_PREFIX);
            return;
        case UNIFYING_RF_REPORT_PLAIN_SYSTEM_CTL:
            NRF_LOG_INFO("%sUnencrypted system control key", UNIFYING_CLASSIFY_LOG_PREFIX);
            return;
        case UNIFYING_RF_REPORT_LED:
            NRF_LOG_INFO("%sLED (outbound)", UNIFYING_CLASSIFY_LOG_PREFIX);
            return;
        case UNIFYING_RF_REPORT_SET_KEEP_ALIVE:
            NRF_LOG_INFO("%sSet keep-alive", UNIFYING_CLASSIFY_LOG_PREFIX);
            return;
        case UNIFYING_RF_REPORT_HIDPP_SHORT:
            NRF_LOG_INFO("%sHID++ short", UNIFYING_CLASSIFY_LOG_PREFIX);
            return;
        case UNIFYING_RF_REPORT_HIDPP_LONG:
            NRF_LOG_INFO("%sHID++ long", UNIFYING_CLASSIFY_LOG_PREFIX);
            return;
        case UNIFYING_RF_REPORT_ENCRYPTED_KEYBOARD:
            //counter = frame.data[10] << 24 | frame.data[11] << 16 | frame.data[12] << 8 | frame.data[13];
            counter = 0;
            if (logitacker_unifying_extract_counter_from_encrypted_keyboard_frame(frame, &counter) == NRF_SUCCESS) {
                NRF_LOG_INFO("%sEncrypted keyboard, counter %08x", UNIFYING_CLASSIFY_LOG_PREFIX, counter);

            }

            return;
        case UNIFYING_RF_REPORT_PAIRING:
            NRF_LOG_INFO("%sPairing", UNIFYING_CLASSIFY_LOG_PREFIX);
            return;
        case 0x00:
            if (keepAliveSet && logKeepAliveEmpty) NRF_LOG_INFO("%sEmpty keep alive", UNIFYING_CLASSIFY_LOG_PREFIX);
            return;
        default:
            NRF_LOG_INFO("%sUnknown frame type %02x, keep alive %s", UNIFYING_CLASSIFY_LOG_PREFIX, frame.data[1], keepAliveSet ? "set" : "not set");
            return;
    }
}

