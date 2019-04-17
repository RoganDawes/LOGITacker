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

void logitacker_device_update_counters_from_frame(logitacker_device_t *device, nrf_esb_payload_t frame) {
    ASSERT(device);
    uint8_t len = frame.length;
    uint8_t unifying_report_type;
    bool unifying_is_keep_alive;
    unifying_frame_classify(frame, &unifying_report_type, &unifying_is_keep_alive);


    if (len == 0) return; //ignore empty frames (ack)

    device->frame_counters.overal++; //overall counter ignores empty frames

    //test if frame has valid logitech checksum
    bool logitech_cksm = unifying_payload_validate_checksum(frame.data, frame.length);
    if (logitech_cksm) {
        device->is_logitech = true;
        device->frame_counters.logitech_chksm++;
    }

    if (len == 5 && unifying_report_type == 0x00 && unifying_is_keep_alive) {
        //keep alive frame, set respective device to be unifying
        device->is_logitech = true;
    } else {
        switch (unifying_report_type) {
            case UNIFYING_RF_REPORT_ENCRYPTED_KEYBOARD:
                if (len != 22) return;
                if (++device->frame_counters.typed[LOGITACKER_COUNTER_TYPE_UNIFYING_ENCRYPTED_KEYBOARD] > 2 || logitech_cksm) {
                    device->is_logitech = true;
                    device->is_encrypted_keyboard = true;
                }
                break;
            case UNIFYING_RF_REPORT_HIDPP_LONG:
                if (len != 22) return;
                if (++device->frame_counters.typed[LOGITACKER_COUNTER_TYPE_UNIFYING_HIDPP_LONG] > 2 || logitech_cksm) {
                    device->is_logitech = true;
                    device->is_unifying_compatible = true;
                }
                break;
            case UNIFYING_RF_REPORT_HIDPP_SHORT:
                if (len != 10) return;
                if (++device->frame_counters.typed[LOGITACKER_COUNTER_TYPE_UNIFYING_HIDPP_SHORT] > 2 || logitech_cksm) {
                    device->is_logitech = true;
                    device->is_unifying_compatible = true;
                }
                break;
            case UNIFYING_RF_REPORT_LED:
                device->frame_counters.typed[LOGITACKER_COUNTER_TYPE_UNIFYING_LED]++;
                break;
            case UNIFYING_RF_REPORT_PAIRING:
                device->frame_counters.typed[LOGITACKER_COUNTER_TYPE_UNIFYING_PAIRING]++;
                break;
            case UNIFYING_RF_REPORT_PLAIN_KEYBOARD:
                if (++device->frame_counters.typed[LOGITACKER_COUNTER_TYPE_UNIFYING_PLAIN_KEYBOARD] > 2 || logitech_cksm) {
                    device->is_logitech = true;
                    device->is_plain_keyboard = true;
                } 
                break;
            case UNIFYING_RF_REPORT_PLAIN_MOUSE:
                if (len != 10) return;
                if (++device->frame_counters.typed[LOGITACKER_COUNTER_TYPE_UNIFYING_PLAIN_MOUSE] > 2 || logitech_cksm) {
                    device->is_logitech = true;
                    device->is_mouse = true;
                }
                break;
            case UNIFYING_RF_REPORT_PLAIN_MULTIMEDIA:
                device->frame_counters.typed[LOGITACKER_COUNTER_TYPE_UNIFYING_PLAIN_MULTIMEDIA]++;
                break;
            case UNIFYING_RF_REPORT_PLAIN_SYSTEM_CTL:
                device->frame_counters.typed[LOGITACKER_COUNTER_TYPE_UNIFYING_PLAIN_SYSTEM_CTL]++;
                break;
            case UNIFYING_RF_REPORT_SET_KEEP_ALIVE:
                if (len != 10) return;
                device->frame_counters.typed[LOGITACKER_COUNTER_TYPE_UNIFYING_SET_KEEP_ALIVE]++;
                break;
            default:
                device->frame_counters.typed[LOGITACKER_COUNTER_TYPE_UNIFYING_INVALID]++; //unknown frame type
        }
        

        NRF_LOG_INFO("device (frames %d): logitech %d, mouse %d, enc_key %d, plain_key %d", \
            device->frame_counters.overal, \
            device->frame_counters.logitech_chksm, \
            device->frame_counters.typed[LOGITACKER_COUNTER_TYPE_UNIFYING_PLAIN_MOUSE], \
            device->frame_counters.typed[LOGITACKER_COUNTER_TYPE_UNIFYING_ENCRYPTED_KEYBOARD], \
            device->frame_counters.typed[LOGITACKER_COUNTER_TYPE_UNIFYING_PLAIN_KEYBOARD]);
    }
}

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

int find_entry_by_base_prefix(uint8_t *base_addr, uint8_t prefix) {
    int entry = -1;
    for (uint32_t i=0; i < LOGITACKER_DEVICES_MAX_LIST_ENTRIES; i++) {
        if (m_dev_list_state[i].is_used) { // non empty entry
            if (m_dev_list[i].addr_prefix != prefix) continue; //no prefix match, check next
            if (memcmp(m_dev_list[i].base_addr, base_addr, 4) != 0) continue; //base addr doesn't match
            // if here, we have a match
            entry = i;
            break;
        }
    }
    return entry;
}

logitacker_device_t* logitacker_device_list_add(logitacker_device_t device) {
    // find first free entry
    int pos = find_free_entry();
    if (pos < 0) return NULL;

    memcpy(&m_dev_list[pos], &device, sizeof(device));
    m_dev_list_state[pos].is_used = true;
    return &m_dev_list[pos];
};

logitacker_device_t* logitacker_device_list_get(uint32_t pos) {
    if (pos >= LOGITACKER_DEVICES_MAX_LIST_ENTRIES) return NULL;
    if (!m_dev_list_state[pos].is_used) return NULL;
    return &m_dev_list[pos];
}

logitacker_device_t* logitacker_device_list_get_by_base_prefix(uint8_t *base_addr, uint8_t prefix) {
    int pos = find_entry_by_base_prefix(base_addr, prefix);
    if (pos < 0) return NULL;
    return &m_dev_list[pos];
}

logitacker_device_t* logitacker_device_list_get_by_addr(uint8_t *addr) {
    uint8_t base_addr[4] = {0};
    uint8_t prefix = 0;
    helper_addr_to_base_and_prefix(base_addr, &prefix, addr, 5);
    return logitacker_device_list_get_by_base_prefix(base_addr, prefix);
}

uint32_t logitacker_device_list_remove_by_base_prefix(uint8_t *base_addr, uint8_t prefix) {
    int pos = find_entry_by_base_prefix(base_addr, prefix);
    if (pos < 0) return NRF_ERROR_INVALID_ADDR;
    m_dev_list_state[pos].is_used = false; // mark as unused
    return NRF_SUCCESS;    
}

uint32_t logitacker_device_list_remove_by_addr(uint8_t *addr) {
    uint8_t base_addr[4] = {0};
    uint8_t prefix = 0;
    helper_addr_to_base_and_prefix(base_addr, &prefix, addr, 5);
    return logitacker_device_list_remove_by_base_prefix(base_addr, prefix);
}

uint32_t logitacker_device_list_remove(logitacker_device_t device) {
    return logitacker_device_list_remove_by_base_prefix(device.base_addr, device.addr_prefix);
}

void logitacker_device_list_flush() {
    for (uint32_t i=0; i < LOGITACKER_DEVICES_MAX_LIST_ENTRIES; i++) m_dev_list_state[i].is_used = false;
}
