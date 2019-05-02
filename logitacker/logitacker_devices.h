#ifndef LOGITACKER_DEVICES_H__
#define LOGITACKER_DEVICES_H__

#include <stdint.h>
#include "nrf_esb_illegalmod.h"

#define LOGITACKER_DEVICES_MAX_LIST_ENTRIES 30
#define LOGITACKER_DEVICE_ADDR_STR_LEN 16
#define LOGITACKER_DEVICE_ADDR_LEN 5

typedef enum {
    LOGITACKER_DEVICE_UNIFYING_TYPE_UNKNOWN = 0x00,
    LOGITACKER_DEVICE_UNIFYING_TYPE_KEYBOARD = 0x01,
    LOGITACKER_DEVICE_UNIFYING_TYPE_MOUSE = 0x02,
    LOGITACKER_DEVICE_UNIFYING_TYPE_NUMPAD = 0x03,
    LOGITACKER_DEVICE_UNIFYING_TYPE_PRESENTER = 0x04,
    LOGITACKER_DEVICE_UNIFYING_TYPE_TRACKBALL = 0x08,
    LOGITACKER_DEVICE_UNIFYING_TYPE_TOUCHPAD = 0x09,

} logitacker_device_unifying_type_t;

typedef enum {
    LOGITACKER_DEVICE_USABILITY_INFO_RESERVED                                     = 0x0,
    LOGITACKER_DEVICE_USABILITY_INFO_PS_LOCATION_ON_THE_BASE                      = 0x1,
    LOGITACKER_DEVICE_USABILITY_INFO_PS_LOCATION_ON_THE_TOP_CASE                  = 0x2,
    LOGITACKER_DEVICE_USABILITY_INFO_PS_LOCATION_ON_THE_EDGE_OF_TOP_RIGHT_CORNER  = 0x3,
    LOGITACKER_DEVICE_USABILITY_INFO_PS_LOCATION_OTHER                            = 0x4,
    LOGITACKER_DEVICE_USABILITY_INFO_PS_LOCATION_ON_THE_TOP_LEFT_CORNER           = 0x5,
    LOGITACKER_DEVICE_USABILITY_INFO_PS_LOCATION_ON_THE_BOTTOM_LEFT_CORNER        = 0x6,
    LOGITACKER_DEVICE_USABILITY_INFO_PS_LOCATION_ON_THE_TOP_RIGHT_CORNER          = 0x7,
    LOGITACKER_DEVICE_USABILITY_INFO_PS_LOCATION_ON_THE_BOTTOM_RIGHT_CORNER       = 0x8,
    LOGITACKER_DEVICE_USABILITY_INFO_PS_LOCATION_ON_THE_TOP_EDGE                  = 0x9,
    LOGITACKER_DEVICE_USABILITY_INFO_PS_LOCATION_ON_THE_RIGHT_EDGE                = 0xa,
    LOGITACKER_DEVICE_USABILITY_INFO_PS_LOCATION_ON_THE_LEFT_EDGE                 = 0xb,
    LOGITACKER_DEVICE_USABILITY_INFO_PS_LOCATION_ON_THE_BOTTOM_EDGE               = 0xc,
} logitacker_device_unifying_usability_info_t;

#define LOGITACKER_DEVICE_REPORT_TYPES_KEYBOARD     (1 << 1)
#define LOGITACKER_DEVICE_REPORT_TYPES_MOUSE        (1 << 2)
#define LOGITACKER_DEVICE_REPORT_TYPES_MULTIMEDIA   (1 << 3)
#define LOGITACKER_DEVICE_REPORT_TYPES_POWER_KEYS   (1 << 4)
#define LOGITACKER_DEVICE_REPORT_TYPES_MEDIA_CENTER (1 << 8)
#define LOGITACKER_DEVICE_REPORT_TYPES_KEYBOARD_LED (1 << 14)
#define LOGITACKER_DEVICE_REPORT_TYPES_SHORT_HIDPP  (1 << 16)
#define LOGITACKER_DEVICE_REPORT_TYPES_LONG_HIDPP   (1 << 17)

#define LOGITACKER_DEVICE_CAPS_LINK_ENCRYPTION       (1 << 0)
#define LOGITACKER_DEVICE_CAPS_UNIFYING_COMPATIBLE   (1 << 2)

typedef enum {
    LOGITACKER_COUNTER_TYPE_UNIFYING_INVALID = 0, //f.e. keep alive without type
    LOGITACKER_COUNTER_TYPE_UNIFYING_PLAIN_KEYBOARD,
    LOGITACKER_COUNTER_TYPE_UNIFYING_PLAIN_MOUSE,
    LOGITACKER_COUNTER_TYPE_UNIFYING_PLAIN_MULTIMEDIA,
    LOGITACKER_COUNTER_TYPE_UNIFYING_PLAIN_SYSTEM_CTL,
    LOGITACKER_COUNTER_TYPE_UNIFYING_LED,
    LOGITACKER_COUNTER_TYPE_UNIFYING_SET_KEEP_ALIVE,
    LOGITACKER_COUNTER_TYPE_UNIFYING_HIDPP_SHORT,
    LOGITACKER_COUNTER_TYPE_UNIFYING_HIDPP_LONG,
    LOGITACKER_COUNTER_TYPE_UNIFYING_ENCRYPTED_KEYBOARD,
    LOGITACKER_COUNTER_TYPE_UNIFYING_PAIRING,
    LOGITACKER_COUNTER_TYPE_LENGTH,
} logitacker_device_frame_counter_type_t;

typedef struct {
    uint8_t typed[LOGITACKER_COUNTER_TYPE_LENGTH]; //frame count of each known type
    uint32_t overal; //overall frame count
    uint32_t logitech_chksm;
} logitacker_device_frame_counter_t;

typedef struct {
    uint8_t index;  //refers to index used on USB end of dongle
    //uint8_t destination_id; //corresponds to prefix
    uint8_t wpid[2];
    logitacker_device_unifying_type_t type;
    uint8_t serial[4];
    uint8_t rf_address[5];
    uint32_t report_types;
    logitacker_device_unifying_usability_info_t usability_info;

    uint8_t raw_key_data[16];
    uint8_t key[16];

    uint8_t caps;

    uint8_t device_name_len;    //4
    char device_name[17];       //5

    bool vuln_plain_injection;
    bool vuln_forced_pairing;

    bool key_known;

    bool is_encrypted; //applies to RF key reports
} logitacker_device_capabilities_t;


#define LOGITACKER_DEVICE_MAX_PREFIX 7

typedef struct {
    uint8_t base_addr[4];

    uint8_t device_prefixes[LOGITACKER_DEVICE_MAX_PREFIX];
    uint8_t num_device_prefixes;

    uint8_t wpid[2];

    bool is_texas_instruments;
    bool is_nordic;
    bool is_logitech;

    logitacker_device_frame_counter_t frame_counters[LOGITACKER_DEVICE_MAX_PREFIX];
    logitacker_device_capabilities_t capabilities[LOGITACKER_DEVICE_MAX_PREFIX];
} logitacker_device_set_t;


void logitacker_device_update_counters_from_frame(uint8_t const *const rf_addr, nrf_esb_payload_t frame);

logitacker_device_set_t *logitacker_device_set_add_new_by_dev_addr(uint8_t const *const rf_addr);

logitacker_device_set_t *logitacker_device_set_list_get_by_addr(uint8_t const *const addr);

logitacker_device_set_t *logitacker_device_set_list_get(uint32_t pos); //returns NULL if position is unused or invalid

uint32_t logitacker_device_list_remove_by_addr(uint8_t const *const rf_addr);

uint32_t logitacker_device_list_remove_by_base(uint8_t const *const base_addr);

void logitacker_device_list_flush();

uint32_t logitacker_device_add_prefix(logitacker_device_set_t *out_device, uint8_t prefix);

uint32_t
logitacker_device_get_prefix_index(int *out_index, logitacker_device_set_t const *const in_device, uint8_t prefix);

logitacker_device_capabilities_t *logitacker_device_get_caps_pointer(uint8_t const *const rf_addr);


#endif