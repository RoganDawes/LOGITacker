#ifndef UNIFYING_H__
#define UNIFYING_H__

#include <stdint.h>
#include "nrf_esb_illegalmod.h"

#define UNIFYING_RF_REPORT_TYPE_MSK             0x1f

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

uint32_t restoreDeviceInfoFromFlash(uint16_t deviceRecordIndex, device_info_t *deviceInfo);
uint32_t restoreDeviceWhitenedReportsFromFlash(uint16_t deviceRecordIndex, whitened_replay_frames_t *reports);
uint32_t updateDeviceInfoOnFlash(uint16_t deviceRecordIndex, device_info_t *deviceInfo);
uint32_t updateDeviceWhitenedReportsOnFlash(uint16_t deviceRecordIndex, whitened_replay_frames_t *reports);
bool unifying_validate_payload(uint8_t * p_array, uint8_t paylen);
bool unifying_payload_update_checksum(uint8_t * p_array, uint8_t paylen);
void unifying_frame_classify(nrf_esb_payload_t frame);
void unifying_replay_records(uint8_t pipe_num);

typedef struct {
    uint32_t pre_delay_ms; //delay in millisecond to sleep before TX of this frame during replay
    uint8_t reportType;
    uint8_t length;
    uint8_t data[32];
} unifying_rf_record_t;

#define UNIFYING_MAX_STORED_REPORTS_PER_PIPE 32


#endif
