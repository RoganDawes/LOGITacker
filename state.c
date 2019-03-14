#include <string.h> // for memcpy
#include "state.h"
#include "flash_device_info.h"
#include "fds.h"

void restoreStateFromFlash(dongle_state_t *state) {
    ret_code_t ret;

    // flash record for dongle state
    fds_record_t state_flash_record = {
        .file_id           = FLASH_FILE_STATE,
        .key               = FLASH_RECORD_STATE_STATE,
        .data.p_data       = state,
        // The length of a record is always expressed in 4-byte units (words).
        .data.length_words = (sizeof(*state) + 3) / sizeof(uint32_t),
    };

    //Try to load first state record from flash, create if not existing
    fds_record_desc_t desc = {0}; //used as ref to current record
    fds_find_token_t  tok  = {0}; //used for find functions if records with same ID exist and we want to find next one

    ret = fds_record_find(FLASH_FILE_STATE, FLASH_RECORD_STATE_STATE, &desc, &tok);
    if (ret == FDS_SUCCESS) { // flash record with device info exists
        /* A config file is in flash. Let's update it. */
        fds_flash_record_t record_state = {0};

        // Open the record and read its contents. */
        ret = fds_record_open(&desc, &record_state);
        APP_ERROR_CHECK(ret);

        // restore state from flash into state (copy)
        memcpy(state, record_state.p_data, sizeof(dongle_state_t));

        //modify state
        state->boot_count++;

        // update respective flash record
        ret = fds_record_close(&desc);
        APP_ERROR_CHECK(ret);


        // Write the updated record to flash.
        ret = fds_record_update(&desc, &state_flash_record);
        APP_ERROR_CHECK(ret);
    } else {
        // no state found, create new flash record
        ret = fds_record_write(&desc, &state_flash_record);
        APP_ERROR_CHECK(ret);
    }

    return;
}