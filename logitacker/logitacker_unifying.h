#ifndef LOGITACKER_UNIFYING_H
#define LOGITACKER_UNIFYING_H

#include "stdint.h"
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

#define UNIFYING_RF_REPORT_BIT_KEEP_ALIVE       0x40
#define UNIFYING_RF_REPORT_BIT_UNKNOWN          0x80


static const uint8_t UNIFYING_GLOBAL_PAIRING_ADDRESS[] = {0xbb, 0x0a, 0xdc, 0xa5, 0x75 };

bool logitacker_unifying_payload_update_checksum(uint8_t *p_array, uint8_t paylen);
bool logiteacker_unifying_payload_validate_checksum(uint8_t *p_array, uint8_t paylen);
void logitacker_unifying_frame_classify(nrf_esb_payload_t frame, uint8_t *p_outRFReportType, bool *p_outHasKeepAliveSet);
void logitacker_unifying_frame_classify_log(nrf_esb_payload_t frame);
uint32_t logitacker_unifying_extract_counter_from_encrypted_keyboard_frame(nrf_esb_payload_t frame, uint32_t *p_counter);

#endif //LOGITACKER_UNIFYING_H
