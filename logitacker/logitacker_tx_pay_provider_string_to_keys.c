#include "utf.h"
#include "logitacker_tx_pay_provider_string_to_keys.h"
#include "logitacker_keyboard_map.h"
#include "logitacker_devices.h"

#define NRF_LOG_MODULE_NAME TX_PAY_PROVIDER_STRING_TO_KEYS
#include "nrf_log.h"

NRF_LOG_MODULE_REGISTER();



typedef struct {

    char const * source_string;

    logitacker_keyboard_map_u8_str_parser_ctx_t str_parser_ctx;
    hid_keyboard_report_t * p_current_hid_report_seq;
    //uint32_t current_report_sequence_size; //size of hid report sequence in bytes
    uint32_t remaining_reports_in_sequence; // count of hid reports in current sequence
    logitacker_keyboard_map_lang_t language_layout;
    //uint32_t pos_in_seq; //position in current report sequence
    logitacker_devices_unifying_device_t * p_device;

} logitacker_tx_payload_provider_string_ctx_t;

bool provider_string_get_next(logitacker_tx_payload_provider_t *self, nrf_esb_payload_t *p_next_payload);
void provider_string_reset(logitacker_tx_payload_provider_t *self);
bool provider_string_inject_get_next(logitacker_tx_payload_provider_string_ctx_t *self,
                                     nrf_esb_payload_t *p_next_payload);
void provider_string_inject_reset(logitacker_tx_payload_provider_string_ctx_t *self);

bool provider_string_get_next_hid_report_seq(logitacker_tx_payload_provider_string_ctx_t *self);

const static int SINGLE_HID_REPORT_SIZE = sizeof(hid_keyboard_report_t);
static logitacker_tx_payload_provider_t m_local_provider;
static logitacker_tx_payload_provider_string_ctx_t m_local_ctx;

// updates self->current_hid_report_seq with next UTF-8 rune from string, returns false if end of string is reached, true otherwise
bool provider_string_get_next_hid_report_seq(logitacker_tx_payload_provider_string_ctx_t *self) {
    uint32_t report_seq_size;
    uint32_t res = logitacker_keyboard_map_u8_str_to_hid_reports(&self->str_parser_ctx, self->source_string, &self->p_current_hid_report_seq, &report_seq_size, self->language_layout);
    if (res == NRF_SUCCESS) {
        self->remaining_reports_in_sequence = report_seq_size / SINGLE_HID_REPORT_SIZE;
        NRF_LOG_INFO("Reports in next sequence %d", self->remaining_reports_in_sequence);
    }

    NRF_LOG_DEBUG("reports for next rune:");
    NRF_LOG_HEXDUMP_DEBUG(self->p_current_hid_report_seq, report_seq_size); //log !raw! resulting report array


    return res == NRF_SUCCESS;
}

bool provider_string_get_next(logitacker_tx_payload_provider_t *self, nrf_esb_payload_t *p_next_payload) {
    return provider_string_inject_get_next((logitacker_tx_payload_provider_string_ctx_t *) (self->p_ctx), p_next_payload);
}

void provider_string_reset(logitacker_tx_payload_provider_t *self) {
    provider_string_inject_reset((logitacker_tx_payload_provider_string_ctx_t *) (self->p_ctx));
    return;
}

static int rc = 0;
static void convert_hid_report_to_rf_payload(logitacker_tx_payload_provider_string_ctx_t * self, nrf_esb_payload_t *p_next_payload, hid_keyboard_report_t * p_hid_report) {

    NRF_LOG_DEBUG("HID report to translate to RF frame (%d):", rc++);
    NRF_LOG_HEXDUMP_DEBUG(p_hid_report, sizeof(hid_keyboard_report_t));

    logitacker_devices_generate_keyboard_frame(self->p_device, p_next_payload, p_hid_report);

    NRF_LOG_INFO("Updated TX payload (%d):", rc++);
    NRF_LOG_HEXDUMP_INFO(p_next_payload->data, p_next_payload->length);

}

bool provider_string_inject_get_next(logitacker_tx_payload_provider_string_ctx_t *self, nrf_esb_payload_t *p_next_payload) {
    if (self->remaining_reports_in_sequence <= 0) {
        // there are no reports left in current sequence, fetch next sequence
        if(!provider_string_get_next_hid_report_seq(self)) {
            // no more report sequence to retrieve, we are done
            return false;
        }
    }



    //return next position in current sequence
    hid_keyboard_report_t * p_cur_hid_report = self->p_current_hid_report_seq;

    convert_hid_report_to_rf_payload(self, p_next_payload, p_cur_hid_report);


    self->remaining_reports_in_sequence--;
    //advance pointer to next report
    if (self->remaining_reports_in_sequence > 0) self->p_current_hid_report_seq++;


    return true;
}


void provider_string_inject_reset(logitacker_tx_payload_provider_string_ctx_t *self) {
    memset(&self->str_parser_ctx, 0, sizeof(self->str_parser_ctx));
    self->str_parser_ctx.append_release = true;
    self->remaining_reports_in_sequence = 0;
    self->p_current_hid_report_seq = NULL;
}


logitacker_tx_payload_provider_t * new_payload_provider_string(logitacker_devices_unifying_device_t * p_device_caps, logitacker_keyboard_map_lang_t lang, char const * const str) {
    if (p_device_caps == NULL) {
        NRF_LOG_WARNING("cannot create payload provider string, no device capabilities given");
    }

    // no real instance, as a static object is used (has to be malloced)
    m_local_provider.p_get_next = provider_string_get_next;
    m_local_provider.p_reset = provider_string_reset;

    // again no real instance

    m_local_ctx.p_device = p_device_caps;
    m_local_ctx.language_layout = lang;
    m_local_ctx.p_current_hid_report_seq = NULL;
    m_local_ctx.remaining_reports_in_sequence = 0;
    m_local_ctx.source_string = str;
    memset(&m_local_ctx.str_parser_ctx, 0, sizeof(m_local_ctx.str_parser_ctx)); //clear string parser context
    m_local_ctx.str_parser_ctx.append_release = true; //append key release reports
    m_local_provider.p_ctx = &m_local_ctx;

    return &m_local_provider;
}
