#include "logitacker_tx_payload_provider_press_to_keys.h"
#include <utf.h>
#include "logitacker_keyboard_map.h"

#define NRF_LOG_MODULE_NAME TX_PAY_PROVIDER_PRESS_TO_KEYS
#include "nrf_log.h"
#include "logitacker_devices.h"

NRF_LOG_MODULE_REGISTER();


static hid_keyboard_report_t release_report = {0};

typedef struct {

    char const * source_string;

    //logitacker_keyboard_map_u8_str_parser_ctx_t str_parser_ctx;
    //hid_keyboard_report_t * p_current_hid_report_seq;
    //uint32_t remaining_reports_in_sequence; // count of hid reports in current sequence
    bool append_release;
    bool key_combo_transmitted;
    bool release_transmitted;
    hid_keyboard_report_t combo_hid_report;
    logitacker_keyboard_map_lang_t language_layout;
    logitacker_devices_unifying_device_t * p_device;

} logitacker_tx_payload_provider_press_ctx_t;

bool provider_press_get_next(logitacker_tx_payload_provider_t * self, nrf_esb_payload_t *p_next_payload);
void provider_press_reset(logitacker_tx_payload_provider_t * self);
bool provider_press_inject_get_next(logitacker_tx_payload_provider_press_ctx_t * self, nrf_esb_payload_t *p_next_payload);
void provider_press_inject_reset(logitacker_tx_payload_provider_press_ctx_t * self);

static logitacker_tx_payload_provider_t m_local_provider;
static logitacker_tx_payload_provider_press_ctx_t m_local_ctx;


bool provider_press_get_next(logitacker_tx_payload_provider_t * self, nrf_esb_payload_t *p_next_payload) {
    return provider_press_inject_get_next((logitacker_tx_payload_provider_press_ctx_t *) (self->p_ctx), p_next_payload);
}

void provider_press_reset(logitacker_tx_payload_provider_t * self) {
    provider_press_inject_reset((logitacker_tx_payload_provider_press_ctx_t *) (self->p_ctx));
    return;
}

static int rc = 0;
static void convert_hid_report_to_rf_payload(logitacker_tx_payload_provider_press_ctx_t * self, nrf_esb_payload_t *p_out_payload, hid_keyboard_report_t * p_hid_report) {
    // ToDo: report format needs to be derived from davice capabilities (enctrypted / plain)
    NRF_LOG_DEBUG("HID report to translate to RF frame (%d):", rc++);
    NRF_LOG_HEXDUMP_DEBUG(p_hid_report, sizeof(hid_keyboard_report_t));

    logitacker_devices_generate_keyboard_frame(self->p_device, p_out_payload, p_hid_report);

    NRF_LOG_INFO("Updated TX payload (%d):", rc++);
    NRF_LOG_HEXDUMP_INFO(p_out_payload->data, p_out_payload->length);

}

bool provider_press_inject_get_next(logitacker_tx_payload_provider_press_ctx_t * self, nrf_esb_payload_t *p_next_payload) {
    if (self->key_combo_transmitted) {
        if (self->append_release) {
            if (self->release_transmitted) {
                return false; // job done
            } else {
                // TX of release report needed
                convert_hid_report_to_rf_payload(self, p_next_payload, &release_report);
                self->release_transmitted = true;
                return true;
            }
        } else {
            // no need to TX key release
            return false; // job done
        }
    } else {
        // key combo report needed
        convert_hid_report_to_rf_payload(self, p_next_payload, &self->combo_hid_report);
        self->key_combo_transmitted = true;
        return true;
    }
}


void provider_press_inject_reset(logitacker_tx_payload_provider_press_ctx_t * self) {
    self->append_release = true;
    self->key_combo_transmitted = false;
    m_local_ctx.release_transmitted = false;
}

logitacker_tx_payload_provider_t * new_payload_provider_press(logitacker_devices_unifying_device_t * p_device, logitacker_keyboard_map_lang_t lang, char const * const str) {
    if (p_device == NULL) {
        NRF_LOG_WARNING("cannot create payload provider 'press', no device given");
    }

    // no real instance, as a static object is used (has to be malloced)
    m_local_provider.p_get_next = provider_press_get_next;
    m_local_provider.p_reset = provider_press_reset;

    // again no real instance

    m_local_ctx.p_device = p_device;
    m_local_ctx.language_layout = lang;
    m_local_ctx.source_string = str;

    m_local_ctx.key_combo_transmitted = false;
    m_local_ctx.release_transmitted = false;
    m_local_ctx.append_release = true; //append key release reports
    m_local_provider.p_ctx = &m_local_ctx;

    // pre fill hid report with resolved combo string
    logitacker_keyboard_map_combo_str_to_hid_report(str, &m_local_ctx.combo_hid_report, lang);

    return &m_local_provider;
}
