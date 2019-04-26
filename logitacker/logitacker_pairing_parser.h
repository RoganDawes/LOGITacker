#ifndef LOGITACKER_PAIRING_PARSER_H
#define LOGITACKER_PAIRING_PARSER_H

#include "nrf_esb_illegalmod.h"


#define LOGITACKER_UNIFYING_PAIRING_REQ1_OFFSET_DEVICE_WPID 9
#define LOGITACKER_UNIFYING_PAIRING_REQ1_OFFSET_DEVICE_TYPE 13
#define LOGITACKER_UNIFYING_PAIRING_REQ1_OFFSET_DEVICE_CAPS 14

#define LOGITACKER_UNIFYING_PAIRING_RSP1_OFFSET_DONGLE_WPID 9
#define LOGITACKER_UNIFYING_PAIRING_RSP1_OFFSET_BASE_ADDR 3
#define LOGITACKER_UNIFYING_PAIRING_RSP1_OFFSET_ADDR_PREFIX 7

#define LOGITACKER_UNIFYING_PAIRING_REQ2_OFFSET_DEVICE_NONCE 3
#define LOGITACKER_UNIFYING_PAIRING_REQ2_OFFSET_DEVICE_SERIAL 7
#define LOGITACKER_UNIFYING_PAIRING_REQ2_OFFSET_DEVICE_REPORT_TYPES_LE 11 //little endian 32bit uint
#define LOGITACKER_UNIFYING_PAIRING_REQ2_OFFSET_DEVICE_USABILITY_INFO 15

#define LOGITACKER_UNIFYING_PAIRING_RSP2_OFFSET_DONGLE_NONCE 3

#define LOGITACKER_UNIFYING_PAIRING_REQ3_OFFSET_DEVICE_NAME_LEN 4
#define LOGITACKER_UNIFYING_PAIRING_REQ3_OFFSET_DEVICE_NAME 5

typedef struct {
    // req 1
    uint8_t device_wpid[2];     //9,10
    uint8_t device_type;        //13
    uint8_t device_caps;        //14
    // rsp 1
    uint8_t dongle_wpid[2];     //9,10
    uint8_t base_addr[4];       //3..6
    uint8_t device_prefix;      //7
    // req 2
    uint8_t device_nonce[4];    //3..6
    uint8_t device_serial[4];   //7..10
    uint32_t device_report_types; //11..14
    uint8_t device_usability_info; // 15
    // rsp 2
    uint8_t dongle_nonce[4];    //3..6
    // req3
    uint8_t device_name_len;    //4
    char device_name[17];       //5

    uint8_t device_rf_address[5];

    uint8_t device_raw_key_material[16];
    uint8_t device_key[16];

    bool has_req1;
    bool has_rsp1;
    bool has_req2;
    bool has_rsp2;
    bool has_req3;
    bool has_rsp_final;
    bool key_material_complete;
    bool full_pairing;
} logitacker_pairing_info_t;

uint32_t logitacker_pairing_parser(logitacker_pairing_info_t * pi, nrf_esb_payload_t const * const rx_payload);
uint32_t logitacker_pairing_parser_print(logitacker_pairing_info_t * pi);

#endif //LOGITACKER_PAIRING_PARSER_H
