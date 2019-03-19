#ifndef UNIFYING_H__
#define UNIFYING_H__

#include <stdint.h>

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

#endif
