#include "logitacker_devices.h"
#include "unifying.h"
#include "helper.h"

#define NRF_LOG_MODULE_NAME LOGITACKER_DEVICES
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

uint32_t logitacker_devices_del_device_(logitacker_devices_unifying_device_rf_address_t const rf_addr);

static logitacker_devices_unifying_dongle_t m_loc_dongle_list[LOGITACKER_DEVICES_DONGLE_LIST_MAX_ENTRIES];
static logitacker_devices_unifying_device_t m_loc_device_list[LOGITACKER_DEVICES_DEVICE_LIST_MAX_ENTRIES];
static bool m_loc_dongle_list_entry_used[LOGITACKER_DEVICES_DONGLE_LIST_MAX_ENTRIES];
static bool m_loc_device_list_entry_used[LOGITACKER_DEVICES_DEVICE_LIST_MAX_ENTRIES];


// returns -1 if no free entry available
int get_next_free_dongle_list_entry_pos() {
    int result = -1;
    for (int i = 0; i < LOGITACKER_DEVICES_DONGLE_LIST_MAX_ENTRIES; i++) {
        if (!m_loc_dongle_list_entry_used[i]) {
            result = i;
            break;
        }
    }
    return result;
}

// returns -1 if no free entry available
int get_next_free_device_list_entry_pos() {
    int result = -1;
    for (int i = 0; i < LOGITACKER_DEVICES_DEVICE_LIST_MAX_ENTRIES; i++) {
        if (!m_loc_device_list_entry_used[i]) {
            result = i;
            break;
        }
    }
    return result;
}

uint32_t logitacker_devices_create_dongle(logitacker_devices_unifying_dongle_t **pp_dongle,
                                          logitacker_devices_unifying_device_rf_addr_base_t const base_addr) {
    VERIFY_TRUE(base_addr != NULL, NRF_ERROR_NULL);
    VERIFY_TRUE(pp_dongle != NULL, NRF_ERROR_NULL);

    // get next free entry
    int next_free_pos = get_next_free_dongle_list_entry_pos();
    if (next_free_pos == -1) return NRF_ERROR_NO_MEM;

    logitacker_devices_unifying_dongle_t *p_dongle = &m_loc_dongle_list[next_free_pos]; //retrieve entry
    m_loc_dongle_list_entry_used[next_free_pos] = true; //mark entry as used

    // clear entry
    memset(p_dongle, 0, sizeof(logitacker_devices_unifying_dongle_t));
    // set base addr
    memcpy(p_dongle->base_addr, base_addr, sizeof(logitacker_devices_unifying_device_rf_addr_base_t));
    p_dongle->classification = DONGLE_CLASSIFICATION_UNKNOWN;

    //set return value
    *pp_dongle = p_dongle;
    return NRF_SUCCESS;
}


uint32_t logitacker_devices_get_dongle_by_base_addr(logitacker_devices_unifying_dongle_t **pp_dongle, logitacker_devices_unifying_device_rf_addr_base_t const base_addr) {
    VERIFY_TRUE(base_addr != NULL, NRF_ERROR_NULL);
    VERIFY_TRUE(pp_dongle != NULL, NRF_ERROR_NULL);


    logitacker_devices_unifying_dongle_t * p_dongle = NULL;
    for (int i=0; i < LOGITACKER_DEVICES_DONGLE_LIST_MAX_ENTRIES; i++) {
        if (m_loc_dongle_list_entry_used[i]) { // non empty entry
            if (memcmp(m_loc_dongle_list[i].base_addr, base_addr, sizeof(logitacker_devices_unifying_device_rf_addr_base_t)) != 0) continue; //base addr doesn't match
            // if here, we have a match
            p_dongle = &m_loc_dongle_list[i];
            break;
        }
    }

    *pp_dongle = p_dongle;
    if (p_dongle == NULL) return NRF_ERROR_NOT_FOUND;

    return NRF_SUCCESS;
}

uint32_t logitacker_devices_get_dongle_by_device_addr(logitacker_devices_unifying_dongle_t **pp_dongle, logitacker_devices_unifying_device_rf_address_t const rf_addr) {
    VERIFY_TRUE(rf_addr != NULL, NRF_ERROR_NULL);
    VERIFY_TRUE(pp_dongle != NULL, NRF_ERROR_NULL);

    logitacker_devices_unifying_device_rf_addr_base_t base_addr = {0};
    for (int i=3; i >= 0; i--) {
        base_addr[3-i] = rf_addr[i];
    }

    return logitacker_devices_get_dongle_by_base_addr(pp_dongle, base_addr);
}

uint32_t logitacker_devices_del_dongle(logitacker_devices_unifying_device_rf_addr_base_t const base_addr) {
    VERIFY_TRUE(base_addr != NULL, NRF_ERROR_NULL);

    logitacker_devices_unifying_dongle_t * p_dongle = NULL;
    int dongle_idx = -1;
    for (int i=0; i < LOGITACKER_DEVICES_DONGLE_LIST_MAX_ENTRIES; i++) {
        if (m_loc_dongle_list_entry_used[i]) { // non empty entry
            if (memcmp(m_loc_dongle_list[i].base_addr, base_addr, sizeof(logitacker_devices_unifying_device_rf_addr_base_t)) != 0) continue; //base addr doesn't match
            // if here, we have a match
            p_dongle = &m_loc_dongle_list[i];
            dongle_idx = i;
            break;
        }
    }

    if (dongle_idx == -1) return NRF_ERROR_NOT_FOUND;

    // delete associated devices
    for (int dev_idx = 0; dev_idx < p_dongle->num_connected_devices; dev_idx++) {
        logitacker_devices_unifying_device_t * p_device = p_dongle->p_connected_devices[dev_idx];
        if (p_device != NULL) {
            logitacker_devices_del_device_(p_device->rf_address);
        }
    }


    //mark unused
    m_loc_dongle_list_entry_used[dongle_idx] = false;
    //clear donogle data
    memset(p_dongle, 0, sizeof(logitacker_devices_unifying_dongle_t));

    return NRF_SUCCESS;
}

uint32_t logitacker_devices_get_next_dongle(logitacker_devices_unifying_dongle_t ** pp_dongle, logitacker_devices_list_iterator_t * iter) {
    VERIFY_TRUE(pp_dongle != NULL, NRF_ERROR_NULL);
    VERIFY_TRUE(iter != NULL, NRF_ERROR_NULL);

    int dongle_idx = -1;
    for (int i=iter->current_pos; i < LOGITACKER_DEVICES_DONGLE_LIST_MAX_ENTRIES; i++) {
        if (m_loc_dongle_list_entry_used[i]) { // non empty entry
            // if here, we have a match
            dongle_idx = i;
            break;
        }
    }


    if (dongle_idx == -1) {
        iter->current_pos = LOGITACKER_DEVICES_DONGLE_LIST_MAX_ENTRIES; // all elements have been tested, advance iterator
        *pp_dongle = NULL;
        return NRF_ERROR_NOT_FOUND;
    }

    *pp_dongle = &m_loc_dongle_list[dongle_idx];
    iter->current_pos = dongle_idx+1;
    return NRF_SUCCESS;
}

uint32_t logitacker_devices_create_device_(logitacker_devices_unifying_device_t ** pp_device, logitacker_devices_unifying_device_rf_address_t const rf_addr) {
    VERIFY_TRUE(rf_addr != NULL, NRF_ERROR_NULL);
    VERIFY_TRUE(pp_device != NULL, NRF_ERROR_NULL);

    // get next free entry
    int next_free_pos = get_next_free_device_list_entry_pos();
    if (next_free_pos == -1) return NRF_ERROR_NO_MEM;

    logitacker_devices_unifying_device_t *p_device = &m_loc_device_list[next_free_pos]; //retrieve entry
    m_loc_device_list_entry_used[next_free_pos] = true; //mark entry as used

    // clear entry
    memset(p_device, 0, sizeof(logitacker_devices_unifying_device_t));
    // set base addr
    memcpy(p_device->rf_address, rf_addr, sizeof(logitacker_devices_unifying_device_rf_address_t));
    // set prefix
    p_device->addr_prefix = rf_addr[4];

    //set return value
    *pp_device = p_device;
    return NRF_SUCCESS;
}

uint32_t logitacker_devices_get_device(logitacker_devices_unifying_device_t ** pp_device, logitacker_devices_unifying_device_rf_address_t const rf_addr) {
    VERIFY_TRUE(rf_addr != NULL, NRF_ERROR_NULL);
    VERIFY_TRUE(pp_device != NULL, NRF_ERROR_NULL);

    logitacker_devices_unifying_device_t * p_device = NULL;
    for (int i=0; i < LOGITACKER_DEVICES_DEVICE_LIST_MAX_ENTRIES; i++) {
        if (m_loc_device_list_entry_used[i]) { // non empty entry
            if (memcmp(m_loc_device_list[i].rf_address, rf_addr, sizeof(logitacker_devices_unifying_device_rf_address_t)) != 0) continue; //base addr doesn't match
            // if here, we have a match
            p_device = &m_loc_device_list[i];
            break;
        }
    }

    *pp_device = p_device;
    if (p_device == NULL) return NRF_ERROR_NOT_FOUND;

    return NRF_SUCCESS;

}

uint32_t logitacker_devices_del_device_(logitacker_devices_unifying_device_rf_address_t const rf_addr) {
    VERIFY_TRUE(rf_addr != NULL, NRF_ERROR_NULL);

    logitacker_devices_unifying_device_t * p_device = NULL;
    int device_idx = -1;
    for (int i=0; i < LOGITACKER_DEVICES_DEVICE_LIST_MAX_ENTRIES; i++) {
        if (m_loc_device_list_entry_used[i]) { // non empty entry
            if (memcmp(m_loc_device_list[i].rf_address, rf_addr, sizeof(logitacker_devices_unifying_device_rf_address_t)) != 0) continue; //base addr doesn't match
            // if here, we have a match
            p_device = &m_loc_device_list[i];
            device_idx = i;
            break;
        }
    }

    if (device_idx == -1) return NRF_ERROR_NOT_FOUND;

    //mark unused
    m_loc_device_list_entry_used[device_idx] = false;
    //clear device data
    memset(p_device, 0, sizeof(logitacker_devices_unifying_device_t));

    return NRF_SUCCESS;
}

uint32_t logitacker_devices_del_all() {
    logitacker_devices_unifying_device_t * p_device = NULL;

    for (int i=0; i < LOGITACKER_DEVICES_DEVICE_LIST_MAX_ENTRIES; i++) {
        p_device = &m_loc_device_list[i];
        //mark unused
        m_loc_device_list_entry_used[i] = false;
        //clear device data
        memset(p_device, 0, sizeof(logitacker_devices_unifying_device_t));

    }

    logitacker_devices_unifying_dongle_t * p_dongle = NULL;

    for (int i=0; i < LOGITACKER_DEVICES_DONGLE_LIST_MAX_ENTRIES; i++) {
        p_dongle = &m_loc_dongle_list[i];
        //mark unused
        m_loc_dongle_list_entry_used[i] = false;
        //clear device data
        memset(p_dongle, 0, sizeof(logitacker_devices_unifying_dongle_t));

    }



    return NRF_SUCCESS;

}

uint32_t logitacker_devices_get_next_device(logitacker_devices_unifying_device_t ** pp_device, logitacker_devices_list_iterator_t * iter) {
    VERIFY_TRUE(pp_device != NULL, NRF_ERROR_NULL);
    VERIFY_TRUE(iter != NULL, NRF_ERROR_NULL);

    int device_idx = -1;
    for (int i=iter->current_pos; i < LOGITACKER_DEVICES_DEVICE_LIST_MAX_ENTRIES; i++) {
        if (m_loc_device_list_entry_used[i]) { // non empty entry
            // if here, we have a match
            device_idx = i;
            break;
        }
    }


    if (device_idx == -1) {
        iter->current_pos = LOGITACKER_DEVICES_DEVICE_LIST_MAX_ENTRIES; // all elements have been tested, advance iterator
        *pp_device = NULL;
        return NRF_ERROR_NOT_FOUND;
    }

    *pp_device = &m_loc_device_list[device_idx];
    iter->current_pos = device_idx+1;
    return NRF_SUCCESS;
}


uint32_t logitacker_devices_add_given_device_to_dongle(logitacker_devices_unifying_dongle_t * p_dongle, logitacker_devices_unifying_device_t * p_device) {
    VERIFY_TRUE(p_device != NULL, NRF_ERROR_NULL);
    VERIFY_TRUE(p_dongle != NULL, NRF_ERROR_NULL);

    if (p_dongle->num_connected_devices >= LOGITACKER_DEVICES_MAX_DEVICES_PER_DONGLE) return NRF_ERROR_NO_MEM;

    p_dongle->p_connected_devices[p_dongle->num_connected_devices++] = p_device;
    p_device->p_dongle = p_dongle;
    return NRF_SUCCESS;
}


uint32_t logitacker_devices_remove_device_from_dongle(logitacker_devices_unifying_device_t * p_device) {
    VERIFY_TRUE(p_device != NULL, NRF_ERROR_NULL);
    VERIFY_TRUE(p_device->p_dongle != NULL, NRF_ERROR_NULL);

    logitacker_devices_unifying_dongle_t * p_dongle = p_device->p_dongle;

    int device_pos = -1;
    for (int i = 0; i < p_dongle->num_connected_devices; i++) {
        if (p_dongle->p_connected_devices[i] == p_device) {
            device_pos = i;
            break;
        }
    }

    if (device_pos == -1) {
        // should never happen
        NRF_LOG_ERROR("device to remove not connected to dongle");
        return NRF_ERROR_INVALID_PARAM;
    }

    // move all other devices one position towards beginning of array
    p_dongle->num_connected_devices--;
    for (int i = device_pos; i < (p_dongle->num_connected_devices); i++) {
        p_dongle->p_connected_devices[i] = p_dongle->p_connected_devices[i+1];
    }
    for (int i = p_dongle->num_connected_devices; i < LOGITACKER_DEVICES_MAX_DEVICES_PER_DONGLE; i++) {
        p_dongle->p_connected_devices[i] = NULL;
    }

    return NRF_SUCCESS;
}


uint32_t logitacker_devices_create_device(logitacker_devices_unifying_device_t ** pp_device, logitacker_devices_unifying_device_rf_address_t const rf_addr) {
    // check if respective dongle exists
    // --> no, create
    //      --> failed, return error
    // --> yes, check if there's room for additional devices (limited number of devices per dongle)
    //      --> no, return error

    // create device

    // add created device to dongle

    // add pointer to dongle to newly created device


    // check if device already_exists, if yes return device
    if (logitacker_devices_get_device(pp_device, rf_addr) == NRF_SUCCESS) {
        //exists already
        NRF_LOG_INFO("device which should be created already exists");
        return NRF_SUCCESS;
    }

    //try to create device
    if (logitacker_devices_create_device_(pp_device, rf_addr) != NRF_SUCCESS) {
        NRF_LOG_ERROR("Failed to create new device, no memory");
        return NRF_ERROR_NO_MEM;
    }

    logitacker_devices_unifying_device_rf_addr_base_t base_addr = {0};
    logitacker_devices_unifying_device_rf_addr_prefix_t addr_prefix = 0;
    logitacker_devices_unifying_dongle_t * p_dongle = NULL;

    helper_addr_to_base_and_prefix(base_addr, &addr_prefix, rf_addr, sizeof(logitacker_devices_unifying_device_rf_address_t)); // convert address to base and prefix

    if (logitacker_devices_get_dongle_by_base_addr(&p_dongle, base_addr) != NRF_SUCCESS) {
        // respective dongle not found, try to create
        if (logitacker_devices_create_dongle(&p_dongle, base_addr) != NRF_SUCCESS) {
            NRF_LOG_ERROR("Failed to create dongle data for new device, no memory ... deleting created device again");
            logitacker_devices_del_device((*pp_device)->rf_address);
            pp_device = NULL;
            return NRF_ERROR_NO_MEM;
        }
    }

    // when here, p_dongle points to dongle data for device
    if (logitacker_devices_add_given_device_to_dongle(p_dongle, *pp_device) != NRF_SUCCESS) {
        NRF_LOG_ERROR("Failed to add new device to existing dongle ... deleting created device again");
        logitacker_devices_del_device((*pp_device)->rf_address);
        pp_device = NULL;
        return NRF_ERROR_NO_MEM;

    }

    return NRF_SUCCESS;
}

uint32_t logitacker_devices_del_device(logitacker_devices_unifying_device_rf_address_t const rf_addr) {
    logitacker_devices_unifying_device_t * p_device = NULL;

    // check if device already_exists, if yes return device
    if (logitacker_devices_get_device(&p_device, rf_addr) != NRF_SUCCESS) {
        //exists already
        NRF_LOG_INFO("device doesn't exist, no need to delete");
        return NRF_SUCCESS;
    }

    // check if connected to dongle
    logitacker_devices_unifying_dongle_t * p_dongle = p_device->p_dongle;
    if (p_dongle != NULL) {
        //delete device from dongle
        logitacker_devices_remove_device_from_dongle(p_device);

        // if dongle has no more devices left, remove dongle from internal list
        if (p_dongle->num_connected_devices == 0) {
            NRF_LOG_INFO("dongle has no more devices, removing from list...")
            logitacker_devices_del_dongle(p_dongle->base_addr);
        }
    }

    // delete device from internal device list
    return logitacker_devices_del_device_(rf_addr);
}

uint32_t logitacker_devices_device_update_classification(logitacker_devices_unifying_device_t * p_device, nrf_esb_payload_t frame) {
    if (p_device == NULL) {
        NRF_LOG_ERROR("logitacker_devices_device_update_classification: given device pointer is NULL")
        return NRF_ERROR_NULL;
    }

    logitacker_devices_unifying_dongle_t * p_dongle = p_device->p_dongle;
    if (p_dongle == NULL) {
        NRF_LOG_ERROR("logitacker_devices_device_update_classification: given device isn't associated to dongle data")
        return NRF_ERROR_NULL;
    }



    uint8_t len = frame.length;
    uint8_t unifying_report_type;
    bool unifying_is_keep_alive;
    unifying_frame_classify(frame, &unifying_report_type, &unifying_is_keep_alive);


    logitacker_device_frame_counter_t *p_frame_counters = &p_device->frame_counters;
    p_frame_counters->overal++; //overall counter ignores empty frames

    if (len == 0) return NRF_SUCCESS; //ignore empty frames, as we can't retrieve valuable information (ack)


    bool logitech_cksm = unifying_payload_validate_checksum(frame.data, frame.length);
    if (logitech_cksm) {
        p_frame_counters->logitech_chksm++;

        NRF_LOG_INFO("... valid Logitech CRC");
        //test if frame has valid logitech checksum
        if (p_dongle->classification == DONGLE_CLASSIFICATION_UNKNOWN) {
            p_dongle->classification = DONGLE_CLASSIFICATION_IS_LOGITECH;
        }
    } else {
        NRF_LOG_INFO("... INVALID Logitech CRC");
        //dealing with situation where Logitech CRC is wrong due to RX/TX errors isn't needed, as at this point the ESB CRC was already valid
        if (p_dongle->classification == DONGLE_CLASSIFICATION_UNKNOWN) {
            p_dongle->classification = DONGLE_CLASSIFICATION_IS_NOT_LOGITECH;
        }
        return NRF_SUCCESS;
    }


    switch (unifying_report_type) {
        case UNIFYING_RF_REPORT_ENCRYPTED_KEYBOARD:
            if (len != 22) return NRF_ERROR_INVALID_DATA;
            p_device->caps |= LOGITACKER_DEVICE_CAPS_LINK_ENCRYPTION;
            p_device->report_types |= LOGITACKER_DEVICE_REPORT_TYPES_KEYBOARD;
            break;

        case UNIFYING_RF_REPORT_HIDPP_LONG:
            //ToDo: check if HID++ reports provide additional information (f.e. long version of device name is exchanged)
            if (len != 22) return NRF_ERROR_INVALID_DATA;
            p_device->caps |= LOGITACKER_DEVICE_CAPS_UNIFYING_COMPATIBLE;
            p_device->report_types |= LOGITACKER_DEVICE_REPORT_TYPES_LONG_HIDPP;
            break;
        case UNIFYING_RF_REPORT_HIDPP_SHORT:
            if (len != 10) return NRF_ERROR_INVALID_DATA;
            p_device->caps |= LOGITACKER_DEVICE_CAPS_UNIFYING_COMPATIBLE;
            p_device->report_types |= LOGITACKER_DEVICE_REPORT_TYPES_SHORT_HIDPP;
            break;
        case UNIFYING_RF_REPORT_LED:
            if (len != 10) return NRF_ERROR_INVALID_DATA;
            p_device->report_types |= LOGITACKER_DEVICE_REPORT_TYPES_KEYBOARD_LED;
            p_device->report_types |= LOGITACKER_DEVICE_REPORT_TYPES_KEYBOARD;
            break;
        case UNIFYING_RF_REPORT_PAIRING:
            break;
        case UNIFYING_RF_REPORT_PLAIN_KEYBOARD:
            p_device->report_types |= LOGITACKER_DEVICE_REPORT_TYPES_KEYBOARD;
            p_device->vuln_plain_injection = true;
            break;
        case UNIFYING_RF_REPORT_PLAIN_MOUSE:
            if (len != 10) return NRF_ERROR_INVALID_DATA;
            p_device->report_types |= LOGITACKER_DEVICE_REPORT_TYPES_MOUSE;
            break;
        case UNIFYING_RF_REPORT_PLAIN_MULTIMEDIA:
            p_device->report_types |= LOGITACKER_DEVICE_REPORT_TYPES_MULTIMEDIA;
            break;
        case UNIFYING_RF_REPORT_PLAIN_SYSTEM_CTL:
            p_device->report_types |= LOGITACKER_DEVICE_REPORT_TYPES_POWER_KEYS;
            break;
        case UNIFYING_RF_REPORT_SET_KEEP_ALIVE:
            if (len != 10) return NRF_ERROR_INVALID_DATA;
            break;
        default:
            break;
    }

    return NRF_SUCCESS;
}

static int device_entries_used = 0;
static int dongle_entries_used = 0;

void logitacker_devices_log_stats() {
    device_entries_used = 0;
    dongle_entries_used = 0;

    for (int i =0; i < LOGITACKER_DEVICES_DEVICE_LIST_MAX_ENTRIES; i++) {
        if (m_loc_device_list_entry_used[i]) device_entries_used++;
    }
    for (int i =0; i < LOGITACKER_DEVICES_DONGLE_LIST_MAX_ENTRIES; i++) {
        if (m_loc_dongle_list_entry_used[i]) dongle_entries_used++;
    }

    NRF_LOG_INFO("device list: %d out of %d used", device_entries_used, LOGITACKER_DEVICES_DEVICE_LIST_MAX_ENTRIES);
    NRF_LOG_INFO("dongle list: %d out of %d used", dongle_entries_used, LOGITACKER_DEVICES_DONGLE_LIST_MAX_ENTRIES);
}











typedef enum {
    KEYBOARD_REPORT_GEN_MODE_PLAIN, // device accepts plain keystrokes
    KEYBOARD_REPORT_GEN_MODE_ENCRYPTED, // device accepts only encrypted keystrokes, key needed
    KEYBOARD_REPORT_GEN_MODE_ENCRYPTED_COUNTER_FLUSH, // same as encrypted, but enough key releases are sent upfront, to overflow the counter buffer of the receiver (slower)
    KEYBOARD_REPORT_GEN_MODE_ENCRYPTED_XOR_SINGLE, // encrypted injection, without knowledge of keys (targets weak XOR encryption, needs a single "whitened frame")
    KEYBOARD_REPORT_GEN_MODE_ENCRYPTED_XOR, // encrypted injection, without knowledge of keys (targets weak XOR encryption, even if counter reuse is fixed needs about 24 "whitened frame")
} keyboard_report_gen_mode_t;



uint32_t logitacker_devices_generate_keyboard_frame_plain(nrf_esb_payload_t *p_result_payload,
                                                          hid_keyboard_report_t const *const p_in_hid_report) {
    /*
     * 07 C1 00 00 00 00 00 00 00 38
     */

    p_result_payload->length = 10;
    p_result_payload->data[0] = 0x00; //device index
    p_result_payload->data[1] = UNIFYING_RF_REPORT_PLAIN_KEYBOARD | UNIFYING_RF_REPORT_BIT_KEEP_ALIVE | UNIFYING_RF_REPORT_BIT_UNKNOWN; //c1
    p_result_payload->data[2] = p_in_hid_report->mod;
    p_result_payload->data[3] = p_in_hid_report->keys[0];
    p_result_payload->data[4] = p_in_hid_report->keys[1];
    p_result_payload->data[5] = p_in_hid_report->keys[2];
    p_result_payload->data[6] = p_in_hid_report->keys[3];
    p_result_payload->data[7] = p_in_hid_report->keys[4];
    p_result_payload->data[8] = p_in_hid_report->keys[5];
    p_result_payload->data[9] = 0x00; // will be overwritten by logitech checksum
    unifying_payload_update_checksum(p_result_payload->data, p_result_payload->length);

    return NRF_SUCCESS;
}

uint32_t logitacker_devices_generate_keyboard_frame_encrypted(logitacker_devices_unifying_device_t const *const p_caps,
                                                              nrf_esb_payload_t *p_result_payload,
                                                              hid_keyboard_report_t const *const p_in_hid_report) {
    NRF_LOG_WARNING("Encrypted injection not implemented, yet");
    return NRF_ERROR_INVALID_PARAM;
}

uint32_t logitacker_devices_generate_keyboard_frame(logitacker_devices_unifying_device_t *p_caps,
                                                    nrf_esb_payload_t *p_result_payload,
                                                    hid_keyboard_report_t const *const p_in_hid_report) {
    keyboard_report_gen_mode_t mode = KEYBOARD_REPORT_GEN_MODE_PLAIN;

    if (p_caps != NULL) {
        if (p_caps->is_encrypted) {
            if (p_caps->key_known) {
                mode = KEYBOARD_REPORT_GEN_MODE_ENCRYPTED;
            } else {
                if (p_caps->has_enough_whitened_reports) {
                    mode = KEYBOARD_REPORT_GEN_MODE_ENCRYPTED_XOR;
                } else if (p_caps->has_single_whitened_report) {
                    mode = KEYBOARD_REPORT_GEN_MODE_ENCRYPTED_XOR_SINGLE;
                } else {
                    NRF_LOG_WARNING("No proper key injection mode for device, fallback to PLAIN injection");
                }
            }
        }
    } else {
        NRF_LOG_WARNING("Device with unknown devices, trying PLAIN injection");
    }



    switch (mode) {
        case KEYBOARD_REPORT_GEN_MODE_PLAIN:
            return logitacker_devices_generate_keyboard_frame_plain(p_result_payload, p_in_hid_report);
        case KEYBOARD_REPORT_GEN_MODE_ENCRYPTED:
            return logitacker_devices_generate_keyboard_frame_encrypted(p_caps, p_result_payload, p_in_hid_report);
        default:
            return logitacker_devices_generate_keyboard_frame_plain(p_result_payload, p_in_hid_report);
    }

}


