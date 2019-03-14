#ifndef FLASH_DEVICE_INFO_H__
#define FLASH_DEVICE_INFO_H__

#include <stdint.h>

/* File ID and Key used for the configuration record. */

#define FLASH_FILE_STATE     (0x0001)
#define FLASH_RECORD_STATE_STATE     (0x0001)

#define FLASH_FILE_DEVICES     (0x0002)
#define FLASH_RECORD_PREFIX_DEVICES_DEVICE_INFO  (0x1000)
#define FLASH_RECORD_PREFIX_DEVICES_DEVICE_WHITENED_KEY_REPORTS  (0x2000)


#define FLASH_RECORD_DEVICES_DEVICE_INFO  (0x0001)
#define FLASH_RECORD_DEVICES_DEVICE_WHITENED_KEY_REPORTS  (0x0002)



/*
// Defined in main.c

void delete_all_begin(void);

// Defined in cli.c 

void cli_init(void);
void cli_start(void);
void cli_process(void);
bool record_delete_next(void);
*/

#endif
