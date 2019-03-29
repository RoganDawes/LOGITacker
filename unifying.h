#ifndef UNIFYING_H__
#define UNIFYING_H__

#include <stdint.h>
#include "nrf_esb_illegalmod.h"

#define UNIFYING_RF_REPORT_TYPE_MSK             0x1f

#define UNIFYING_RF_REPORT_INVALID              0x00
#define UNIFYING_RF_REPORT_PLAIN_KEYBOARD       0x01
#define UNIFYING_RF_REPORT_PLAIN_MOUSE          0x02
#define UNIFYING_RF_REPORT_PLAIN_MULTIMEDIA     0x03
#define UNIFYING_RF_REPORT_PLAIN_SYSTEM_CTL     0x04
#define UNIFYING_RF_REPORT_LED                  0x0e
#define UNIFYING_RF_REPORT_SET_KEEP_ALIVE       0x0f
#define UNIFYING_RF_REPORT_HIDPP_SHORT          0x10
#define UNIFYING_RF_REPORT_HIDPP_LONG           0x11
#define UNIFYING_RF_REPORT_ENCRYPTED_KEYBOARD   0x13
#define UNIFYING_RF_REPORT_PAIRING              0x1f


typedef struct
{
    uint8_t     RfAddress[5];
    uint8_t     WPID[2];
    uint8_t     DongleWPID[2];
    uint8_t     Nonce[4];
    uint8_t     DongleNonce[4];

    uint8_t     DeviceCapabilities;
    uint8_t     DeviceType;

    char        DeviceName[16];
    uint8_t     DeviceNameLen;

    //uint8_t     KEY[16];
} device_info_t;

typedef struct
{
    uint8_t     data[22];
} rf_report_22_t;

#define NUM_WHITENED_REPORTS 26

typedef struct
{
    rf_report_22_t  report[NUM_WHITENED_REPORTS];
} whitened_replay_frames_t;

void unifying_init();
uint32_t restoreDeviceInfoFromFlash(uint16_t deviceRecordIndex, device_info_t *deviceInfo);
uint32_t restoreDeviceWhitenedReportsFromFlash(uint16_t deviceRecordIndex, whitened_replay_frames_t *reports);
uint32_t updateDeviceInfoOnFlash(uint16_t deviceRecordIndex, device_info_t *deviceInfo);
uint32_t updateDeviceWhitenedReportsOnFlash(uint16_t deviceRecordIndex, whitened_replay_frames_t *reports);
bool unifying_validate_payload(uint8_t * p_array, uint8_t paylen);
bool unifying_payload_update_checksum(uint8_t * p_array, uint8_t paylen);
void unifying_frame_classify(nrf_esb_payload_t frame, uint8_t *p_outRFReportType, bool *p_outHasKeepAliveSet);
void unifying_frame_classify_log(nrf_esb_payload_t frame);
void unifying_transmit_records(uint8_t pipe_num);
bool unifying_record_rf_frame(nrf_esb_payload_t frame);

// note: 26 frames are minimum to overwrite counter re-use protection, but more frames
//       are used to overcome changing counters by real keypresses in between replayed RF frames
#define UNIFYING_MAX_STORED_REPORTS_PER_PIPE 40

typedef struct {
    uint32_t pre_delay_ms; //delay in millisecond to sleep before TX of this frame during replay
    uint8_t reportType;
    uint8_t length;
    //uint16_t dummy; //assure 32bit alignment to avoid hard fault when used with app_timer
    uint8_t data[32];
    bool isEncrytedKeyRelease; //true if encrypted keyboard report is assumed to be a key release frame
    uint32_t counter; //counter, in case this is a encrypted keyboard report
} unifying_rf_record_t;

typedef struct {
    unifying_rf_record_t* records;
    uint8_t write_pos;
    uint8_t read_pos;
    uint8_t first_pos;
    uint8_t last_pos;

    uint8_t pipe_num;

    uint8_t lastRecordedReportType;
    bool disallowWrite;
    //uint8_t dummy[3]; //assure 32bit alignment to avoid hard fault when used with app_timer
} unifying_rf_record_set_t;




#endif
