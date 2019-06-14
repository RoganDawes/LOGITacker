#include "logitacker_tx_payload_provider_string_to_altkeys.h"


#define NRF_LOG_MODULE_NAME TX_PAY_PROVIDER_STRING_TO_ALTKEYS
#include "nrf_log.h"

NRF_LOG_MODULE_REGISTER();


typedef struct {

    char const * source_string;

    const char * p_pos_string;
    //bool append_release;
    hid_keyboard_report_t current_hid_report_seq[8];
    uint8_t report_sequence_pos; // count of hid reports in current sequence
    uint8_t report_sequence_len; // count of hid reports in current sequence
    logitacker_devices_unifying_device_t * p_device;

} logitacker_tx_payload_provider_altstring_ctx_t;


static logitacker_tx_payload_provider_t m_local_provider;
static logitacker_tx_payload_provider_altstring_ctx_t m_local_ctx;

bool provider_altstring_get_next(logitacker_tx_payload_provider_t *self, nrf_esb_payload_t *p_next_payload) {
    if (m_local_ctx.report_sequence_pos >= m_local_ctx.report_sequence_len) {
        // if no more reports to send in current sequence, update HID report sequence based on next run
        if (m_local_ctx.p_pos_string == NULL) m_local_ctx.p_pos_string = m_local_ctx.source_string; //initialize position pointer for string
        else m_local_ctx.p_pos_string++; //otherwise, advance string position

        // if end of string reached, return false
        if (*m_local_ctx.p_pos_string == 0x00) return false;

        // fetch next char as ordinal value
        m_local_ctx.report_sequence_len = 0;
        uint8_t ord = *m_local_ctx.p_pos_string;


        memset(m_local_ctx.current_hid_report_seq, 0, sizeof(hid_keyboard_report_t) * 8);

        // parse char
        m_local_ctx.current_hid_report_seq[m_local_ctx.report_sequence_len].keys[0] = HID_KEY_NONE;
        m_local_ctx.current_hid_report_seq[m_local_ctx.report_sequence_len].mod = HID_MOD_KEY_LEFT_ALT;
        m_local_ctx.report_sequence_len++;

        uint8_t val = ord / 100;
        if (val != 0) {
            m_local_ctx.current_hid_report_seq[m_local_ctx.report_sequence_len].keys[0] = HID_KEY_KPENTER + val;
            m_local_ctx.current_hid_report_seq[m_local_ctx.report_sequence_len].mod = HID_MOD_KEY_LEFT_ALT;
            ord -= val * 100;
            m_local_ctx.report_sequence_len++;

            m_local_ctx.current_hid_report_seq[m_local_ctx.report_sequence_len].keys[0] = HID_KEY_NONE;
            m_local_ctx.current_hid_report_seq[m_local_ctx.report_sequence_len].mod = HID_MOD_KEY_LEFT_ALT;
            m_local_ctx.report_sequence_len++;
        }

        val = ord / 10;
        if (val == 0) {
            m_local_ctx.current_hid_report_seq[m_local_ctx.report_sequence_len].keys[0] = HID_KEY_KP0;
        } else {
            m_local_ctx.current_hid_report_seq[m_local_ctx.report_sequence_len].keys[0] = HID_KEY_KPENTER + val;
        }
        m_local_ctx.current_hid_report_seq[m_local_ctx.report_sequence_len].mod = HID_MOD_KEY_LEFT_ALT;
        ord -= val * 10;
        m_local_ctx.report_sequence_len++;
        m_local_ctx.current_hid_report_seq[m_local_ctx.report_sequence_len].keys[0] = HID_KEY_NONE;
        m_local_ctx.current_hid_report_seq[m_local_ctx.report_sequence_len].mod = HID_MOD_KEY_LEFT_ALT;
        m_local_ctx.report_sequence_len++;

        val = ord;
        if (val == 0) {
            m_local_ctx.current_hid_report_seq[m_local_ctx.report_sequence_len].keys[0] = HID_KEY_KP0;
        } else {
            m_local_ctx.current_hid_report_seq[m_local_ctx.report_sequence_len].keys[0] = HID_KEY_KPENTER + val;
        }
        m_local_ctx.current_hid_report_seq[m_local_ctx.report_sequence_len].mod = HID_MOD_KEY_LEFT_ALT;
        m_local_ctx.report_sequence_len++;
        m_local_ctx.current_hid_report_seq[m_local_ctx.report_sequence_len].keys[0] = HID_KEY_NONE;
        m_local_ctx.current_hid_report_seq[m_local_ctx.report_sequence_len].mod = HID_MOD_KEY_LEFT_ALT;
        m_local_ctx.report_sequence_len++;

        m_local_ctx.report_sequence_len++;

        m_local_ctx.report_sequence_pos = 0;
    }

    // if not all reports of current sequence are returned, return next one
    hid_keyboard_report_t * p_hid_report = &m_local_ctx.current_hid_report_seq[m_local_ctx.report_sequence_pos++];

    NRF_LOG_INFO("HID report to translate to RF frame: %d", m_local_ctx.report_sequence_pos);
    NRF_LOG_HEXDUMP_INFO(p_hid_report, sizeof(hid_keyboard_report_t));

    logitacker_devices_generate_keyboard_frame(m_local_ctx.p_device, p_next_payload, p_hid_report);

    NRF_LOG_INFO("Updated TX ESB payload (%d):");
    NRF_LOG_HEXDUMP_INFO(p_next_payload->data, p_next_payload->length);

    return true;

}

void provider_altstring_reset(logitacker_tx_payload_provider_t *self) {
    m_local_ctx.report_sequence_pos = 0;
    m_local_ctx.report_sequence_len = 0;
    memset(m_local_ctx.current_hid_report_seq, 0, sizeof(hid_keyboard_report_t) * 8);
    m_local_ctx.p_pos_string = NULL;
    return;
}


logitacker_tx_payload_provider_t * new_payload_provider_altstring(logitacker_devices_unifying_device_t * p_device_caps, char const * const str) {
    if (p_device_caps == NULL) {
        NRF_LOG_WARNING("cannot create payload provider string, no device capabilities given");
    }

    // no real instance, as a static object is used (has to be malloced)
    m_local_provider.p_get_next = provider_altstring_get_next;
    m_local_provider.p_reset = provider_altstring_reset;

    // again no real instance

    m_local_ctx.p_device = p_device_caps;
    memset(m_local_ctx.current_hid_report_seq, 0, sizeof(hid_keyboard_report_t) * 4);
    m_local_ctx.report_sequence_pos = 0;
    m_local_ctx.report_sequence_len = 0;
    m_local_ctx.source_string = str;
    m_local_ctx.p_pos_string = NULL;
    m_local_provider.p_ctx = &m_local_ctx;

    return &m_local_provider;
}

