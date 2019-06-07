#ifndef LOGITACKER_DEVICES_H__
#define LOGITACKER_DEVICES_H__

#include <stdint.h>
#include "nrf_esb_illegalmod.h"
#include "logitacker_keyboard_map.h"

#define LOGITACKER_DEVICES_DONGLE_LIST_MAX_ENTRIES 30
#define LOGITACKER_DEVICES_DEVICE_LIST_MAX_ENTRIES 60
#define LOGITACKER_DEVICES_MAX_DEVICES_PER_DONGLE 7

#define LOGITACKER_DEVICE_ADDR_STR_LEN 16
#define LOGITACKER_DEVICE_ADDR_LEN 5


#define LOGITACKER_DEVICE_PROTOCOL_UNIFYING 0x04

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

typedef uint32_t logitacker_device_unifying_report_types_t;
#define LOGITACKER_DEVICE_REPORT_TYPES_KEYBOARD     (1 << 1)
#define LOGITACKER_DEVICE_REPORT_TYPES_MOUSE        (1 << 2)
#define LOGITACKER_DEVICE_REPORT_TYPES_MULTIMEDIA   (1 << 3)
#define LOGITACKER_DEVICE_REPORT_TYPES_POWER_KEYS   (1 << 4)
#define LOGITACKER_DEVICE_REPORT_TYPES_MEDIA_CENTER (1 << 8)
#define LOGITACKER_DEVICE_REPORT_TYPES_KEYBOARD_LED (1 << 14)
#define LOGITACKER_DEVICE_REPORT_TYPES_SHORT_HIDPP  (1 << 16)
#define LOGITACKER_DEVICE_REPORT_TYPES_LONG_HIDPP   (1 << 17)

typedef uint8_t logitacker_device_unifying_device_capbilities_t;
#define LOGITACKER_DEVICE_CAPS_LINK_ENCRYPTION       (1 << 0)
#define LOGITACKER_DEVICE_CAPS_UNIFYING_COMPATIBLE   (1 << 2)

typedef uint8_t logitacker_device_unifying_wpid_t[2];
typedef uint8_t logitacker_device_unifying_device_serial_t[4];
typedef uint8_t logitacker_devices_unifying_device_rf_address_t[5]; //byte order as seen on air (base address LSB first, prefix is last byte of address)
typedef uint8_t logitacker_devices_unifying_device_rf_addr_prefix_t; //prefix of rf_address
typedef uint8_t logitacker_devices_unifying_device_rf_addr_base_t[4]; //base rf_address (nRF5 SDK byte order, MSB first)
typedef uint8_t logitacker_device_unifying_device_key_t[16];

typedef uint16_t logitacker_device_unifying_device_fds_key_t;

typedef enum {
    DONGLE_CLASSIFICATION_UNKNOWN = 0, //f.e. keep alive without type
    DONGLE_CLASSIFICATION_IS_LOGITECH,
    DONGLE_CLASSIFICATION_IS_NOT_LOGITECH,
} logitacker_devices_dongle_classification_t;

typedef struct {
    uint32_t overal; //overall frame count
    uint32_t logitech_chksm;
} logitacker_device_frame_counter_t;

typedef struct logitacker_device_unifying_device logitacker_devices_unifying_device_t;
typedef struct logitacker_device_unifying_dongle logitacker_devices_unifying_dongle_t;


typedef struct logitacker_device_unifying_device {
    logitacker_device_unifying_device_fds_key_t storage_id;
    logitacker_devices_unifying_dongle_t * p_dongle;

    uint8_t index;  //refers to index used on USB end of dongle
    //uint8_t destination_id; //corresponds to prefix
    logitacker_devices_unifying_device_rf_addr_prefix_t addr_prefix;
    logitacker_device_unifying_wpid_t wpid;
    logitacker_device_unifying_type_t type;
    logitacker_device_unifying_device_serial_t serial;
    logitacker_devices_unifying_device_rf_address_t rf_address;
    logitacker_device_unifying_report_types_t report_types;
    logitacker_device_unifying_usability_info_t usability_info;

    logitacker_device_unifying_device_key_t raw_key_data;
    logitacker_device_unifying_device_key_t key;

    logitacker_device_unifying_device_capbilities_t caps;

    uint8_t device_name_len;    //4
    char device_name[17];       //5

    bool vuln_plain_injection;
    bool vuln_forced_pairing;

    bool key_known;

    //bool is_encrypted; //applies to RF key reports

    bool has_enough_whitened_reports;
    bool has_single_whitened_report;

    uint32_t last_used_aes_ctr;
    logitacker_device_frame_counter_t frame_counters;

    int executed_auto_inject_count; // has to be resetted on device load / store
} logitacker_devices_unifying_device_t;



typedef struct logitacker_device_unifying_dongle {
    logitacker_device_unifying_device_fds_key_t storage_id;

    logitacker_devices_unifying_device_rf_addr_base_t base_addr;

    logitacker_device_unifying_wpid_t wpid;

    bool is_texas_instruments;
    bool is_nordic;
    bool active_enumeration_finished;

    logitacker_devices_dongle_classification_t classification;

    uint8_t num_connected_devices;
    logitacker_devices_unifying_device_t * p_connected_devices[LOGITACKER_DEVICES_MAX_DEVICES_PER_DONGLE];
} logitacker_devices_unifying_dongle_t;


typedef struct {
    int current_pos;
} logitacker_devices_list_iterator_t;

void logitacker_devices_log_stats();

uint32_t logitacker_devices_create_dongle(logitacker_devices_unifying_dongle_t **pp_dongle, logitacker_devices_unifying_device_rf_addr_base_t const base_addr);
uint32_t logitacker_devices_restore_dongle_from_flash(logitacker_devices_unifying_dongle_t **pp_dongle, logitacker_devices_unifying_device_rf_addr_base_t const base_addr);
uint32_t logitacker_devices_store_dongle_to_flash(logitacker_devices_unifying_device_rf_addr_base_t const base_addr);
uint32_t logitacker_devices_get_dongle_by_base_addr(logitacker_devices_unifying_dongle_t **pp_dongle, logitacker_devices_unifying_device_rf_addr_base_t const base_addr);
uint32_t logitacker_devices_get_dongle_by_device_addr(logitacker_devices_unifying_dongle_t **pp_dongle, logitacker_devices_unifying_device_rf_address_t const rf_addr);
uint32_t logitacker_devices_del_dongle(logitacker_devices_unifying_device_rf_addr_base_t const base_addr);
uint32_t logitacker_devices_get_next_dongle(logitacker_devices_unifying_dongle_t ** pp_dongle, logitacker_devices_list_iterator_t * iter);

uint32_t logitacker_devices_add_given_device_to_dongle(logitacker_devices_unifying_dongle_t * p_dongle, logitacker_devices_unifying_device_t * p_device);
uint32_t logitacker_devices_remove_device_from_dongle(logitacker_devices_unifying_device_t * p_device);


uint32_t logitacker_devices_create_device(logitacker_devices_unifying_device_t ** pp_device, logitacker_devices_unifying_device_rf_address_t const rf_addr);
uint32_t logitacker_devices_restore_device_from_flash(logitacker_devices_unifying_device_t **pp_device, logitacker_devices_unifying_device_rf_address_t const rf_address);
uint32_t logitacker_devices_store_ram_device_to_flash(logitacker_devices_unifying_device_rf_address_t const rf_addr);
uint32_t logitacker_devices_remove_device_from_flash(logitacker_devices_unifying_device_rf_address_t const rf_addr);
uint32_t logitacker_devices_get_device(logitacker_devices_unifying_device_t ** pp_device, logitacker_devices_unifying_device_rf_address_t const rf_addr);
uint32_t logitacker_devices_del_device(logitacker_devices_unifying_device_rf_address_t const rf_addr);
uint32_t logitacker_devices_del_all();
uint32_t logitacker_devices_get_next_device(logitacker_devices_unifying_device_t ** pp_device, logitacker_devices_list_iterator_t * iter);

uint32_t logitacker_devices_device_update_classification(logitacker_devices_unifying_device_t * p_device, nrf_esb_payload_t frame);

uint32_t logitacker_devices_generate_keyboard_frame(logitacker_devices_unifying_device_t *p_device,
                                                    nrf_esb_payload_t *p_result_payload,
                                                    hid_keyboard_report_t const *const p_in_hid_report);


#endif