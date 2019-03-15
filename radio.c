#include <string.h> //memcpy, memset
#include <stddef.h> //NULL

#include "radio.h"
#include "crc16.h"
#include "nrf_error.h"
#include "nrf_esb_illegalmod.h"


#include "sdk_macros.h"

#define BIT_MASK_UINT_8(x) (0xFF >> (8 - (x)))

typedef struct
{
    bool initialized;
    radio_rf_mode_t         mode;
    nrf_esb_event_handler_t event_handler;
    uint8_t rf_channel;             /**< Channel to use (must be between 0 and 100). */
    uint8_t base_addr_p0[4];        /**< Base address for pipe 0 encoded in big endian. */
    uint8_t base_addr_p1[4];        /**< Base address for pipe 1-7 encoded in big endian. */
    uint8_t pipe_prefixes[8];       /**< Address prefix for pipe 0 to 7. */
    uint8_t num_pipes;              /**< Number of pipes available. */
    uint8_t addr_length;            /**< Length of the address including the prefix. */
    uint8_t rx_pipes_enabled;       /**< Bitfield for enabled pipes. */    
} radio_config_t;

static nrf_esb_config_t m_local_esb_config = NRF_ESB_DEFAULT_CONFIG;

static radio_config_t m_local_config = {
    .mode = RADIO_MODE_DISABLED,
    .initialized = false,
    .base_addr_p0 = { 0xE7, 0xE7, 0xE7, 0xE7 },
    .base_addr_p1       = { 0xC2, 0xC2, 0xC2, 0xC2 },                           \
    .pipe_prefixes      = { 0xE7, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8 },   \
    .addr_length        = 5,                                                    \
    .num_pipes          = NRF_ESB_PIPE_COUNT,                                   \
    .rf_channel         = 5,                                                    \
    .rx_pipes_enabled   = 0xFF                                                  \
};

bool check_crc16(uint8_t * p_array, uint8_t len) {
    uint16_t crc = crc16_compute(p_array, (uint32_t) len, NULL);

    if (crc == 0x0000) {
        return true;
    }

    return false;
}

bool validate_esb_frame(uint8_t * p_array, uint8_t addrlen) {
    uint8_t framelen = p_array[addrlen] >> 2;
    if (framelen > 32) {
        return false; // early out if ESB frame has a length > 32, this only accounts for "old style" ESB which is bound to 32 byte max payload length
    }
    uint8_t crclen = addrlen + 1 + framelen + 2; //+1 for PCF (we ignore one bit), +2 for crc16

    return check_crc16(p_array, crclen);
}

void array_shl(uint8_t *p_array, uint8_t len, uint8_t bits) {
    if (len == 1) {
        p_array[0] = p_array[0] << bits;
        return;
    }
    
    for (uint8_t i=0; i<(len-1); i++) {
        p_array[i] = p_array[i] << bits | p_array[i+1] >> (8-bits);
    }
    p_array[len-1] = p_array[len-1] << bits;
    return;
}

#define VALIDATION_SHIFT_BUF_SIZE 64
uint32_t validate_esb_payload(nrf_esb_payload_t * p_payload) {
    uint8_t assumed_addrlen = 5; //Validation has to take RF address length of raw frames into account, thus we assume the given length in byte
    uint8_t tmpData[VALIDATION_SHIFT_BUF_SIZE];
    uint8_t tmpDataLen = p_payload->length;
    static bool crcmatch = false;

    for (uint8_t i=0; i< tmpDataLen; i++) {
        tmpData[i] = p_payload -> data[i];
    }
  
    // if processing takes too long RF frames are discarded
    // the shift value in the following for loop controls how often a received
    // frame is shifted for CRC check. If this value is too large, frames are dropped,
    // if it is too low, chance for detecting valid frames decreases.
    // The validate_esb_frame function has an early out, if determined ESB frame length
    // exceeds 32 byte, which avoids unnecessary CRC16 calculations.
    crcmatch = false;
    for (uint8_t shift=0; shift<32; shift++) {
        if (validate_esb_frame(tmpData, assumed_addrlen)) {
            crcmatch = true;
            break;
        }
        array_shl(tmpData, tmpDataLen, 1);
    }

    if (crcmatch) {
        //wipe out old rx data
        memset(p_payload->data, 0, p_payload->length);

        uint8_t esb_len = tmpData[assumed_addrlen] >> 2; //extract length bits from the assumed Packet Control Field, which starts right behind the RF address

        //correct RF frame length if CRC match
        p_payload->length = assumed_addrlen + 1 + esb_len + 2; //final payload (5 byte address, ESB payload with dynamic length, higher 8 PCF bits, 2 byte CRC)

        //byte allign payload (throw away no_ack bit of PCF, keep the other 8 bits)
        array_shl(&tmpData[assumed_addrlen+1], esb_len, 1); //shift left all bytes behind the PCF field by one bit (we loose the lowest PCF bit, which is "no ack", and thus not of interest)

        /*
        //zero out rest of report
        memset(&tmpData[p_payload->length], 0, VALIDATION_SHIFT_BUF_SIZE - p_payload->length);
        */
        memcpy(&p_payload->data[2], tmpData, p_payload->length);
        p_payload->length += 2;
        p_payload->data[0] = p_payload->pipe; //encode rx pipe
        p_payload->data[1] = esb_len; //encode real ESB payload length

        return NRF_SUCCESS;
    } else {
        return NRF_ERROR_INVALID_DATA;
    }
}

uint32_t restoreRfSettings() {
    uint32_t err_code;

    err_code = nrf_esb_set_base_address_0(m_local_config.base_addr_p0);
    VERIFY_SUCCESS(err_code);
    err_code = nrf_esb_set_base_address_1(m_local_config.base_addr_p1);
    VERIFY_SUCCESS(err_code);
    err_code = nrf_esb_set_address_length(m_local_config.addr_length);
    VERIFY_SUCCESS(err_code);
    err_code = nrf_esb_set_prefixes(m_local_config.pipe_prefixes, m_local_config.num_pipes);
    VERIFY_SUCCESS(err_code);
    err_code = nrf_esb_enable_pipes(m_local_config.rx_pipes_enabled);
    VERIFY_SUCCESS(err_code);

    return err_code;
}

uint32_t radioInit(nrf_esb_event_handler_t event_handler) {
    //uint32_t err_code;
    m_local_config.event_handler            = event_handler;
    //err_code = nrf_esb_init(&m_local_esb_config);
    //VERIFY_SUCCESS(err_code);
    m_local_config.initialized = true;
    m_local_config.mode = RADIO_MODE_DISABLED;

    

    return NRF_SUCCESS;
    //return err_code;
}

uint32_t radioInitPromiscuousMode() {
    if (!m_local_config.initialized) return NRF_ERROR_INVALID_STATE;

    uint32_t err_code;
    
    uint8_t base_addr_0[4] = {0xa8, 0xa8, 0xa8, 0xa8}; //only one octet used, as address length will be illegal
    uint8_t base_addr_1[4] = {0xaa, 0xaa, 0xaa, 0xaa}; //only one octet used, as address length will be illegal
    uint8_t addr_prefix[8] = {0xaa, 0x1f, 0x9f, 0xa8, 0xaf, 0xa9, 0x8f, 0xaa}; //prefix for pipe 0..7                                                               
    
    nrf_esb_config_t esb_config = NRF_ESB_ILLEGAL_CONFIG;
    esb_config.event_handler            = m_local_config.event_handler;
 
    
 
    err_code = nrf_esb_init(&esb_config);
    VERIFY_SUCCESS(err_code);
    m_local_esb_config = esb_config;

    err_code = nrf_esb_set_base_address_0(base_addr_0);
    VERIFY_SUCCESS(err_code);

    err_code = nrf_esb_set_base_address_1(base_addr_1);
    VERIFY_SUCCESS(err_code);

    err_code = nrf_esb_set_prefixes(addr_prefix, 8);
    VERIFY_SUCCESS(err_code);

    err_code = nrf_esb_set_rf_channel(m_local_config.rf_channel);
    VERIFY_SUCCESS(err_code);

    err_code = nrf_esb_start_rx();
    VERIFY_SUCCESS(err_code);
   
    return NRF_SUCCESS;
}

uint32_t radioDeinitPromiscuousMode() {
    if (!m_local_config.initialized) return NRF_ERROR_INVALID_STATE;
    if (m_local_config.mode != RADIO_MODE_PROMISCOUS) return NRF_ERROR_INVALID_STATE;

    uint32_t err_code;
    
    err_code = nrf_esb_stop_rx();
    VERIFY_SUCCESS(err_code);

    err_code = restoreRfSettings();
    VERIFY_SUCCESS(err_code);

    m_local_config.mode = RADIO_MODE_DISABLED;
    return err_code;
}

uint32_t radioInitPRXPassiveMode() {
    if (!m_local_config.initialized) return NRF_ERROR_INVALID_STATE;

    uint32_t err_code;
    
    
    nrf_esb_config_t esb_config = NRF_ESB_DEFAULT_CONFIG;
    esb_config.mode = NRF_ESB_MODE_PRX;
    esb_config.selective_auto_ack = true; //Don't send ack without permission
    esb_config.event_handler    = m_local_config.event_handler;
    
    
 
    err_code = nrf_esb_init(&esb_config);
    VERIFY_SUCCESS(err_code);
    m_local_esb_config = esb_config;

    err_code = restoreRfSettings();
    VERIFY_SUCCESS(err_code);


/*
    err_code = nrf_esb_set_base_address_0(m_local_config.base_addr_p0);
    VERIFY_SUCCESS(err_code);

    err_code = nrf_esb_set_base_address_1(m_local_config.base_addr_p1);
    VERIFY_SUCCESS(err_code);

    err_code = nrf_esb_set_prefixes(m_local_config.pipe_prefixes, m_local_config.num_pipes);
    VERIFY_SUCCESS(err_code);

    err_code = nrf_esb_set_rf_channel(m_local_config.rf_channel);
    VERIFY_SUCCESS(err_code);

    err_code = nrf_esb_enable_pipes(m_local_config.rx_pipes_enabled);
    VERIFY_SUCCESS(err_code);
*/
    
    err_code = nrf_esb_start_rx();
    VERIFY_SUCCESS(err_code);
    
   
    return NRF_SUCCESS;
}

radio_rf_mode_t radioGetMode() {
    return m_local_config.mode;
}

uint32_t radioSetMode(radio_rf_mode_t mode) {
    if (m_local_config.mode == mode) {
        return NRF_SUCCESS; //no change
    }

    uint32_t err_code;

    switch (mode) {
        case RADIO_MODE_DISABLED:
            break;
        case RADIO_MODE_PTX:
            break;
        case RADIO_MODE_PRX_ACTIVE:
            break;
        case RADIO_MODE_PRX_PASSIVE:
            //bring radio to IDLE state, depends on current mode
            if (m_local_config.mode == RADIO_MODE_PROMISCOUS) {
                nrf_esb_stop_rx();
            }
            err_code = radioInitPRXPassiveMode();
            VERIFY_SUCCESS(err_code);
            m_local_config.mode = mode; //update current mode
            break;
        case RADIO_MODE_PROMISCOUS:
            //bring radio to IDLE state, depends on current mode
            if (m_local_config.mode == RADIO_MODE_PRX_PASSIVE) {
                nrf_esb_stop_rx();
            }

            // promiscous mode always needs (re)init, as Packer Format is changed
            err_code = radioInitPromiscuousMode();
            VERIFY_SUCCESS(err_code);
            m_local_config.mode = mode; //update current mode
            break;
    }

    return 0;
}

uint32_t radioSetAddressLength(uint8_t length) {
    if (m_local_config.mode == RADIO_MODE_PROMISCOUS) return NRF_ERROR_FORBIDDEN;
    uint32_t err_code = nrf_esb_set_address_length(length);
    VERIFY_SUCCESS(err_code);
    m_local_config.addr_length = length;
    return err_code;
}

uint32_t radioSetBaseAddress0(uint8_t const * p_addr) {
    if (m_local_config.mode == RADIO_MODE_PROMISCOUS) return NRF_ERROR_FORBIDDEN;
    uint32_t err_code = nrf_esb_set_base_address_0(p_addr);
    VERIFY_SUCCESS(err_code);
    memcpy(m_local_config.base_addr_p0, p_addr, 4);
    return err_code;
}

uint32_t radioSetBaseAddress1(uint8_t const * p_addr) {
    if (m_local_config.mode == RADIO_MODE_PROMISCOUS) return NRF_ERROR_FORBIDDEN;
    uint32_t err_code = nrf_esb_set_base_address_1(p_addr);
    VERIFY_SUCCESS(err_code);
    memcpy(m_local_config.base_addr_p1, p_addr, 4);
    return err_code;
}

uint32_t radioSetPrefixes(uint8_t const * p_prefixes, uint8_t num_pipes) {
    if (m_local_config.mode == RADIO_MODE_PROMISCOUS) return NRF_ERROR_FORBIDDEN;
    uint32_t err_code = nrf_esb_set_prefixes(p_prefixes, num_pipes);
    VERIFY_SUCCESS(err_code);
    memcpy(m_local_config.pipe_prefixes, p_prefixes, num_pipes);
    m_local_config.num_pipes = num_pipes;
    m_local_config.rx_pipes_enabled = BIT_MASK_UINT_8(num_pipes);
    return err_code;
}

uint32_t radioEnablePipes(uint8_t enable_mask) {
    if (m_local_config.mode == RADIO_MODE_PROMISCOUS) return NRF_ERROR_FORBIDDEN;
    uint32_t err_code = nrf_esb_enable_pipes(enable_mask);
    VERIFY_SUCCESS(err_code);

    m_local_config.rx_pipes_enabled = enable_mask;
    return err_code;
}

uint32_t radioUpdatePrefix(uint8_t pipe, uint8_t prefix) {
    if (m_local_config.mode == RADIO_MODE_PROMISCOUS) return NRF_ERROR_FORBIDDEN;
    uint32_t err_code = nrf_esb_update_prefix(pipe, prefix);
    VERIFY_SUCCESS(err_code);
    m_local_config.pipe_prefixes[pipe] = prefix;
    return err_code;
}

uint32_t radioSetRfChannel(uint32_t channel) {
    uint32_t ret;
    switch (m_local_config.mode) {
        case RADIO_MODE_PROMISCOUS:
        case RADIO_MODE_PRX_ACTIVE:
        case RADIO_MODE_PRX_PASSIVE:
            nrf_esb_stop_rx();
            ret = nrf_esb_set_rf_channel(channel);
            if (ret == NRF_SUCCESS) m_local_config.rf_channel = channel;
            nrf_esb_start_rx();
            return ret;

            break;
        default:
            break;
    }
    return NRF_SUCCESS;
}

uint32_t radioGetRfChannel(uint32_t * p_channel) {
    return nrf_esb_get_rf_channel(p_channel);
}

