#include "logitacker_devices.h"
#include "unifying.h"
#include "helper.h"

#define NRF_LOG_MODULE_NAME LOGITACKER_DEVICES
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

typedef struct {
    bool is_used;
} logitacker_device_list_entry_state_t;

logitacker_device_t m_dev_list[LOGITACKER_DEVICES_MAX_LIST_ENTRIES];
logitacker_device_list_entry_state_t m_dev_list_state[LOGITACKER_DEVICES_MAX_LIST_ENTRIES];

// searches usable list index, returns -1 if none found
int find_free_entry() {
    int free_entry = -1;
    for (uint32_t i=0; i < LOGITACKER_DEVICES_MAX_LIST_ENTRIES; i++) {
        if (!m_dev_list_state[i].is_used) {
            free_entry = i;
            break;
        }
    }
    return free_entry;
}

int find_entry_by_base(uint8_t *base_addr) {
    int entry = -1;
    for (uint32_t i=0; i < LOGITACKER_DEVICES_MAX_LIST_ENTRIES; i++) {
        if (m_dev_list_state[i].is_used) { // non empty entry
            if (memcmp(m_dev_list[i].base_addr, base_addr, 4) != 0) continue; //base addr doesn't match
            // if here, we have a match
            entry = i;
            break;
        }
    }
    return entry;
}


int find_entry_by_base_prefix(uint8_t *base_addr, uint8_t prefix) {
    // find device with correct base addr first
    
    int device_index = find_entry_by_base(base_addr);
    if (device_index < 0) return device_index; // return error value

    // check if prefix matches, return -1 otherwise
    int prefix_index = 0;
    uint32_t err = logitacker_device_get_prefix_index(&prefix_index, &m_dev_list[device_index], prefix);
    if (err == NRF_SUCCESS) return device_index;

    return -1; //error value
}



uint32_t logitacker_device_get_prefix_index(int *out_index, logitacker_device_t const * const in_device, uint8_t prefix) {
    ASSERT(in_device);
    for (int i=0; i<in_device->num_prefixes; i++) {
        if (in_device->prefixes[i] == prefix) {
            *out_index = (uint8_t) i;
            return NRF_SUCCESS;
        }
    }
    return NRF_ERROR_INVALID_PARAM;;
}

uint32_t logitacker_device_add_prefix(logitacker_device_t * out_device, uint8_t prefix) {
    ASSERT(out_device);

    // check if prefix is already present
    int prefix_index = 0;
    uint32_t err_res = logitacker_device_get_prefix_index(&prefix_index, out_device, prefix);

    if (err_res == NRF_SUCCESS) return err_res; // prefix already exists

    if (!(out_device->num_prefixes < LOGITACKER_DEVICE_MAX_PREFIX)) return NRF_ERROR_NO_MEM; // no room to add additional device indices 

    out_device->prefixes[out_device->num_prefixes++] = prefix; //add the new prefiy and inc num_prefixes

    return NRF_SUCCESS;
}


/* NEW */

logitacker_device_t* logitacker_device_list_add_addr(uint8_t const * const rf_addr) {
    ASSERT(rf_addr);

    // translate to base and prefix
    uint8_t prefix = 0;
    uint8_t base[4] = { 0 };
    helper_addr_to_base_and_prefix(base, &prefix, rf_addr, LOGITACKER_DEVICE_ADDR_LEN);

    // fetch device with corresponding base addr, create otherwise
    logitacker_device_t * p_device = NULL;
    int device_index = find_entry_by_base(base);
    if (device_index == -1) { // new entry needed
        // find first free entry
        int pos = find_free_entry();
        if (pos < 0) {
            NRF_LOG_ERROR("cannot add additional devices, storage limit reached");
            return NULL;
        }

        memset(&m_dev_list[pos], 0, sizeof(logitacker_device_t));
        memcpy(m_dev_list[pos].base_addr, base, 4);
        p_device = &m_dev_list[pos];

        m_dev_list_state[pos].is_used = true;
    } else {
        p_device = &m_dev_list[device_index];
    }

    // add given prefix
    if (logitacker_device_add_prefix(p_device, prefix) != NRF_SUCCESS) {
        NRF_LOG_ERROR("cannot add prefix to existing device, prefix limit reached");
        return NULL;
    }

    return p_device;
}

logitacker_device_t* logitacker_device_list_get_by_addr(uint8_t const * const addr) {
    if (addr == NULL) return NULL;

    // resolve base address / prefix
    uint8_t base[4] = {0};
    uint8_t prefix = 0;
    helper_addr_to_base_and_prefix(base, &prefix, addr, LOGITACKER_DEVICE_ADDR_LEN);

    //retrieve device
    int entry = find_entry_by_base(base);
    if (entry < 0) {
        NRF_LOG_ERROR("logitacker_device_list_get_by_addr: no entry for base address")
        return NULL;
    }

    logitacker_device_t * p_device = &m_dev_list[entry];

    int prefix_index = -1;
    uint32_t err_res = logitacker_device_get_prefix_index(&prefix_index, p_device, prefix);
    if (err_res != NRF_SUCCESS) {
        NRF_LOG_ERROR("logitacker_device_list_get_by_addr: no entry for given prefix")
        return NULL;
    }

    return p_device;
}

logitacker_device_t* logitacker_device_list_get(uint32_t pos) {
    if (pos >= LOGITACKER_DEVICES_MAX_LIST_ENTRIES) return NULL;
    if (!m_dev_list_state[pos].is_used) return NULL;
    return &m_dev_list[pos];
}

uint32_t logitacker_device_list_remove_by_addr(uint8_t const * const rf_addr);
uint32_t logitacker_device_list_remove_by_base(uint8_t const * const base_addr);

void logitacker_device_list_flush() {
    for (uint32_t i=0; i < LOGITACKER_DEVICES_MAX_LIST_ENTRIES; i++) m_dev_list_state[i].is_used = false;
}

logitacker_device_capabilities_t * logitacker_device_get_caps_pointer(uint8_t const * const rf_addr) {
    logitacker_device_t * p_dev = logitacker_device_list_get_by_addr(rf_addr);
    if (p_dev == NULL) return NULL;

    int prefix_index = -1;
    uint32_t err_res = logitacker_device_get_prefix_index(&prefix_index, p_dev, rf_addr[4]);
    if (err_res != NRF_SUCCESS) {
        NRF_LOG_ERROR("logitacker_device_get_caps_pointer: no entry for given prefix")
        return NULL;
    }


    return &p_dev->capabilities[prefix_index];
}

void logitacker_device_update_counters_from_frame(uint8_t const * const rf_addr, nrf_esb_payload_t frame) {
    ASSERT(rf_addr);
    uint8_t len = frame.length;
    uint8_t unifying_report_type;
    bool unifying_is_keep_alive;
    unifying_frame_classify(frame, &unifying_report_type, &unifying_is_keep_alive);


    if (len == 0) return; //ignore empty frames (ack)

    // resolve base addre / prefix
    uint8_t base[4] = {0};
    uint8_t prefix = 0;
    helper_addr_to_base_and_prefix(base, &prefix, rf_addr, LOGITACKER_DEVICE_ADDR_LEN);

    //retrieve device
    int entry = find_entry_by_base(base);
    if (entry < 0) {
        NRF_LOG_ERROR("logitacker_device_update_counters_from_frame: no entry for base address")
        return;
    }

    logitacker_device_t * p_device = &m_dev_list[entry];

    int prefix_index = -1;
    uint32_t err_res = logitacker_device_get_prefix_index(&prefix_index, p_device, prefix);
    if (err_res != NRF_SUCCESS) {
        NRF_LOG_ERROR("logitacker_device_update_counters_from_frame: no data to update for given prefix")
        return;
    }

    // get counters struct
    logitacker_device_frame_counter_t *p_frame_counters = &p_device->frame_counters[prefix_index];
    logitacker_device_capabilities_t *p_caps = &p_device->capabilities[prefix_index];



    p_frame_counters->overal++; //overall counter ignores empty frames

    //test if frame has valid logitech checksum
    bool logitech_cksm = unifying_payload_validate_checksum(frame.data, frame.length);
    if (logitech_cksm) {
        p_caps->is_logitech = true;
        p_frame_counters->logitech_chksm++;
    }

    if (len == 5 && unifying_report_type == 0x00 && unifying_is_keep_alive) {
        //keep alive frame, set respective device to be unifying
        p_caps->is_logitech = true;
    } else {
        switch (unifying_report_type) {
            case UNIFYING_RF_REPORT_ENCRYPTED_KEYBOARD:
                if (len != 22) return;
                if (++p_frame_counters->typed[LOGITACKER_COUNTER_TYPE_UNIFYING_ENCRYPTED_KEYBOARD] > 2 || logitech_cksm) {
                    p_caps->is_logitech = true;
                    p_caps->is_encrypted_keyboard = true;
                }
                break;
            case UNIFYING_RF_REPORT_HIDPP_LONG:
                if (len != 22) return;
                if (++p_frame_counters->typed[LOGITACKER_COUNTER_TYPE_UNIFYING_HIDPP_LONG] > 2 || logitech_cksm) {
                    p_caps->is_logitech = true;
                    p_caps->is_unifying_compatible = true;
                }
                break;
            case UNIFYING_RF_REPORT_HIDPP_SHORT:
                if (len != 10) return;
                if (++p_frame_counters->typed[LOGITACKER_COUNTER_TYPE_UNIFYING_HIDPP_SHORT] > 2 || logitech_cksm) {
                    p_caps->is_logitech = true;
                    p_caps->is_unifying_compatible = true;
                }
                break;
            case UNIFYING_RF_REPORT_LED:
                if (len != 10) return;
                p_frame_counters->typed[LOGITACKER_COUNTER_TYPE_UNIFYING_LED]++;
                break;
            case UNIFYING_RF_REPORT_PAIRING:
                p_frame_counters->typed[LOGITACKER_COUNTER_TYPE_UNIFYING_PAIRING]++;
                break;
            case UNIFYING_RF_REPORT_PLAIN_KEYBOARD:
                if (++p_frame_counters->typed[LOGITACKER_COUNTER_TYPE_UNIFYING_PLAIN_KEYBOARD] > 2 || logitech_cksm) {
                    p_caps->is_logitech = true;
                    p_caps->is_plain_keyboard = true;
                } 
                break;
            case UNIFYING_RF_REPORT_PLAIN_MOUSE:
                if (len != 10) return;
                if (++p_frame_counters->typed[LOGITACKER_COUNTER_TYPE_UNIFYING_PLAIN_MOUSE] > 2 || logitech_cksm) {
                    p_caps->is_logitech = true;
                    p_caps->is_mouse = true;
                }
                break;
            case UNIFYING_RF_REPORT_PLAIN_MULTIMEDIA:
                p_frame_counters->typed[LOGITACKER_COUNTER_TYPE_UNIFYING_PLAIN_MULTIMEDIA]++;
                break;
            case UNIFYING_RF_REPORT_PLAIN_SYSTEM_CTL:
                p_frame_counters->typed[LOGITACKER_COUNTER_TYPE_UNIFYING_PLAIN_SYSTEM_CTL]++;
                break;
            case UNIFYING_RF_REPORT_SET_KEEP_ALIVE:
                if (len != 10) return;
                p_frame_counters->typed[LOGITACKER_COUNTER_TYPE_UNIFYING_SET_KEEP_ALIVE]++;
                break;
            default:
                p_frame_counters->typed[LOGITACKER_COUNTER_TYPE_UNIFYING_INVALID]++; //unknown frame type
        }
        

        NRF_LOG_INFO("device (frames %d): logitech %d, mouse %d, enc_key %d, plain_key %d", \
            p_frame_counters->overal, \
            p_frame_counters->logitech_chksm, \
            p_frame_counters->typed[LOGITACKER_COUNTER_TYPE_UNIFYING_PLAIN_MOUSE], \
            p_frame_counters->typed[LOGITACKER_COUNTER_TYPE_UNIFYING_ENCRYPTED_KEYBOARD], \
            p_frame_counters->typed[LOGITACKER_COUNTER_TYPE_UNIFYING_PLAIN_KEYBOARD]);
    }
}
