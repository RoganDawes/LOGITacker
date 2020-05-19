#include "logitacker_processor_prx.h"

#include "logitacker.h"
#include "logitacker_unifying.h"
#include "logitacker_devices.h"
#include "logitacker_unifying_crypto.h"
#include "logitacker_keyboard_map.h"
#include "logitacker_bsp.h"
#include "helper.h"

#define NRF_LOG_MODULE_NAME LOGITACKER_PROCESSOR_PRX
#include "nrf_log.h"
#include "logitacker_usb.h"
#include "logitacker_options.h"

NRF_LOG_MODULE_REGISTER();



typedef struct {
    //logitacker_mode_t * p_logitacker_mainstate;
    uint8_t current_rf_address[5];

    uint8_t base_addr[4];
    uint8_t known_prefix;
    uint8_t current_channel_index;

    logitacker_devices_unifying_dongle_t devices[NRF_ESB_PIPE_COUNT];

    uint32_t tx_count;

    nrf_esb_payload_t tmp_rx_payload;
    nrf_esb_payload_t tmp_tx_payload;
} logitacker_processor_prx_ctx_t;

static logitacker_processor_t m_processor = {0};
static logitacker_processor_prx_ctx_t m_static_prx_ctx; //we reuse the same context, alternatively an malloc'ed ctx would allow separate instances
static char addr_str_buff[LOGITACKER_DEVICE_ADDR_STR_LEN] = {0};
static uint8_t m_keyboard_report_decryption_buffer[8] = { 0 };
static uint8_t m_hidpp_long_report_decryption_buffer[22] = { 0 };

void processor_prx_init_func(logitacker_processor_t *p_processor);
void processor_prx_init_func_(logitacker_processor_prx_ctx_t *self);

void processor_prx_deinit_func(logitacker_processor_t *p_processor);
void processor_prx_deinit_func_(logitacker_processor_prx_ctx_t *self);

void processor_prx_esb_handler_func(logitacker_processor_t *p_processor, nrf_esb_evt_t *p_esb_evt);
void processor_prx_esb_handler_func_(logitacker_processor_prx_ctx_t *self, nrf_esb_evt_t *p_esb_event);

void processor_prx_radio_handler_func(logitacker_processor_t *p_processor, radio_evt_t const *p_event);
void processor_prx_radio_handler_func_(logitacker_processor_prx_ctx_t *self, radio_evt_t const *p_event);

void processor_prx_bsp_handler_func(logitacker_processor_t *p_processor, bsp_event_t event);
void processor_prx_bsp_handler_func_(logitacker_processor_prx_ctx_t *self, bsp_event_t event);

void prx_process_rx(logitacker_processor_prx_ctx_t *self);

logitacker_processor_t * contruct_processor_prx_instance(logitacker_processor_prx_ctx_t *const prx_ctx) {
    m_processor.p_ctx = prx_ctx;

    m_processor.p_init_func = processor_prx_init_func;
    m_processor.p_deinit_func = processor_prx_deinit_func;
    m_processor.p_esb_handler = processor_prx_esb_handler_func;
//    m_processor.p_timer_handler = processor_active_enum_timer_handler_func;
    m_processor.p_bsp_handler = processor_prx_bsp_handler_func;
    m_processor.p_radio_handler = processor_prx_radio_handler_func;

    return &m_processor;
}

void processor_prx_init_func(logitacker_processor_t *p_processor) {
    processor_prx_init_func_((logitacker_processor_prx_ctx_t *) p_processor->p_ctx);
}

void processor_prx_init_func_(logitacker_processor_prx_ctx_t *self) {

    helper_addr_to_hex_str(addr_str_buff, LOGITACKER_DEVICE_ADDR_LEN, self->current_rf_address);
    NRF_LOG_INFO("Entering passive enumeration mode for address %s", addr_str_buff);


    nrf_esb_stop_rx(); //stop rx in case running
    radio_disable_rx_timeout_event(); // disable RX timeouts
    radio_stop_channel_hopping(); // disable channel hopping

    self->tx_count = 0;

//    *self->p_logitacker_mainstate = LOGITACKER_MODE_PASSIVE_ENUMERATION;

    //reset device state
    memset(self->devices, 0, sizeof(self->devices));

    helper_addr_to_base_and_prefix(self->base_addr, &self->known_prefix, self->current_rf_address, 5);

    // define listening device_prefixes for neighbours and dongle address (0x00 prefix)
    uint8_t prefixes[] = {
            0x00,
            self->known_prefix,
            self->known_prefix-1,
            self->known_prefix-2,
            self->known_prefix+1,
            self->known_prefix+2,
            self->known_prefix+3,
    };
    int prefix_count = sizeof(prefixes);

    // if the rf_address is in device list and has more than 1 prefix (f.e. from active scan), overwrite listen device_prefixes
    logitacker_devices_unifying_dongle_t * p_dongle = NULL;

    logitacker_devices_get_dongle_by_device_addr(&p_dongle, self->current_rf_address);
    if (p_dongle != NULL && p_dongle->num_connected_devices > 1) {
        logitacker_devices_unifying_device_t * p_device = NULL;
        for (int dev_idx=0; dev_idx < p_dongle->num_connected_devices; dev_idx++) {
            p_device = p_dongle->p_connected_devices[dev_idx];
            if (p_device != NULL) prefixes[dev_idx] = p_device->addr_prefix;
        }
        prefix_count = p_dongle->num_connected_devices;
    }


    bsp_board_leds_off(); //disable all LEDs

    uint32_t res = 0;

    //res = nrf_esb_set_mode(NRF_ESB_MODE_SNIFF);
    res = nrf_esb_set_mode(NRF_ESB_MODE_PRX);
    if (res != NRF_SUCCESS) NRF_LOG_ERROR("nrf_esb_set_mode SNIFF: %d", res);
    res = nrf_esb_set_base_address_1(self->base_addr); // set base addr1
    if (res != NRF_SUCCESS) NRF_LOG_ERROR("nrf_esb_set_base_address_1: %d", res);

    switch (g_logitacker_global_config.workmode) {
        case OPTION_LOGITACKER_WORKMODE_LIGHTSPEED:
            nrf_esb_update_channel_frequency_table_lightspeed();
            break;
        case OPTION_LOGITACKER_WORKMODE_G700:
        case OPTION_LOGITACKER_WORKMODE_UNIFYING:
            nrf_esb_update_channel_frequency_table_unifying();
            break;
    }

    for (int i=0; i<prefix_count; i++) {
        res = nrf_esb_update_prefix(i+1, prefixes[i]); // set suffix 0x00 for pipe 1 (dongle address
        if (res == NRF_SUCCESS) {
            NRF_LOG_INFO("listen on prefix %.2x with pipe %d", prefixes[i], i+1);
        } else {
            NRF_LOG_ERROR("error updating prefix pos %d with %.2x: %d", i+1, prefixes[i], res);
        }

    }

    // add global pairing address to pipe 0
    uint8_t pairing_base_addr[4];
    uint8_t pairing_prefix;
    helper_addr_to_base_and_prefix(pairing_base_addr, &pairing_prefix, UNIFYING_GLOBAL_PAIRING_ADDRESS, 5);
    res = nrf_esb_set_base_address_0(pairing_base_addr);
    if (res != NRF_SUCCESS) NRF_LOG_ERROR("nrf_esb_set_base_address_0: %d", res);
    res = nrf_esb_update_prefix(0, pairing_prefix);
    if (res != NRF_SUCCESS) NRF_LOG_ERROR("update prefix 0: %d", res);

    nrf_esb_enable_pipes(0xff >> (7-prefix_count)); //enable all used needed rx pipes + pipe 0


    res = nrf_esb_start_rx();
    if (res != NRF_SUCCESS) NRF_LOG_ERROR("start_rx: %d", res);

    radio_enable_rx_timeout_event(LOGITACKER_PRX_STAY_ON_CHANNEL_AFTER_RX_MS); //set RX timeout, the eventhandler starts channel hopping once this timeout is reached
}

void processor_prx_deinit_func(logitacker_processor_t *p_processor) {
    processor_prx_deinit_func_((logitacker_processor_prx_ctx_t *) p_processor->p_ctx);
}

void processor_prx_deinit_func_(logitacker_processor_prx_ctx_t *self) {

    NRF_LOG_INFO("Leaving passive enumeration mode");

    radio_disable_rx_timeout_event(); // disable RX timeouts
    radio_stop_channel_hopping(); // disable channel hopping
    nrf_esb_stop_rx(); //stop rx in case running

    self->tx_count = 0;

//    *self->p_logitacker_mainstate = LOGITACKER_MODE_IDLE;

    nrf_esb_set_mode(NRF_ESB_MODE_PTX);
}

void processor_prx_esb_handler_func(logitacker_processor_t *p_processor, nrf_esb_evt_t *p_esb_evt) {
    processor_prx_esb_handler_func_((logitacker_processor_prx_ctx_t *) p_processor->p_ctx, p_esb_evt);
}

void processor_prx_esb_handler_func_(logitacker_processor_prx_ctx_t *self, nrf_esb_evt_t *p_esb_event) {
    switch (p_esb_event->evt_id) {
        case NRF_ESB_EVENT_RX_RECEIVED:
            NRF_LOG_DEBUG("ESB EVENT HANDLER PASSIVE ENUMERATION RX_RECEIVED");
            prx_process_rx(self);
            break;
        /*
        case NRF_ESB_EVENT_TX_SUCCESS:
            NRF_LOG_INFO("PRX tx'de");
            self->tx_count++;
        */
        default:
            break;
    }

}

void processor_prx_radio_handler_func(logitacker_processor_t *p_processor, radio_evt_t const *p_event) {
    processor_prx_radio_handler_func_((logitacker_processor_prx_ctx_t *) p_processor->p_ctx, p_event);
}

void processor_prx_radio_handler_func_(logitacker_processor_prx_ctx_t *self, radio_evt_t const *p_event) {
    //helper_log_priority("UNIFYING_event_handler");
    switch (p_event->evt_id)
    {
        case RADIO_EVENT_NO_RX_TIMEOUT:
        {
            NRF_LOG_INFO("Passive enumeration: no RX on current channel for %d ms ... restart channel hopping ...", LOGITACKER_PASSIVE_ENUM_STAY_ON_CHANNEL_AFTER_RX_MS);
            radio_start_channel_hopping(LOGITACKER_PASSIVE_ENUM_CHANNEL_HOP_INTERVAL_MS, 0, true); //start channel hopping directly (0ms delay) with 30ms hop interval, automatically stop hopping on RX
            break;
        }
        case RADIO_EVENT_CHANNEL_CHANGED_FIRST_INDEX:
        {
            NRF_LOG_DEBUG("DISCOVERY MODE channel hop reached first channel");
            bsp_board_led_invert(LED_B); // toggle scan LED everytime we jumped through all channels
            break;
        }
        default:
            break;
    }
}

void processor_prx_bsp_handler_func(logitacker_processor_t *p_processor, bsp_event_t event) {
    processor_prx_bsp_handler_func_((logitacker_processor_prx_ctx_t *) p_processor->p_ctx, event);
}

void processor_prx_bsp_handler_func_(logitacker_processor_prx_ctx_t *self, bsp_event_t event) {
    NRF_LOG_INFO("Passive enumeration BSP event: %d", (unsigned int)event);
    switch ((unsigned int)event)
    {
        case BSP_USER_EVENT_RELEASE_0:
            NRF_LOG_INFO("ACTION BUTTON RELEASED in passive enumeration mode, changing back to discovery...");
            logitacker_enter_mode_discovery();
            break;
        default:
            break; // no implementation needed
    }

}


static uint8_t m_pass_through_keyboard_hid_input_report[8] = {0};
static uint8_t m_pass_through_mouse_hid_input_report[7] = {0};

void prx_process_rx(logitacker_processor_prx_ctx_t *self) {
    bsp_board_led_invert(LED_G);
    while (nrf_esb_read_rx_payload(&(self->tmp_rx_payload)) == NRF_SUCCESS) {
        uint8_t len = self->tmp_rx_payload.length;

//        NRF_LOG_INFO("Data RX on ch %d", self->tmp_rx_payload.rx_channel);
//        NRF_LOG_HEXDUMP_INFO(self->tmp_rx_payload.data, len);

        uint8_t unifying_report_type;
        bool unifying_is_keep_alive;
        logitacker_unifying_frame_classify(self->tmp_rx_payload, &unifying_report_type, &unifying_is_keep_alive);

//            logitacker_devices_unifying_dongle_t *p_device = &m_state_local.substate_prxeration.devices[rx_payload.pipe]; //pointer to correct device meta data

        uint8_t addr[5];
        nrf_esb_convert_pipe_to_address(self->tmp_rx_payload.pipe, addr);

        if (g_logitacker_global_config.passive_enum_pass_through_hidraw) {
            //logitacker_usb_write_hidraw_input_report_rf_frame(LOGITACKER_MODE_prxERATION, addr, &self->tmp_rx_payload);
        }


        uint8_t prefix;
        uint8_t base[4];
        helper_addr_to_base_and_prefix(base, &prefix, addr, 5); //convert device addr to base+prefix and update device

        logitacker_devices_unifying_device_t * p_device = NULL;
        logitacker_devices_get_device(&p_device, addr);


        if (p_device != NULL) logitacker_devices_device_update_classification(p_device, self->tmp_rx_payload);



        if (!(unifying_report_type == 0x00 && unifying_is_keep_alive) && len != 0) { //ignore keep-alive only frames and empty frames
            // Write a test payload
            if (self->tx_count < 100) {
                NRF_LOG_INFO("Writing TX payload %d", self->tx_count);
                self->tmp_tx_payload.pipe = self->tmp_rx_payload.pipe;
                self->tmp_tx_payload.data[0] = 0x00;
                self->tmp_tx_payload.data[1] = 0x10;
                self->tmp_tx_payload.data[2] = prefix;
                self->tmp_tx_payload.data[3] = 0x00;
                self->tmp_tx_payload.data[4] = 0x1F;
                self->tmp_tx_payload.data[5] = 0x00;
                self->tmp_tx_payload.data[6] = 0x00;
                self->tmp_tx_payload.data[7] = 0x00;
                self->tmp_tx_payload.data[8] = 0xFF;
                self->tmp_tx_payload.data[9] = 0xAC;
                self->tmp_tx_payload.length = 10;
                logitacker_unifying_payload_update_checksum(self->tmp_tx_payload.data, self->tmp_tx_payload.length);
                nrf_esb_write_payload(&self->tmp_tx_payload);
                self->tx_count++;
            }


            uint8_t ch_idx = self->tmp_rx_payload.rx_channel_index;
            uint8_t ch = self->tmp_rx_payload.rx_channel;
            helper_addr_to_hex_str(addr_str_buff, LOGITACKER_DEVICE_ADDR_LEN, addr);
            NRF_LOG_INFO("frame RX in passive enumeration mode (addr %s, len: %d, ch idx %d, raw ch %d)", addr_str_buff, len, ch_idx, ch);
            logitacker_unifying_frame_classify_log(self->tmp_rx_payload);
            NRF_LOG_HEXDUMP_INFO(self->tmp_rx_payload.data, self->tmp_rx_payload.length);


            // handle plain keyboard reports
            if (unifying_report_type == UNIFYING_RF_REPORT_PLAIN_KEYBOARD && len == 10) {
                // if pass-through for keyboard is enabled, send keystrokes to USB HID keyboard
                if (g_logitacker_global_config.passive_enum_pass_through_keyboard) {
                    //convert to hid out report

                    m_pass_through_keyboard_hid_input_report[0] = self->tmp_rx_payload.data[2]; // copy modifier
                    memcpy(&m_pass_through_keyboard_hid_input_report[2], &self->tmp_rx_payload.data[3], 6); // copy keys
                    // send via USB
                    if (logitacker_usb_write_keyboard_input_report(m_pass_through_keyboard_hid_input_report) != NRF_SUCCESS) {
                        NRF_LOG_WARNING("Failed to pass through USB report, busy with old report");
                    } else {
                        NRF_LOG_INFO("passed through plain keyboard frame to USB");
                    }
                }

            }

            // handle plain mouse reports
            if (unifying_report_type == UNIFYING_RF_REPORT_PLAIN_MOUSE && len == 10) {
                // if pass-through for mouse is enabled, send input to USB HID mouse
                if (g_logitacker_global_config.passive_enum_pass_through_mouse) {
                    //convert to hid out report

                    memcpy(m_pass_through_mouse_hid_input_report, &self->tmp_rx_payload.data[2], 7); // copy report data, don't copy report ID (we use 0x00 to align with the descriptor)
                    // send via USB
                    if (logitacker_usb_write_mouse_input_report(m_pass_through_mouse_hid_input_report) != NRF_SUCCESS) {
                        NRF_LOG_WARNING("Failed to pass through USB mouse report, busy with old report");
                    } else {
                        NRF_LOG_INFO("passed through mouse frame to USB");
                    }
                }

            }

            if (unifying_report_type == UNIFYING_RF_REPORT_ENCRYPTED_HIDPP_LONG) {
                if (p_device != NULL && p_device->key_known) {
                    if (logitacker_unifying_crypto_decrypt_encrypted_hidpp_frame(m_hidpp_long_report_decryption_buffer, p_device->key, &self->tmp_rx_payload) == NRF_SUCCESS) {
                        if (g_logitacker_global_config.passive_enum_pass_through_hidraw) {
                            // generate decrypted pseudo frame for hidraw pass trough
                            nrf_esb_payload_t pseudo_frame;
                            pseudo_frame.rssi = self->tmp_rx_payload.rssi;
                            pseudo_frame.pid = self->tmp_rx_payload.pid;
                            pseudo_frame.rx_channel = self->tmp_rx_payload.rx_channel;
                            pseudo_frame.length = 22;
                            pseudo_frame.validated_promiscuous_frame = false;
                            memcpy(pseudo_frame.data, m_hidpp_long_report_decryption_buffer, 22);

                            uint32_t result = logitacker_usb_write_hidraw_input_report_rf_frame(LOGITACKER_MODE_PASSIVE_ENUMERATION, addr, &pseudo_frame);
                            if (result != NRF_SUCCESS) {
                                NRF_LOG_ERROR("Failed writing decrypted frame to hidraw: %x", result);
                            }
                        }
                    }
                }
            }

            if (unifying_report_type == UNIFYING_RF_REPORT_ENCRYPTED_KEYBOARD) {
                // check if device key is known
                if (p_device != NULL && p_device->key_known) {
                    // try to decrypt frame
                    if (logitacker_unifying_crypto_decrypt_encrypted_keyboard_frame(m_keyboard_report_decryption_buffer, p_device->key, &self->tmp_rx_payload) == NRF_SUCCESS) {
                        NRF_LOG_INFO("Test decryption of keyboard payload:");
                        NRF_LOG_HEXDUMP_INFO(m_keyboard_report_decryption_buffer, 8);
                        //ToDo: print modifier

                        //print human readable modifier key
                        char tmp_mod_str[128];
                        modcode_to_str(tmp_mod_str, m_keyboard_report_decryption_buffer[0]);
                        NRF_LOG_INFO("Mod: %s", nrf_log_push(tmp_mod_str));

                        // print human readable form of keys
                        bool no_key = true;
                        for (int k=1; k<7; k++) {
                            if (m_keyboard_report_decryption_buffer[k] != 0x00) {
                                NRF_LOG_INFO("Key: %s", keycode_to_str(m_keyboard_report_decryption_buffer[k]));
                                no_key = false;
                            }

                        }
                        if (no_key) NRF_LOG_INFO("Key: NONE");

                        // if pass-through for keyboard is enabled, send keystrokes to USB HID keyboard
                        if (g_logitacker_global_config.passive_enum_pass_through_keyboard) {
                            //convert to hid out report

                            m_pass_through_keyboard_hid_input_report[0] = m_keyboard_report_decryption_buffer[0]; // copy modifier
                            memcpy(&m_pass_through_keyboard_hid_input_report[2], &m_keyboard_report_decryption_buffer[1], 6); // copy keys
                            // send via USB
                            if (logitacker_usb_write_keyboard_input_report(m_pass_through_keyboard_hid_input_report) != NRF_SUCCESS) {
                                NRF_LOG_WARNING("Failed to pass through USB report, busy with old report");
                            } else {
                                NRF_LOG_INFO("passed through decrypted keyboard frame to USB");
                            }
                        }

                        if (g_logitacker_global_config.passive_enum_pass_through_hidraw) {
                            // generate decrypted pseudo frame for hidraw pass trough
                            // 07 C1 00 4E 00 00 00 00 00 EA
                            nrf_esb_payload_t pseudo_frame;
                            pseudo_frame.rssi = self->tmp_rx_payload.rssi;
                            pseudo_frame.pid = self->tmp_rx_payload.pid;
                            pseudo_frame.rx_channel = self->tmp_rx_payload.rx_channel;
                            pseudo_frame.data[0] = self->tmp_rx_payload.data[0];
                            pseudo_frame.data[1] = 0xC1;
                            memcpy(&pseudo_frame.data[2], m_keyboard_report_decryption_buffer, 7);
                            logitacker_unifying_payload_update_checksum(pseudo_frame.data, 10);
                            pseudo_frame.length = 10;
                            pseudo_frame.validated_promiscuous_frame = false;

                            if (logitacker_usb_write_hidraw_input_report_rf_frame(LOGITACKER_MODE_PASSIVE_ENUMERATION, addr, &pseudo_frame) != NRF_SUCCESS) {
                                NRF_LOG_ERROR("Failed wirting decrypted frame to hidraw");
                            }
                        }

                    }
                }
            }
        }
    }
}

logitacker_processor_t * new_processor_prx(uint8_t *rf_address) {
    //update checksums of RF payloads (only needed as they are not hardcoded, to allow changing payloads)


    // initialize context (static in this case, has to use malloc for new instances)
    logitacker_processor_prx_ctx_t *const p_ctx = &m_static_prx_ctx;
    memset(p_ctx, 0, sizeof(*p_ctx)); //replace with malloc for dedicated instance

    //initialize member variables
    memcpy(p_ctx->current_rf_address, rf_address, 5);



    return contruct_processor_prx_instance(&m_static_prx_ctx);
}


