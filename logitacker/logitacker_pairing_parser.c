#include "helper.h"
#include "logitacker_pairing_parser.h"
#include "nrf_esb_illegalmod.h"
#include "sdk_common.h"
#include "logitacker_devices.h"

#define NRF_LOG_MODULE_NAME LOGITACKER_PAIRING_PARSER
#include "nrf_log.h"

NRF_LOG_MODULE_REGISTER();

typedef enum {
    REQ1,
    REQ2,
    REQ3,
    RSP1,
    RSP2,
    RSP_FINAL,
    UNKNOWN
} pairing_frame_class_t;

pairing_frame_class_t classify_frame(nrf_esb_payload_t const * const rx_payload) {
    if (rx_payload->length < 5) return UNKNOWN;
    if (rx_payload->length == 22) {
        if (rx_payload->data[1] == 0x5f) { //req
            switch (rx_payload->data[2]) {
                case 0x01:
                    return REQ1;
                case 0x02:
                    return REQ2;
                case 0x03:
                    return REQ3;
                default:
                    break;
            }
        } else if (rx_payload->data[1] == 0x1f) { //rsp
            switch (rx_payload->data[2]) {
                case 0x01:
                    return RSP1;
                case 0x02:
                    return RSP2;
                default:
                    break;
            }
        }
    } else if (rx_payload->length == 10 && rx_payload->data[1] == 0x0f) {
        return RSP_FINAL;
    }

    return UNKNOWN;
}

bool logitacker_pairing_parser_complete(logitacker_pairing_info_t * pi) {
    return pi->has_rsp_final &&
           pi->has_req1 &&
           pi->has_req2 &&
           pi->has_req3 &&
           pi->has_rsp1 &&
           pi->has_rsp2;
}

bool logitacker_pairing_derive_key_material(logitacker_pairing_info_t * pi) {
    memcpy(pi->device_raw_key_material, pi->device_rf_address, 4);
    memcpy(&pi->device_raw_key_material[4], pi->device_wpid, 2);
    memcpy(&pi->device_raw_key_material[6], pi->dongle_wpid, 2);
    memcpy(&pi->device_raw_key_material[8], pi->device_nonce, 4);
    memcpy(&pi->device_raw_key_material[12], pi->dongle_nonce, 4);
    return NRF_SUCCESS;
}

bool logitacker_pairing_derive_key(logitacker_pairing_info_t * pi) {
    pi->device_key[2] = pi->device_raw_key_material[0];
    pi->device_key[1] = pi->device_raw_key_material[1] ^ 0xFF;
    pi->device_key[5] = pi->device_raw_key_material[2] ^ 0xFF;
    pi->device_key[3] = pi->device_raw_key_material[3];
    pi->device_key[14] = pi->device_raw_key_material[4];
    pi->device_key[11] = pi->device_raw_key_material[5];
    pi->device_key[9] = pi->device_raw_key_material[6];
    pi->device_key[0] = pi->device_raw_key_material[7];
    pi->device_key[8] = pi->device_raw_key_material[8];
    pi->device_key[6] = pi->device_raw_key_material[9] ^ 0x55;
    pi->device_key[4] = pi->device_raw_key_material[10];
    pi->device_key[15] = pi->device_raw_key_material[11];
    pi->device_key[10] = pi->device_raw_key_material[12] ^0xFF;
    pi->device_key[12] = pi->device_raw_key_material[13];
    pi->device_key[7] = pi->device_raw_key_material[14];
    pi->device_key[13] = pi->device_raw_key_material[15] ^ 0x55;
    return NRF_SUCCESS;
}

bool logitacker_pairing_parser_complete_key_material(logitacker_pairing_info_t * pi) {
    return pi->has_req1 && //device WPID
    pi->has_req2 && // device nonce
    pi->has_rsp1 && // dongle WPID & base_addr (serial)
    pi->has_rsp2; // dongle nonce
}


uint32_t logitacker_pairing_parser_req1(logitacker_pairing_info_t * pi, nrf_esb_payload_t const * const rx_payload) {
    memcpy(pi->device_wpid, &rx_payload->data[LOGITACKER_UNIFYING_PAIRING_REQ1_OFFSET_DEVICE_WPID], 2);
    pi->device_caps = rx_payload->data[LOGITACKER_UNIFYING_PAIRING_REQ1_OFFSET_DEVICE_CAPS];
    pi->device_type = rx_payload->data[LOGITACKER_UNIFYING_PAIRING_REQ1_OFFSET_DEVICE_TYPE];
    pi->has_req1 = true;
    return NRF_SUCCESS;
}

uint32_t logitacker_pairing_parser_req2(logitacker_pairing_info_t * pi, nrf_esb_payload_t const * const rx_payload) {
    memcpy(pi->device_nonce, &rx_payload->data[LOGITACKER_UNIFYING_PAIRING_REQ2_OFFSET_DEVICE_NONCE], 4);
    memcpy(pi->device_serial, &rx_payload->data[LOGITACKER_UNIFYING_PAIRING_REQ2_OFFSET_DEVICE_SERIAL], 4);
    pi->device_report_types = rx_payload->data[LOGITACKER_UNIFYING_PAIRING_REQ2_OFFSET_DEVICE_REPORT_TYPES_LE] +
            (rx_payload->data[LOGITACKER_UNIFYING_PAIRING_REQ2_OFFSET_DEVICE_REPORT_TYPES_LE+1] << 8) +
            (rx_payload->data[LOGITACKER_UNIFYING_PAIRING_REQ2_OFFSET_DEVICE_REPORT_TYPES_LE+2] << 16) +
            (rx_payload->data[LOGITACKER_UNIFYING_PAIRING_REQ2_OFFSET_DEVICE_REPORT_TYPES_LE+3] << 24);
    pi->device_usability_info = rx_payload->data[LOGITACKER_UNIFYING_PAIRING_REQ2_OFFSET_DEVICE_USABILITY_INFO];
    pi->has_req2 = true;

    return NRF_SUCCESS;
}

uint32_t logitacker_pairing_parser_req3(logitacker_pairing_info_t * pi, nrf_esb_payload_t const * const rx_payload) {
    pi->device_name_len = rx_payload->data[LOGITACKER_UNIFYING_PAIRING_REQ3_OFFSET_DEVICE_NAME_LEN];
    if (pi->device_name_len > 16) pi->device_name_len = 16;
    memcpy(pi->device_name, &rx_payload->data[LOGITACKER_UNIFYING_PAIRING_REQ3_OFFSET_DEVICE_NAME], pi->device_name_len);
    pi->device_name[pi->device_name_len] = 0x00; //terminate string
    pi->has_req3 = true;
    return NRF_SUCCESS;
}

uint32_t logitacker_pairing_parser_rsp1(logitacker_pairing_info_t * pi, nrf_esb_payload_t const * const rx_payload) {
    memcpy(pi->dongle_wpid, &rx_payload->data[LOGITACKER_UNIFYING_PAIRING_RSP1_OFFSET_DONGLE_WPID], 2);
    memcpy(pi->device_rf_address, &rx_payload->data[LOGITACKER_UNIFYING_PAIRING_RSP1_OFFSET_BASE_ADDR], 5);

    helper_addr_to_base_and_prefix(pi->base_addr,&pi->device_prefix, pi->device_rf_address, LOGITACKER_DEVICE_ADDR_LEN);
    pi->has_rsp1 = true;

    return NRF_SUCCESS;
}

uint32_t logitacker_pairing_parser_rsp2(logitacker_pairing_info_t * pi, nrf_esb_payload_t const * const rx_payload) {
    memcpy(pi->dongle_nonce, &rx_payload->data[LOGITACKER_UNIFYING_PAIRING_RSP2_OFFSET_DONGLE_NONCE], 4);
    pi->has_rsp2 = true;

    if (logitacker_pairing_parser_complete_key_material(pi)) {
        // derive raw key material
        logitacker_pairing_derive_key_material(pi);
        logitacker_pairing_derive_key(pi);
        pi->key_material_complete = true;
    }
    return NRF_SUCCESS;
}

uint32_t logitacker_pairing_parser_rsp_final(logitacker_pairing_info_t * pi, nrf_esb_payload_t const * const rx_payload) {
    pi->has_rsp_final = true;

    pi->full_pairing = logitacker_pairing_parser_complete(pi);
    return NRF_SUCCESS;
}

uint32_t logitacker_pairing_parser_print(logitacker_pairing_info_t * pi) {
    NRF_LOG_INFO("Device name: %s", pi->device_name);
    NRF_LOG_INFO("Device RF address: %.2x:%.2x:%.2x:%.2x:%.2x", pi->device_rf_address[0], pi->device_rf_address[1], pi->device_rf_address[2], pi->device_rf_address[3], pi->device_rf_address[4]);
    NRF_LOG_INFO("Device serial: %.2x:%.2x:%.2x:%.2x", pi->device_serial[0], pi->device_serial[1], pi->device_serial[2], pi->device_serial[3]);
    NRF_LOG_INFO("Device WPID: 0x%.2x%.2x", pi->device_wpid[0], pi->device_wpid[1]);
    NRF_LOG_INFO("Device report types: 0x%.8x", pi->device_report_types);
    NRF_LOG_INFO("Device usability info: 0x%.2x", pi->device_usability_info);
    NRF_LOG_INFO("Dongle WPID: 0x%.2x%.2x", pi->dongle_wpid[0], pi->dongle_wpid[1]);
    NRF_LOG_INFO("Device caps: 0x%.2x", pi->device_caps);
    NRF_LOG_INFO("Device report types: 0x%.8x", pi->device_report_types);
    if (pi->key_material_complete) {
        NRF_LOG_INFO("Device raw key material:")
        NRF_LOG_HEXDUMP_INFO(pi->device_raw_key_material, sizeof(pi->device_raw_key_material));
        NRF_LOG_INFO("Device key:")
        NRF_LOG_HEXDUMP_INFO(pi->device_key, sizeof(pi->device_key));
    }
    return NRF_SUCCESS;
}

uint32_t logitacker_pairing_parser(logitacker_pairing_info_t * pi, nrf_esb_payload_t const * const rx_payload) {
    switch (classify_frame(rx_payload)) {
        case REQ1:
            logitacker_pairing_parser_req1(pi, rx_payload);
            break;
        case REQ2:
            logitacker_pairing_parser_req2(pi, rx_payload);
            break;
        case REQ3:
            logitacker_pairing_parser_req3(pi, rx_payload);
            break;
        case RSP1:
            logitacker_pairing_parser_rsp1(pi, rx_payload);
            break;
        case RSP2:
            logitacker_pairing_parser_rsp2(pi, rx_payload);
            break;
        case RSP_FINAL:
            logitacker_pairing_parser_rsp_final(pi, rx_payload);
            if (logitacker_pairing_parser_complete(pi)) {
                logitacker_pairing_parser_print(pi);
                return NRF_SUCCESS;
            }
            break;
        default:
            break;
    }

    if (logitacker_pairing_parser_complete(pi)) {
        return NRF_SUCCESS;
    }

    return NRF_ERROR_INVALID_DATA; //indicate that frames are missing
}