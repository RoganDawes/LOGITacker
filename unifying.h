#ifndef UNIFYING_H__
#define UNIFYING_H__

#include <stdint.h>
#include "nrf_esb_illegalmod.h"



#define UNIFYING_SLEEP_MS_BETWEEN_TX                        4  //should be 8ms, but as keep-alives are inserted, could be halfed (see next param)
#define UNIFYING_REPLAY_KEEP_ALIVES_TO_INSERT_BETWEEN_TX    3

#define XOR_BRUTEFORCE_FIRST_HID_CODE       0x04
#define XOR_BRUTEFORCE_LAST_HID_CODE        0x65
//#define XOR_BRUTEFORCE_LAST_HID_CODE        0x52 //exclude keypad keys, "key Non-US" and "key application"
//#define XOR_BRUTEFORCE_LAST_HID_CODE        0x39 //exclude keypad keys, "key Non-US" and "key application"

#define REPLAYS_BEFORE_BRUTEFORCE_ITERATION 1 //how often keys are replayed to check for LED reports, before next bruteforce iteration

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

#define UNIFYING_RF_REPORT_BIT_KEEP_ALIVE       0x40
#define UNIFYING_RF_REPORT_BIT_UNKNOWN          0x80

// note: 26 frames are minimum to overwrite counter re-use protection, but more frames
//       are used to overcome changing counters by real keypresses in between replayed RF frames
#define UNIFYING_MAX_STORED_REPORTS_PER_PIPE 60                                 //space for record storage per pipe
#define UNIFYING_MIN_STORED_REPORTS_VALID_PER_PIPE 52                           //number of encrypted keyboard reports to collect (shouldn't be less than 26, not more than UNIFYING_MIN_STORED_REPORTS_VALID_PER_PIPE)
#define UNIFYING_MIN_STORED_REPORTS_WITH_SUCCESSIVE_LED_TOGGLE_KEY_DOWNS 13     //required number of key down reports which toggle LED during bruteforce (not less than 13, not more than UNIFYING_MIN_STORED_REPORTS_VALID_PER_PIPE/2)


static const uint8_t UNIFYING_GLOBAL_PAIRING_ADDRESS[] = {0xbb, 0x0a, 0xdc, 0xa5, 0x75 };

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



typedef struct {
    uint32_t pre_delay_ms; //delay in millisecond to sleep before TX of this frame during replay
    uint8_t reportType;
    uint8_t length;
    uint8_t data[32];
    bool isEncrytedKeyRelease; //true if encrypted keyboard report is assumed to be a key release frame
    uint32_t counter; //counter, in case this is a encrypted keyboard report
    bool resulted_in_LED_report;
    uint8_t resulting_LED_state;
} unifying_rf_record_t;

typedef struct {
    unifying_rf_record_t* records;
    uint8_t write_pos;
    uint8_t first_pos;
    uint8_t last_pos;

    uint8_t lastRecordedReportType;
    bool disallowWrite;

    uint8_t XOR_key_for_LED_brute_force;
    bool all_encrypted_reports_produce_LED_reports;
} unifying_rf_record_set_t;



typedef struct {
    rf_report_22_t  report[NUM_WHITENED_REPORTS];
} whitened_replay_frames_t;

typedef enum {
    UNIFYING_EVENT_STORED_SUFFICIENT_ENCRYPTED_KEY_FRAMES,   // event fires if an alternating sequence of encrypted key down / key release frames with successive counters has been recorded with length UNIFYING_MIN_STORED_REPORTS_VALID_PER_PIPE
    UNIFYING_EVENT_REPLAY_RECORDS_STARTED,    
    UNIFYING_EVENT_REPLAY_RECORDS_FINISHED,
    UNIFYING_EVENT_REPLAY_RECORDS_FAILED
} unifying_evt_id_t;

typedef struct {
    unifying_evt_id_t   evt_id;
    uint8_t             pipe;
} unifying_evt_t;



typedef void (* unifying_event_handler_t)(unifying_evt_t const * p_event);

typedef void (* unifying_replay_ack_payload_handler_t) (unifying_rf_record_set_t *p_rs, nrf_esb_payload_t const *p_ack_payload);

void unifying_init(unifying_event_handler_t event_handler);
uint32_t restoreDeviceInfoFromFlash(uint16_t deviceRecordIndex, device_info_t *deviceInfo);
uint32_t restoreDeviceWhitenedReportsFromFlash(uint16_t deviceRecordIndex, whitened_replay_frames_t *reports);
uint32_t updateDeviceInfoOnFlash(uint16_t deviceRecordIndex, device_info_t *deviceInfo);
uint32_t updateDeviceWhitenedReportsOnFlash(uint16_t deviceRecordIndex, whitened_replay_frames_t *reports);
//bool unifying_validate_payload(uint8_t * p_array, uint8_t paylen); 
bool unifying_payload_update_checksum(uint8_t * p_array, uint8_t paylen);
bool unifying_payload_validate_checksum(uint8_t * p_array, uint8_t paylen);
void unifying_frame_classify(nrf_esb_payload_t frame, uint8_t *p_outRFReportType, bool *p_outHasKeepAliveSet);
void unifying_frame_classify_log(nrf_esb_payload_t frame);
/*
unifying_replay_records

replay_realtime:        if enabled frames are played back with recording speed and gaps filled with keep alives (every 8ms)
                        if disabled frames are played back with 8ms interval
keep_alives_to_insert   if replay_realtime is disabled, this number of 8ms keep-alives is inserted after each frame (helps
                        to correlate received ack payloads to replayed frames)
*/
void unifying_replay_records(uint8_t pipe_num, bool replay_realtime, uint8_t keep_alives_to_insert);
bool unifying_record_rf_frame(nrf_esb_payload_t frame);
void unifying_replay_records_LED_bruteforce_iteration(uint8_t pipe_num);
bool unifying_replay_records_LED_bruteforce_done(uint8_t pipe_num);

// returns true if the given esb event was consumed by unifying module
bool unifying_process_esb_event(nrf_esb_evt_t *p_event);


#endif
