#ifndef LOGITACKER_FLASH_H
#define LOGITACKER_FLASH_H

#include <stdint.h>
#include "logitacker_devices.h"

#define LOGITACKER_FLASH_FILE_ID_GLOBAL_OPTIONS 0x1000
#define LOGITACKER_FLASH_KEY_GLOBAL_OPTIONS_LOGITACKER 0x1000

#define LOGITACKER_FLASH_FILE_ID_DEVICE_SETS 0x1001
#define LOGITACKER_FLASH_KEY_DEVICE_SET_LIST 0x1000
#define LOGITACKER_FLASH_MAX_ENTRIES_DEVICE_SET_LIST 64

typedef struct {
    uint16_t * record_key;
} logitacker_flash_device_list_entry_t;

typedef struct {
    uint16_t num_entries;
    logitacker_flash_device_list_entry_t first_key;
    logitacker_flash_device_list_entry_t next_free_key;
} logitacker_flash_device_list_t;

uint32_t logitacker_load_device_set_from_flash_by_addr(logitacker_device_unifying_dongle_t * p_device_set, uint8_t rf_address[5]);
uint32_t logitacker_load_device_set_from_flash_by_key(logitacker_device_unifying_dongle_t * p_device_set, uint16_t key);
uint32_t logitacker_remove_device_set_from_flash_by_addr(uint8_t rf_address[5]);
uint32_t logitacker_remove_device_set_from_flash_by_key(uint16_t key);
uint32_t logitacker_store_device_set_to_flash(logitacker_device_unifying_dongle_t * p_device_set, uint16_t * out_key);

uint32_t logitacker_flash_init();

#endif //LOGITACKER_FLASH_H
