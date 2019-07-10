#include "logitacker_processor_pair_device.h"

#include "nrf.h"
#include "logitacker.h"
#include "logitacker_processor.h"
#include "helper.h"
#include "string.h"
#include "logitacker_devices.h"
#include "logitacker_pairing_parser.h"
#include "logitacker_unifying.h"

#define NRF_LOG_MODULE_NAME LOGITACKER_PROCESSOR_PAIR_DEVICE
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

// ToDo: implement error conditions leading to failed pairing

#define PAIR_DEVICE_TX_DELAY_MS 8 //delay in ms between successful transmits
#define PAIR_DEVICE_MARKER_BYTE_PHASE1 0xe1 // byte used as device ID in pairing requests
#define PAIR_DEVICE_MARKER_BYTE_PHASE2 0xe2 // byte used as device ID in pairing requests
#define PAIR_DEVICE_MARKER_BYTE_PHASE3 0xe3 // byte used as device ID in pairing requests

static const uint8_t pseudo_device_address[5] = { 0xde, 0xad, 0xbe, 0xef, 0x82};
static char addr_str_buff[LOGITACKER_DEVICE_ADDR_STR_LEN] = {0};
static uint8_t tmp_addr[LOGITACKER_DEVICE_ADDR_LEN] = {0};

#define PAIR_DEVICE_RETRANSMIT_BEFORE_FAIL 10

typedef enum {
    PAIR_DEVICE_PHASE_START,
    PAIR_DEVICE_PHASE_REQUEST1_SENT,
    PAIR_DEVICE_PHASE_REQUEST1_ACKED,
    PAIR_DEVICE_PHASE_RESPONSE1_PULLED,
    PAIR_DEVICE_PHASE_RESPONSE1_RECEIVED,
    PAIR_DEVICE_PHASE_1_INFORM_ADDRESS_ACCEPTED_SENT,
    PAIR_DEVICE_PHASE_1_INFORM_ADDRESS_ACCEPTED_ACKED,

    PAIR_DEVICE_PHASE_REQUEST2_SENT,
    PAIR_DEVICE_PHASE_REQUEST2_ACKED,
    PAIR_DEVICE_PHASE_RESPONSE2_PULLED,
    PAIR_DEVICE_PHASE_RESPONSE2_RECEIVED,

    PAIR_DEVICE_PHASE_REQUEST3_SENT,
    PAIR_DEVICE_PHASE_REQUEST3_ACKED,
    PAIR_DEVICE_PHASE_FINAL_RESPONSE_PULLED,
    PAIR_DEVICE_PHASE_FINAL_RESPONSE_RECEIVED,

    PAIR_DEVICE_PHASE_FINAL_REQUEST_SENT,
    //PAIR_DEVICE_PHASE_FINAL_REQUEST_ACKED,


    PAIR_DEVICE_PHASE_SUCCEEDED,
    PAIR_DEVICE_PHASE_FAILED,
} pair_device_phase_t;



typedef struct {
    logitacker_mode_t * p_logitacker_mainstate;

    uint8_t current_rf_address[5];

    uint8_t base_addr[4];
    uint8_t prefix;

    uint8_t tx_delay_ms;

    bool receiver_in_range;
    pair_device_phase_t phase;
    app_timer_id_t timer_next_action;

    logitacker_pairing_info_t device_pairing_info;

    nrf_esb_payload_t tmp_tx_payload;
    nrf_esb_payload_t tmp_rx_payload;

    int retransmit_counter;
} logitacker_processor_pair_device_ctx_t;

static logitacker_processor_t m_processor = {0};
static logitacker_processor_pair_device_ctx_t m_static_pair_device_ctx; //we reuse the same context, alternatively an malloc'ed ctx would allow separate instances

void processor_pair_device_init_func(logitacker_processor_t *p_processor);
void processor_pair_device_init_func_(logitacker_processor_pair_device_ctx_t *self);

void processor_pair_device_deinit_func(logitacker_processor_t *p_processor);
void processor_pair_device_deinit_func_(logitacker_processor_pair_device_ctx_t *self);

void processor_pair_device_esb_handler_func(logitacker_processor_t *p_processor, nrf_esb_evt_t *p_esb_evt);
void processor_pair_device_esb_handler_func_(logitacker_processor_pair_device_ctx_t *self, nrf_esb_evt_t *p_esb_event);

void processor_pair_device_timer_handler_func(logitacker_processor_t *p_processor, void *p_timer_ctx);
void processor_pair_device_timer_handler_func_(logitacker_processor_pair_device_ctx_t *self, void *p_timer_ctx);

void processor_pair_device_bsp_handler_func(logitacker_processor_t *p_processor, bsp_event_t event);
void processor_pair_device_bsp_handler_func_(logitacker_processor_pair_device_ctx_t *self, bsp_event_t event);


void processor_pair_device_create_req1_pay(logitacker_processor_pair_device_ctx_t *self);
void processor_pair_device_create_req1_pull_pay(logitacker_processor_pair_device_ctx_t *self);
bool processor_pair_device_parse_rsp1_pay(logitacker_processor_pair_device_ctx_t *self);
void processor_pair_device_create_phase1_address_accepted_pay(logitacker_processor_pair_device_ctx_t *self);
void processor_pair_device_create_req2_pay(logitacker_processor_pair_device_ctx_t *self);
void processor_pair_device_create_req2_pull_pay(logitacker_processor_pair_device_ctx_t *self);
bool processor_pair_device_parse_rsp2_pay(logitacker_processor_pair_device_ctx_t *self);
void processor_pair_device_create_req3_pay(logitacker_processor_pair_device_ctx_t *self);
void processor_pair_device_create_req3_pull_pay(logitacker_processor_pair_device_ctx_t *self);
bool processor_pair_device_parse_rsp3_pay(logitacker_processor_pair_device_ctx_t *self);
void processor_pair_device_create_final_req_pay(logitacker_processor_pair_device_ctx_t *self);

bool processor_pair_device_validate_rx_payload(logitacker_processor_pair_device_ctx_t *self);
void processor_pair_device_update_tx_payload_and_transmit(logitacker_processor_pair_device_ctx_t *self);



logitacker_processor_t * contruct_processor_pair_device_instance(logitacker_processor_pair_device_ctx_t *const pair_device_ctx) {
    m_processor.p_ctx = pair_device_ctx;
    m_processor.p_init_func = processor_pair_device_init_func;
    m_processor.p_deinit_func = processor_pair_device_deinit_func;
    m_processor.p_esb_handler = processor_pair_device_esb_handler_func;
    m_processor.p_timer_handler = processor_pair_device_timer_handler_func;
    m_processor.p_bsp_handler = processor_pair_device_bsp_handler_func;

    return &m_processor;
}

void processor_pair_device_bsp_handler_func(logitacker_processor_t *p_processor, bsp_event_t event) {
    processor_pair_device_bsp_handler_func_((logitacker_processor_pair_device_ctx_t *) p_processor->p_ctx, event);
}

void processor_pair_device_bsp_handler_func_(logitacker_processor_pair_device_ctx_t *self, bsp_event_t event) {

}

void processor_pair_device_init_func(logitacker_processor_t *p_processor) {
    processor_pair_device_init_func_((logitacker_processor_pair_device_ctx_t *) p_processor->p_ctx);
}

void processor_pair_device_init_func_(logitacker_processor_pair_device_ctx_t *self) {
    *self->p_logitacker_mainstate = LOGITACKER_MODE_PAIR_DEVICE;
    self->tx_delay_ms = PAIR_DEVICE_TX_DELAY_MS;

    helper_addr_to_base_and_prefix(self->base_addr, &self->prefix, self->current_rf_address, LOGITACKER_DEVICE_ADDR_LEN);

    helper_addr_to_hex_str(addr_str_buff, LOGITACKER_DEVICE_ADDR_LEN, self->current_rf_address);
    NRF_LOG_INFO("Try to pair new device on target address %s", addr_str_buff);

    radio_disable_rx_timeout_event(); // disable RX timeouts
    radio_stop_channel_hopping(); // disable channel hopping
    nrf_esb_stop_rx(); //stop rx in case running

    // set current address for pipe 1
    nrf_esb_enable_pipes(0x00); //disable all pipes
    nrf_esb_set_base_address_0(self->base_addr); // set base addr 0
    nrf_esb_update_prefix(0, self->prefix); // set prefix and enable pipe 0
    nrf_esb_enable_pipes(0x01); //enable pipe 0


    // clear TX/RX payload buffers (just to be sure)
    memset(&self->tmp_tx_payload, 0, sizeof(self->tmp_tx_payload)); //unset TX payload
    memset(&self->tmp_rx_payload, 0, sizeof(self->tmp_rx_payload)); //unset RX payload

    // prepare test TX payload (first report will be a pairing request)
    self->tmp_tx_payload.noack = false;
    self->phase = PAIR_DEVICE_PHASE_START;

    self->retransmit_counter = 0;

    // setup radio as PTX
    nrf_esb_set_mode(NRF_ESB_MODE_PTX);
    nrf_esb_enable_all_channel_tx_failover(true); //retransmit payloads on all channels if transmission fails
    nrf_esb_set_all_channel_tx_failover_loop_count(2); //iterate over channels two time before failing
    nrf_esb_set_retransmit_count(1);
    nrf_esb_set_retransmit_delay(250);
    nrf_esb_set_tx_power(NRF_ESB_TX_POWER_8DBM);


    processor_pair_device_update_tx_payload_and_transmit(self);

}

void processor_pair_device_deinit_func(logitacker_processor_t *p_processor) {
    processor_pair_device_deinit_func_((logitacker_processor_pair_device_ctx_t *) p_processor->p_ctx);
}

void processor_pair_device_deinit_func_(logitacker_processor_pair_device_ctx_t *self) {
    *self->p_logitacker_mainstate = LOGITACKER_MODE_IDLE;

    NRF_LOG_INFO("DEINIT active enumeration for address %s", addr_str_buff);

    radio_disable_rx_timeout_event(); // disable RX timeouts
    radio_stop_channel_hopping(); // disable channel hopping
    nrf_esb_stop_rx(); //stop rx in case running

    nrf_esb_set_mode(NRF_ESB_MODE_PROMISCOUS); //should disable and end up in idle state

    // set current address for pipe 1
    nrf_esb_enable_pipes(0x00); //disable all pipes

    // reset inner loop count
    self->prefix = 0x00; //unset prefix
//    self->phase = PAIR_DEVICE_PHASE_SUCCEEDED;
    memset(self->base_addr, 0, 4); //unset base address
    memset(self->current_rf_address, 0, 5); //unset RF address

    memset(&self->tmp_tx_payload, 0, sizeof(self->tmp_tx_payload)); //unset TX payload
    memset(&self->tmp_rx_payload, 0, sizeof(self->tmp_rx_payload)); //unset RX payload

    self->phase = PAIR_DEVICE_PHASE_START;
    self->retransmit_counter = 0;

    nrf_esb_enable_all_channel_tx_failover(false); // disable all channel failover
}

void processor_pair_device_timer_handler_func(logitacker_processor_t *p_processor, void *p_timer_ctx) {
    processor_pair_device_timer_handler_func_((logitacker_processor_pair_device_ctx_t *) p_processor->p_ctx, p_timer_ctx);
}

void processor_pair_device_timer_handler_func_(logitacker_processor_pair_device_ctx_t *self, void *p_timer_ctx) {
    // if timer is called, write (and auto transmit) current ESB payload
    logitacker_unifying_payload_update_checksum(self->tmp_tx_payload.data, self->tmp_tx_payload.length);

    if (nrf_esb_write_payload(&self->tmp_tx_payload) != NRF_SUCCESS) {
        NRF_LOG_INFO("Error writing payload");
    } else {
        nrf_esb_convert_pipe_to_address(self->tmp_tx_payload.pipe, tmp_addr);
        helper_addr_to_hex_str(addr_str_buff, 5, tmp_addr);
        NRF_LOG_INFO("TX'ed to %s", nrf_log_push(addr_str_buff));
    }
}

void processor_pair_device_esb_handler_func(logitacker_processor_t *p_processor, nrf_esb_evt_t *p_esb_evt) {
    processor_pair_device_esb_handler_func_((logitacker_processor_pair_device_ctx_t *) p_processor->p_ctx, p_esb_evt);
}

void processor_pair_device_esb_handler_func_(logitacker_processor_pair_device_ctx_t *self, nrf_esb_evt_t *p_esb_event) {

/*
    uint32_t channel_freq;
    nrf_esb_get_rf_frequency(&channel_freq);
*/
    if (self->retransmit_counter >= PAIR_DEVICE_RETRANSMIT_BEFORE_FAIL) {
        self->phase = PAIR_DEVICE_PHASE_FAILED;
    }

    if (self->phase == PAIR_DEVICE_PHASE_FAILED) {
        NRF_LOG_WARNING("Device pairing failed, switching mode to discover");
        logitacker_enter_mode_discovery();
        return;
    }

    switch (p_esb_event->evt_id) {
        case NRF_ESB_EVENT_TX_FAILED:
        {
            //re-transmit last frame (payload still enqued)
            nrf_esb_start_tx();

            break;
        }
        case NRF_ESB_EVENT_TX_SUCCESS_ACK_PAY:
            self->tmp_rx_payload.length = 0; //clear RX payload before fetching new one
            while (nrf_esb_read_rx_payload(&self->tmp_rx_payload) == NRF_SUCCESS) {
                processor_pair_device_validate_rx_payload(self);
            }
            processor_pair_device_update_tx_payload_and_transmit(self);
            break;
        case NRF_ESB_EVENT_TX_SUCCESS:
        {
            self->tmp_rx_payload.length = 0; //clear RX payload before fetching new one
            processor_pair_device_validate_rx_payload(self); // indicate that empty ack was received
            processor_pair_device_update_tx_payload_and_transmit(self);
            break;
        }
        case NRF_ESB_EVENT_RX_RECEIVED:
        {
            NRF_LOG_ERROR("ESB EVENT HANDLER PAIR DEVICE RX_RECEIVED ... !!shouldn't happen!!");
            break;
        }
    }


    if (self->phase == PAIR_DEVICE_PHASE_SUCCEEDED) {
        NRF_LOG_WARNING("Device pairing succeeded, switching mode to discover");
        logitacker_enter_mode_discovery();
        return;
    }

    return;
}



void processor_pair_device_create_req1_pay(logitacker_processor_pair_device_ctx_t *self) {
    self->tmp_tx_payload.length = 22;
    memset(self->tmp_tx_payload.data, 0, self->tmp_tx_payload.length);
    self->tmp_tx_payload.data[0] = PAIR_DEVICE_MARKER_BYTE_PHASE1; //seq ID during this phase
    self->tmp_tx_payload.data[1] = 0x1f | 0x40; //Report type: pairing with keep-alive
    self->tmp_tx_payload.data[2] = 0x01; //Pairing phase: 1
    memcpy(&self->tmp_tx_payload.data[3], pseudo_device_address, 5); // 3..7 pseudo device's current RF address
    self->tmp_tx_payload.data[8] = 0x08; //likely default keep-alive interval
    memcpy(&self->tmp_tx_payload.data[9], self->device_pairing_info.device_wpid, 2); //WPID of device
    self->tmp_tx_payload.data[11] = LOGITACKER_DEVICE_PROTOCOL_UNIFYING; //likely protocol (0x04 == Unifying ?)
    self->tmp_tx_payload.data[12] = 0x00; //unknown 0x00 for some devices (mouse, keyboard), 0x02 for others (mouse Anywhere MX 2, presenter)
    self->tmp_tx_payload.data[13] = self->device_pairing_info.device_type;
    self->tmp_tx_payload.data[14] = self->device_pairing_info.device_caps; // should have LOGITACKER_DEVICE_CAPS_UNIFYING_COMPATIBLE set and LOGITACKER_DEVICE_CAPS_LINK_ENCRYPTION unset
    //15 is unknown

    self->tmp_tx_payload.pipe = 0; // first request is sent on pipe 0 (dongle pairing address)

    // update checksum
    logitacker_unifying_payload_update_checksum(self->tmp_tx_payload.data, self->tmp_tx_payload.length);
}

void processor_pair_device_create_req1_pull_pay(logitacker_processor_pair_device_ctx_t *self) {
    self->tmp_tx_payload.length = 5;
    memset(self->tmp_tx_payload.data, 0, self->tmp_tx_payload.length);
    self->tmp_tx_payload.data[0] = PAIR_DEVICE_MARKER_BYTE_PHASE1; //seq ID during this phase
    self->tmp_tx_payload.data[1] = 0x40; //Report type: keep-alive
    self->tmp_tx_payload.data[2] = 0x01; //Pairing phase: 1 (not keep alive in this case)
    self->tmp_tx_payload.data[3] = pseudo_device_address[0]; // first byte of device's current RF address

    self->tmp_tx_payload.pipe = 0; // first request is sent on pipe 0 (dongle pairing address)

    // update checksum
    logitacker_unifying_payload_update_checksum(self->tmp_tx_payload.data, self->tmp_tx_payload.length);
}

bool processor_pair_device_parse_rsp1_pay(logitacker_processor_pair_device_ctx_t *self) {
    if (self->tmp_rx_payload.length != 22) return false; //wrong length
    if (self->tmp_rx_payload.data[0] != PAIR_DEVICE_MARKER_BYTE_PHASE1) return false; //wrong seq ID for pairing phase
    if (self->tmp_rx_payload.data[1] != 0x1f) return false; //no pairing report
    if (self->tmp_rx_payload.data[2] != 0x01) return false; //no pairing phase1 response
    memcpy(self->device_pairing_info.device_rf_address, &self->tmp_rx_payload.data[3], 5); //offered RF address
    helper_addr_to_base_and_prefix(self->device_pairing_info.base_addr, &self->device_pairing_info.device_prefix, self->device_pairing_info.device_rf_address, 5); //translate to base addr and prefix

    // activate additional address on pipe 1
    nrf_esb_set_base_address_1(self->device_pairing_info.base_addr);
    nrf_esb_update_prefix(1, self->device_pairing_info.device_prefix);
    nrf_esb_enable_pipes(0x03); //keep pairing address enabled, additionally enable offered address

    // byte 8 likely keep alive interval

    memcpy(self->device_pairing_info.dongle_wpid, &self->tmp_rx_payload.data[9], 2); //offered RF address
    //byte 11 likely proto Unifying
    //byte 12 ??
    //byte 13 should be device type from pairing request 1, but we don't check
    //byte 14 should be caps field from pairing request 1, but we don't check

    return true;
}

void processor_pair_device_create_phase1_address_accepted_pay(logitacker_processor_pair_device_ctx_t *self) {
    self->tmp_tx_payload.length = 5;
    memset(self->tmp_tx_payload.data, 0, self->tmp_tx_payload.length);
    self->tmp_tx_payload.data[0] = PAIR_DEVICE_MARKER_BYTE_PHASE1; //seq ID during this phase
    self->tmp_tx_payload.data[1] = 0x40; //Report type: keep-alive
    self->tmp_tx_payload.data[2] = 0x01; //Pairing phase: 1 (not keep alive in this case)
    self->tmp_tx_payload.data[3] = pseudo_device_address[0]; // first byte of !old! RF address

    self->tmp_tx_payload.pipe = 0; // still sent on old address (pipe 0), while already listening on new one (pipe 1)

    // update checksum
    logitacker_unifying_payload_update_checksum(self->tmp_tx_payload.data, self->tmp_tx_payload.length);
}


void processor_pair_device_create_req2_pay(logitacker_processor_pair_device_ctx_t *self) {
    self->tmp_tx_payload.length = 22;
    memset(self->tmp_tx_payload.data, 0, self->tmp_tx_payload.length);
    self->tmp_tx_payload.data[0] = PAIR_DEVICE_MARKER_BYTE_PHASE2; //seq ID during this phase
    self->tmp_tx_payload.data[1] = 0x1f | 0x40; //Report type: pairing with keep-alive
    self->tmp_tx_payload.data[2] = 0x02; //Pairing phase: 2
    memcpy(&self->tmp_tx_payload.data[3], self->device_pairing_info.device_nonce, 4); // 3..6 device nonce used for key gen (could all be 0x00 if no link encryption)
    memcpy(&self->tmp_tx_payload.data[7], self->device_pairing_info.device_serial, 4); // 7..10 device serial, used to distinguish new device from re-paired device (unique per device)

    // bytes 11..14 are supported report types in little endian
    self->tmp_tx_payload.data[11] = (self->device_pairing_info.device_report_types >> 0) & 0xFF; //LSB
    self->tmp_tx_payload.data[12] = (self->device_pairing_info.device_report_types >> 8) & 0xFF;
    self->tmp_tx_payload.data[13] = (self->device_pairing_info.device_report_types >> 16) & 0xFF;
    self->tmp_tx_payload.data[14] = (self->device_pairing_info.device_report_types >> 24) & 0xFF; //MSB

    // usability info (where is the power switch residing)
    self->tmp_tx_payload.data[15] = self->device_pairing_info.device_usability_info;

    self->tmp_tx_payload.pipe = 1; // request is sent on pipe 1 (offered address pairing address)

    // update checksum
    logitacker_unifying_payload_update_checksum(self->tmp_tx_payload.data, self->tmp_tx_payload.length);
}

void processor_pair_device_create_req2_pull_pay(logitacker_processor_pair_device_ctx_t *self) {
    self->tmp_tx_payload.length = 5;
    memset(self->tmp_tx_payload.data, 0, self->tmp_tx_payload.length);
    self->tmp_tx_payload.data[0] = PAIR_DEVICE_MARKER_BYTE_PHASE2; //seq ID during this phase
    self->tmp_tx_payload.data[1] = 0x40; //Report type: keep-alive
    self->tmp_tx_payload.data[2] = 0x02; //Pairing phase: 2 (not keep alive in this case)
    self->tmp_tx_payload.data[3] = self->device_pairing_info.device_rf_address[0];

    self->tmp_tx_payload.pipe = 1; // first request is sent on pipe 0 (dongle pairing address)

    // update checksum
    logitacker_unifying_payload_update_checksum(self->tmp_tx_payload.data, self->tmp_tx_payload.length);
}

bool processor_pair_device_parse_rsp2_pay(logitacker_processor_pair_device_ctx_t *self) {
    if (self->tmp_rx_payload.length != 22) return false; //wrong length
    if (self->tmp_rx_payload.data[0] != PAIR_DEVICE_MARKER_BYTE_PHASE2) return false; //wrong seq ID for pairing phase
    if (self->tmp_rx_payload.data[1] != 0x1f) return false; //no pairing report
    if (self->tmp_rx_payload.data[2] != 0x02) return false; //no pairing phase2 response
    if (self->tmp_rx_payload.pipe != 1) return false; //wrong pipe

    memcpy(self->device_pairing_info.dongle_nonce, &self->tmp_rx_payload.data[3], 4); //nonce from dongle

    // byte 7 onwards should be copy of request phase 2 (ignored)

    return true;
}

void processor_pair_device_create_req3_pay(logitacker_processor_pair_device_ctx_t *self) {
    self->tmp_tx_payload.length = 22;
    memset(self->tmp_tx_payload.data, 0, self->tmp_tx_payload.length);
    self->tmp_tx_payload.data[0] = PAIR_DEVICE_MARKER_BYTE_PHASE3; //seq ID during this phase
    self->tmp_tx_payload.data[1] = 0x1f | 0x40; //Report type: pairing with keep-alive
    self->tmp_tx_payload.data[2] = 0x03; //Pairing phase: 3
    self->tmp_tx_payload.data[3] = 0x01; // likely how many reports are needed to TX the full device name, fix to 1
    self->tmp_tx_payload.data[4] = self->device_pairing_info.device_name_len > 16 ? 16 : self->device_pairing_info.device_name_len; //clamp name length to 16
    memcpy(&self->tmp_tx_payload.data[5], self->device_pairing_info.device_name, self->device_pairing_info.device_name_len); //put in name

    self->tmp_tx_payload.pipe = 1; // request is sent on pipe 1 (offered address pairing address)

    // update checksum
    logitacker_unifying_payload_update_checksum(self->tmp_tx_payload.data, self->tmp_tx_payload.length);
}

void processor_pair_device_create_req3_pull_pay(logitacker_processor_pair_device_ctx_t *self) {
    self->tmp_tx_payload.length = 5;
    memset(self->tmp_tx_payload.data, 0, self->tmp_tx_payload.length);
    self->tmp_tx_payload.data[0] = PAIR_DEVICE_MARKER_BYTE_PHASE3; //seq ID during this phase
    self->tmp_tx_payload.data[1] = 0x40; //Report type: keep-alive
    self->tmp_tx_payload.data[2] = 0x03; //Pairing phase: 2 (not keep alive in this case)
    self->tmp_tx_payload.data[3] = 0x01; //same as byte 3 of request

    self->tmp_tx_payload.pipe = 1; // first request is sent on pipe 0 (dongle pairing address)

    // update and checksum
    logitacker_unifying_payload_update_checksum(self->tmp_tx_payload.data, self->tmp_tx_payload.length);
}

bool processor_pair_device_parse_rsp3_pay(logitacker_processor_pair_device_ctx_t *self) {
    if (self->tmp_rx_payload.length != 10) return false; //wrong length
    if (self->tmp_rx_payload.data[0] != PAIR_DEVICE_MARKER_BYTE_PHASE3) return false; //wrong seq ID for pairing phase
    if (self->tmp_rx_payload.data[1] != 0x0f) return false; //final pairing report (same as set-keep-alive without keep-alive bit)
    if (self->tmp_rx_payload.pipe != 1) return false; //wrong pipe

    // byte 2 should be 0x06, but we don't check
    // byte 3 should be 0x02, but we don't check
    // byte 4 should be 0x03, but we don't check
    // byte 5..6, last two bytes of dongle nonce
    // byte 7..8, first two bytes of device serial

    return true;
}

void processor_pair_device_create_final_req_pay(logitacker_processor_pair_device_ctx_t *self) {
    self->tmp_tx_payload.length = 10;
    memset(self->tmp_tx_payload.data, 0, self->tmp_tx_payload.length);
    self->tmp_tx_payload.data[0] = PAIR_DEVICE_MARKER_BYTE_PHASE2; //seq ID during this phase
    self->tmp_tx_payload.data[1] = 0x0f | 0x40; //Report type: final pairing with keep-alive (same as set keep alive)
    self->tmp_tx_payload.data[2] = 0x06;
    self->tmp_tx_payload.data[3] = 0x01;

    self->tmp_tx_payload.pipe = 1; // request is sent on pipe 1 (offered address pairing address)

    // update checksum
    logitacker_unifying_payload_update_checksum(self->tmp_tx_payload.data, self->tmp_tx_payload.length);
}

char tmp_str[10] = {0};
// must only be called on TX_SUCCESS or TX_SUCCESS_ACK_PAY
bool processor_pair_device_validate_rx_payload(logitacker_processor_pair_device_ctx_t *self) {
/*
 *    PAIR_DEVICE_PHASE_START, --> req1 pay
    PAIR_DEVICE_PHASE_REQUEST1_SENT, <-- empty ack,
    PAIR_DEVICE_PHASE_REQUEST1_ACKED, --> resp 1 pull
    PAIR_DEVICE_PHASE_RESPONSE1_PULLED, <-- move on to received
    PAIR_DEVICE_PHASE_RESPONSE1_RECEIVED, --> confirm address update
    PAIR_DEVICE_PHASE_1_INFORM_ADDRESS_ACCEPTED_SENT,
    PAIR_DEVICE_PHASE_1_INFORM_ADDRESS_ACCEPTED_ACKED, --> req 2

    PAIR_DEVICE_PHASE_REQUEST2_SENT,
    PAIR_DEVICE_PHASE_REQUEST2_ACKED, --> resp 2 pull
    PAIR_DEVICE_PHASE_RESPONSE2_PULLED,
    PAIR_DEVICE_PHASE_RESPONSE2_RECEIVED, --> req 3

    PAIR_DEVICE_PHASE_REQUEST3_SENT,
    PAIR_DEVICE_PHASE_REQUEST3_ACKED, --> resp 3 pull
    PAIR_DEVICE_PHASE_FINAL_RESPONSE_PULLED,
    PAIR_DEVICE_PHASE_FINAL_RESPONSE_RECEIVED, --> final request

    PAIR_DEVICE_PHASE_FINAL_REQUEST_SENT,
    PAIR_DEVICE_PHASE_FINAL_REQUEST_ACKED, --> finished


    PAIR_DEVICE_PHASE_SUCCEEDED,
    PAIR_DEVICE_PHASE_FAILED,
 */

    sprintf(tmp_str, "%d", self->phase);
    NRF_LOG_INFO("Phase before RX: %s", nrf_log_push(tmp_str));
    NRF_LOG_HEXDUMP_INFO(self->tmp_rx_payload.data, self->tmp_rx_payload.length);

    switch (self->phase) {
        case PAIR_DEVICE_PHASE_REQUEST1_SENT:
            self->phase = PAIR_DEVICE_PHASE_REQUEST1_ACKED; // we had TX_SUCCESS when this method is called
            goto success;
        case PAIR_DEVICE_PHASE_RESPONSE1_PULLED:
            if (processor_pair_device_parse_rsp1_pay(self)) {
                self->phase = PAIR_DEVICE_PHASE_RESPONSE1_RECEIVED;
                //self->phase = PAIR_DEVICE_PHASE_1_INFORM_ADDRESS_ACCEPTED_ACKED; //skip a step
                goto success;
            } else {
                break;
            }
        case PAIR_DEVICE_PHASE_1_INFORM_ADDRESS_ACCEPTED_SENT:
            if (self->tmp_rx_payload.length == 0)
                self->phase = PAIR_DEVICE_PHASE_1_INFORM_ADDRESS_ACCEPTED_ACKED; // we had TX_SUCCESS when this method is called
            goto success;
        case PAIR_DEVICE_PHASE_REQUEST2_SENT:
            self->phase = PAIR_DEVICE_PHASE_REQUEST2_ACKED; // we had TX_SUCCESS when this method is called
            goto success;
        case PAIR_DEVICE_PHASE_RESPONSE2_PULLED:
            if (processor_pair_device_parse_rsp2_pay(self)) {
                self->phase = PAIR_DEVICE_PHASE_RESPONSE2_RECEIVED;
                goto success;
            } else {
                break;
            }
        case PAIR_DEVICE_PHASE_REQUEST3_SENT:
            self->phase = PAIR_DEVICE_PHASE_REQUEST3_ACKED; // we had TX_SUCCESS when this method is called
            return true;
        case PAIR_DEVICE_PHASE_FINAL_RESPONSE_PULLED:
            if (processor_pair_device_parse_rsp3_pay(self)) {
                self->phase = PAIR_DEVICE_PHASE_FINAL_RESPONSE_RECEIVED;
                goto success;
            } else {
                break;
            }
        case PAIR_DEVICE_PHASE_FINAL_REQUEST_SENT:
        //    self->phase = PAIR_DEVICE_PHASE_FINAL_REQUEST_ACKED; // we had TX_SUCCESS when this method is called
            self->phase = PAIR_DEVICE_PHASE_SUCCEEDED; // we had TX_SUCCESS when this method is called
            goto success;

        default:
            break;
    }



    // increment errors could be placed here, as abort condition
    return false;

success:
    sprintf(tmp_str, "%d", self->phase);
    NRF_LOG_INFO("RX phase after parsing: %s", nrf_log_push(tmp_str));
    return true;
}

void processor_pair_device_update_tx_payload_and_transmit(logitacker_processor_pair_device_ctx_t *self) {
/*
 *    PAIR_DEVICE_PHASE_START, --> req1 pay
    PAIR_DEVICE_PHASE_REQUEST1_SENT,
    PAIR_DEVICE_PHASE_REQUEST1_ACKED, --> resp 1 pull
    PAIR_DEVICE_PHASE_RESPONSE1_PULLED,
    PAIR_DEVICE_PHASE_RESPONSE1_RECEIVED, --> confirm address update
    PAIR_DEVICE_PHASE_1_INFORM_ADDRESS_ACCEPTED_SENT,
    PAIR_DEVICE_PHASE_1_INFORM_ADDRESS_ACCEPTED_ACKED, --> req 2

    PAIR_DEVICE_PHASE_REQUEST2_SENT,
    PAIR_DEVICE_PHASE_REQUEST2_ACKED, --> resp 2 pull
    PAIR_DEVICE_PHASE_RESPONSE2_PULLED,
    PAIR_DEVICE_PHASE_RESPONSE2_RECEIVED, --> req 3

    PAIR_DEVICE_PHASE_REQUEST3_SENT,
    PAIR_DEVICE_PHASE_REQUEST3_ACKED, --> resp 3 pull
    PAIR_DEVICE_PHASE_FINAL_RESPONSE_PULLED,
    PAIR_DEVICE_PHASE_FINAL_RESPONSE_RECEIVED, --> final request

    PAIR_DEVICE_PHASE_FINAL_REQUEST_SENT,
    PAIR_DEVICE_PHASE_FINAL_REQUEST_ACKED, --> finished


    PAIR_DEVICE_PHASE_SUCCEEDED,
    PAIR_DEVICE_PHASE_FAILED,
 */

    pair_device_phase_t old_phase = self->phase;
    sprintf(tmp_str, "%d", self->phase);
    NRF_LOG_INFO("phase before TX: %s", nrf_log_push(tmp_str));



    switch (self->phase) {
        case PAIR_DEVICE_PHASE_START:
            // prepare pairing request 1 payload
            processor_pair_device_create_req1_pay(self);
            self->phase = PAIR_DEVICE_PHASE_REQUEST1_SENT; // state is valid after TX has been started, which should happen immediately
            break;
        case PAIR_DEVICE_PHASE_REQUEST1_ACKED:
            processor_pair_device_create_req1_pull_pay(self);
            self->phase = PAIR_DEVICE_PHASE_RESPONSE1_PULLED; // state is valid after TX has been started, which should happen immediately
            break;
        case PAIR_DEVICE_PHASE_RESPONSE1_RECEIVED:
            processor_pair_device_create_phase1_address_accepted_pay(self); //<-- enables pipe 1 to listen on new address
            self->phase = PAIR_DEVICE_PHASE_1_INFORM_ADDRESS_ACCEPTED_SENT; // state is valid after TX has been started, which should happen immediately
            break;
        case PAIR_DEVICE_PHASE_1_INFORM_ADDRESS_ACCEPTED_ACKED:
            processor_pair_device_create_req2_pay(self);
            self->phase = PAIR_DEVICE_PHASE_REQUEST2_SENT; // state is valid after TX has been started, which should happen immediately
            break;
        case PAIR_DEVICE_PHASE_REQUEST2_ACKED:
            processor_pair_device_create_req2_pull_pay(self);
            self->phase = PAIR_DEVICE_PHASE_RESPONSE2_PULLED; // state is valid after TX has been started, which should happen immediately
            break;
        case PAIR_DEVICE_PHASE_RESPONSE2_RECEIVED:
            processor_pair_device_create_req3_pay(self);
            self->phase = PAIR_DEVICE_PHASE_REQUEST3_SENT; // state is valid after TX has been started, which should happen immediately
            break;
        case PAIR_DEVICE_PHASE_REQUEST3_ACKED:
            processor_pair_device_create_req3_pull_pay(self);
            self->phase = PAIR_DEVICE_PHASE_FINAL_RESPONSE_PULLED; // state is valid after TX has been started, which should happen immediately
            break;
        case PAIR_DEVICE_PHASE_FINAL_RESPONSE_RECEIVED:
            processor_pair_device_create_final_req_pay(self);
            self->phase = PAIR_DEVICE_PHASE_FINAL_REQUEST_SENT; // state is valid after TX has been started, which should happen immediately
            break;
        case PAIR_DEVICE_PHASE_SUCCEEDED:
            // shouldn't be called here, as pairing is finished
            NRF_LOG_WARNING("Called TX payload update, while pairing already finished");
            break;
        default:
            NRF_LOG_WARNING("update TX payload called for unknown pairing phase: %d", self->phase);
    }

    if (old_phase == self->phase) {
        self->retransmit_counter++;
    } else {
        self->retransmit_counter = 0;
    }

    self->tmp_tx_payload.noack = false; // we need an ack

    // schedule payload for sending
//    app_timer_start(self->timer_next_action, APP_TIMER_TICKS(PAIR_DEVICE_TX_DELAY_MS), NULL);
    app_timer_start(self->timer_next_action, APP_TIMER_TICKS(self->tx_delay_ms), NULL);

    sprintf(tmp_str, "%d", self->phase);
    NRF_LOG_INFO("phase after TX: %s", nrf_log_push(tmp_str));
    NRF_LOG_HEXDUMP_INFO(self->tmp_tx_payload.data, self->tmp_tx_payload.length);
}

logitacker_processor_t * new_processor_pair_device(uint8_t const *target_rf_address, logitacker_pairing_info_t const * pairing_info, app_timer_id_t timer_next_action) {

    // initialize context (static in this case, has to use malloc for new instances)
    logitacker_processor_pair_device_ctx_t *const p_ctx = &m_static_pair_device_ctx;
    memset(p_ctx, 0, sizeof(*p_ctx)); //replace with malloc for dedicated instance

    //initialize member variables
    memcpy(p_ctx->current_rf_address, target_rf_address, 5);
    memcpy(&p_ctx->device_pairing_info, pairing_info, sizeof(p_ctx->device_pairing_info));
    p_ctx->timer_next_action = timer_next_action;


    return contruct_processor_pair_device_instance(&m_static_pair_device_ctx);
}