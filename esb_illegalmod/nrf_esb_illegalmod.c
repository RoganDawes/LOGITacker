/**
 * This is a modified version of the nrf_esb library by Marcus Mengs.
 * The original library is provided with the nRF5 SDK by Nordic Semiconductor ASA.
 * The copyright notice from the unmodified library could be found below.
 *
 * Many changes have been made to the library in order to support pseudo-promiscuous
 * mode for ESB, amongst these changes:
 *
 * - Pseudo promiscuous mode has a "post processing step" (checks valid ESB frames
 * in received noise). As this adds a huge amount of computation overhead, it was
 * off-loaded to app_scheduler. Thus the library depends on app_scheduler.
 * - The promiscuous mode post processing step, optionally checks captured ESB frames
 * to be valid Logitech RF communication (additional vendor checksum, special length).
 * Thus it depends on the `logitacker_unifying" module and can't be used stand-alone.
 * This behavior could be changed by un-setting the `LOGITECH_FILTER` definition in
 * `nrf_esb_illegalmod.h`
 * - Additional operation modes for the radio are deployed and used by Logitacker.
 * For example the mode `NRF_ESB_MODE_PTX_STAY_RX` which puts the radio into PTX mode,
 * but toggles back AND STAYS in PRX mode after a single transmission. This was implemented
 * to reduce timing overhead of manual toggling between PRX and PTX to an absolute minimum
 * (utilizes PPI and proper SHORTS)
 * - The `nrf_esb_payload_t` data structure contains a boolean field called `noack`.
 * This boolean is connected to the no-acknowledge bit in the Packet Control Field (PCF)
 * of an ESB frame. The unmodified `nrf_esb` library unsets the no-acknowledge bit in the
 * PCF field if the `noack`boolean in the data structure is set to true (somehow reversed
 * behavior). This has been changed in the `nrf_esb_illegalmod` library: If the `noack`
 * boolean is set to true, the no-acknowledgment bit of the resulting ESB frame is set
 * (not unset) before transmission.
 *
 * Note: The term `illegal`in the library name refers to undocumented register settings
 * used for the nRF52844 radio, in order to obtain pseudo promiscuous RX capabilities.
 * It does not mean that something illegal (in common sense of the word) is done by this
 * library
 *
 */

/**
 * Copyright (c) 2016 - 2018, Nordic Semiconductor ASA
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "nrf_error.h"
#include "nrf_esb_illegalmod.h"
#include "nrf_esb_illegalmod_error_codes.h"
#include "nrf_gpio.h"
#include <string.h>
#include <stddef.h>
#include "sdk_common.h"
#include "sdk_macros.h"
#include "app_util.h"
#include "nrf_delay.h"

#include "bsp.h"
#include "helper.h"
#include "logitacker_unifying.h"
#include "app_scheduler.h"

#define NRF_LOG_MODULE_NAME ESB_ILLEGALMOD
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

#define LOGITECH_FILTER

#define BIT_MASK_UINT_8(x) (0xFF >> (8 - (x)))

// Constant parameters
//#define RX_WAIT_FOR_ACK_TIMEOUT_US_2MBPS        (48)        /**< 2 Mb RX wait for acknowledgment time-out value. Smallest reliable value - 43. */
#define RX_WAIT_FOR_ACK_TIMEOUT_US_2MBPS        (50)        /**< 2 Mb RX wait for acknowledgment time-out value. For nRF24LU1+ (CU0007) 50 is the minimum reliable value (endless tests). */
#define RX_WAIT_FOR_ACK_TIMEOUT_US_1MBPS        (73)        /**< 1 Mb RX wait for acknowledgment time-out value. Smallest reliable value - 68. */
#define RX_WAIT_FOR_ACK_TIMEOUT_US_250KBPS      (250)       /**< 250 Kb RX wait for acknowledgment time-out value. */
#define RX_WAIT_FOR_ACK_TIMEOUT_US_1MBPS_BLE    (73)        /**< 1 Mb RX wait for acknowledgment time-out (combined with BLE). Smallest reliable value - 68.*/

// Interrupt flags
#define     NRF_ESB_INT_TX_SUCCESS_MSK          0x01        /**< Interrupt mask value for TX success. */
#define     NRF_ESB_INT_TX_FAILED_MSK           0x02        /**< Interrupt mask value for TX failure. */
#define     NRF_ESB_INT_RX_DATA_RECEIVED_MSK    0x04        /**< Interrupt mask value for RX_DR. */
#define     NRF_ESB_INT_RX_PROMISCUOUS_DATA_RECEIVED_MSK    0x08        /**< Interrupt mask value for RX_DR. */

#define     NRF_ESB_PID_RESET_VALUE             0xFF        /**< Invalid PID value which is guaranteed to not collide with any valid PID value. */
#define     NRF_ESB_PID_MAX                     3           /**< Maximum value for PID. */
#define     NRF_ESB_CRC_RESET_VALUE             0xFFFF      /**< CRC reset value. */

// Internal Enhanced ShockBurst module state.
typedef enum {
    NRF_ESB_STATE_IDLE,                                     /**< Module idle. */
    NRF_ESB_STATE_PTX_TX,                                   /**< Module transmitting without acknowledgment. */
    NRF_ESB_STATE_PTX_TX_ACK,                               /**< Module transmitting with acknowledgment. */
    NRF_ESB_STATE_PTX_RX_ACK,                               /**< Module transmitting with acknowledgment and reception of payload with the acknowledgment response. */
    NRF_ESB_STATE_PRX,                                      /**< Module receiving packets without acknowledgment. */
    NRF_ESB_STATE_PRX_SEND_ACK,                             /**< Module transmitting acknowledgment in RX mode. */
} nrf_esb_mainstate_t;


#define DISABLE_RF_IRQ()      NVIC_DisableIRQ(RADIO_IRQn)
#define ENABLE_RF_IRQ()       NVIC_EnableIRQ(RADIO_IRQn)

#define _RADIO_SHORTS_COMMON ( RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk | \
            RADIO_SHORTS_ADDRESS_RSSISTART_Msk | RADIO_SHORTS_DISABLED_RSSISTOP_Msk )

#define VERIFY_PAYLOAD_LENGTH(p)                            \
do                                                          \
{                                                           \
    if (p->length == 0 ||                                   \
       p->length > NRF_ESB_MAX_PAYLOAD_LENGTH)              \
    {                                                       \
        return NRF_ERROR_INVALID_LENGTH;                    \
    }                                                       \
}while (0)


/* @brief Structure holding pipe info PID and CRC and acknowledgment payload. */
typedef struct
{
    uint16_t    crc;                                      /**< CRC value of the last received packet (Used to detect retransmits). */
    uint8_t     pid;                                      /**< Packet ID of the last received packet (Used to detect retransmits). */
    bool        ack_payload;                              /**< Flag indicating the state of the transmission of acknowledgment payloads. */
} pipe_info_t;


/* @brief  First-in, first-out queue of payloads to be transmitted. */
typedef struct
{
    nrf_esb_payload_t * p_payload[NRF_ESB_TX_FIFO_SIZE];  /**< Pointer to the actual queue. */
    uint32_t            entry_point;                      /**< Current start of queue. */
    uint32_t            exit_point;                       /**< Current end of queue. */
    uint32_t            count;                            /**< Current number of elements in the queue. */
} nrf_esb_payload_tx_fifo_t;


/* @brief First-in, first-out queue of received payloads. */
typedef struct
{
    nrf_esb_payload_t * p_payload[NRF_ESB_RX_FIFO_SIZE];  /**< Pointer to the actual queue. */
    uint32_t            entry_point;                      /**< Current start of queue. */
    uint32_t            exit_point;                       /**< Current end of queue. */
    uint32_t            count;                            /**< Current number of elements in the queue. */
} nrf_esb_payload_rx_fifo_t;


/**@brief Enhanced ShockBurst address.
 *
 * Enhanced ShockBurst addresses consist of a base address and a prefix
 *          that is unique for each pipe. See @ref esb_addressing in the ESB user
 *          guide for more information.
*/
typedef struct
{
    uint8_t base_addr_p0[4];        /**< Base address for pipe 0 encoded in big endian. */
    uint8_t base_addr_p1[4];        /**< Base address for pipe 1-7 encoded in big endian. */
    uint8_t pipe_prefixes[8];       /**< Address prefix for pipe 0 to 7. */
    uint8_t num_pipes;              /**< Number of pipes available. */
    uint8_t addr_length;            /**< Length of the address including the prefix. */
    uint8_t rx_pipes_enabled;       /**< Bitfield for enabled pipes. */
    uint8_t rf_channel;             /**< Channel to use (must be between 0 and 100). */

    uint8_t channel_to_frequency[101];
    uint8_t channel_to_frequency_len;
} nrf_esb_address_t;


// Module state
static bool                         m_esb_initialized           = false;
static volatile nrf_esb_mainstate_t m_nrf_esb_mainstate         = NRF_ESB_STATE_IDLE;
static nrf_esb_payload_t          * mp_current_payload;

static nrf_esb_event_handler_t      m_event_handler;

// Address parameters
__ALIGN(4) static nrf_esb_address_t m_esb_addr = NRF_ESB_ADDR_DEFAULT;

// RF parameters
static nrf_esb_config_t             m_config_local;

// TX FIFO
static nrf_esb_payload_t            m_tx_fifo_payload[NRF_ESB_TX_FIFO_SIZE];
static nrf_esb_payload_tx_fifo_t    m_tx_fifo;

// RX FIFO
static nrf_esb_payload_t            m_rx_fifo_payload[NRF_ESB_RX_FIFO_SIZE];
static nrf_esb_payload_rx_fifo_t    m_rx_fifo;

// Payload buffers
static  uint8_t                     m_tx_payload_buffer[NRF_ESB_MAX_PAYLOAD_LENGTH + 2 + 28];
static  uint8_t                     m_rx_payload_buffer[NRF_ESB_MAX_PAYLOAD_LENGTH + 2 + 28];

// Run time variables
static volatile uint32_t            m_interrupt_flags = 0;
static uint8_t                      m_pids[NRF_ESB_PIPE_COUNT];
static pipe_info_t                  m_rx_pipe_info[NRF_ESB_PIPE_COUNT];
static volatile uint32_t            m_retransmits_remaining;
static volatile uint32_t            m_last_tx_attempts;
static volatile uint32_t            m_wait_for_ack_timeout_us;

static volatile uint32_t            m_retransmit_all_channels_remaining_channel_hop_count;
static volatile bool                m_retransmit_all_channels_running_channel_sweep;

static volatile uint32_t            m_radio_shorts_common = _RADIO_SHORTS_COMMON;

// These function pointers are changed dynamically, depending on protocol configuration and state.
static void (*on_radio_disabled)(void) = 0;
static void (*on_radio_end)(void) = 0;
static void (*update_rf_payload_format)(uint32_t payload_length) = 0;


// The following functions are assigned to the function pointers above.
static void on_radio_disabled_tx_noack(void);
static void on_radio_disabled_tx(void);
static void on_radio_disabled_tx_wait_for_ack(void);
static void on_radio_disabled_tx_wait_for_ack_continue_rx(void);
static void on_radio_disabled_rx(void);
static void on_radio_disabled_rx_ack(void);


#define NRF_ESB_ADDR_UPDATE_MASK_BASE0          (1 << 0)    /*< Mask value to signal updating BASE0 radio address. */
#define NRF_ESB_ADDR_UPDATE_MASK_BASE1          (1 << 1)    /*< Mask value to signal updating BASE1 radio address. */
#define NRF_ESB_ADDR_UPDATE_MASK_PREFIX         (1 << 2)    /*< Mask value to signal updating radio prefixes. */


// Function to do bytewise bit-swap on an unsigned 32-bit value
static uint32_t bytewise_bit_swap(uint8_t const * p_inp)
{
#if __CORTEX_M == (0x04U)
    uint32_t inp = (*(uint32_t*)p_inp);
    return __REV((uint32_t)__RBIT(inp)); //lint -esym(628, __rev) -esym(526, __rev) -esym(628, __rbit) -esym(526, __rbit) */
#else
    uint32_t inp = (p_inp[3] << 24) | (p_inp[2] << 16) | (p_inp[1] << 8) | (p_inp[0]);
    inp = (inp & 0xF0F0F0F0) >> 4 | (inp & 0x0F0F0F0F) << 4;
    inp = (inp & 0xCCCCCCCC) >> 2 | (inp & 0x33333333) << 2;
    inp = (inp & 0xAAAAAAAA) >> 1 | (inp & 0x55555555) << 1;
    return inp;
#endif
}


// Convert a base address from nRF24L format to nRF5 format
static uint32_t addr_conv(uint8_t const* p_addr)
{
    return __REV(bytewise_bit_swap(p_addr)); //lint -esym(628, __rev) -esym(526, __rev) */
}

#ifdef NRF52832_XXAA
static ret_code_t apply_address_workarounds()
{
    if ((NRF_FICR->INFO.VARIANT & 0x0000FF00) == 0x00004200) //Check if the device is an nRF52832 Rev. 1.
    {
        // Workaround for nRF52832 Rev 1 erratas
        //  Set up radio parameters.
        NRF_RADIO->MODECNF0 = (NRF_RADIO->MODECNF0 & ~RADIO_MODECNF0_RU_Msk) | RADIO_MODECNF0_RU_Default << RADIO_MODECNF0_RU_Pos;

        // Workaround for nRF52832 Rev 1 Errata 102 and nRF52832 Rev 1 Errata 106. This will reduce sensitivity by 3dB.
        *((volatile uint32_t *)0x40001774) = (*((volatile uint32_t *)0x40001774) & 0xFFFFFFFE) | 0x01000000;
    }

    if ((NRF_FICR->INFO.VARIANT & 0x0000FF00) == 0x00004500)//Check if the device is an nRF52832 Rev. 2.
    {
        /*
        Workaround for nRF52832 Rev 2 Errata 143
        Check if the most significant bytes of address 0 (including prefix) match those of another address.
        It's recommended to use a unique address 0 since this will avoid the 3dBm penalty incurred from the workaround.
        */
        uint32_t base_address_mask = m_esb_addr.addr_length == 5 ? 0xFFFF0000 : 0xFF000000;

        // Load the two addresses before comparing them to ensure defined ordering of volatile accesses.
        uint32_t addr0 = NRF_RADIO->BASE0 & base_address_mask;
        uint32_t addr1 = NRF_RADIO->BASE1 & base_address_mask;
        if (addr0 == addr1)
        {
            uint32_t prefix0 = NRF_RADIO->PREFIX0 & 0x000000FF;
            uint32_t prefix1 = (NRF_RADIO->PREFIX0 & 0x0000FF00) >> 8;
            uint32_t prefix2 = (NRF_RADIO->PREFIX0 & 0x00FF0000) >> 16;
            uint32_t prefix3 = (NRF_RADIO->PREFIX0 & 0xFF000000) >> 24;
            uint32_t prefix4 = NRF_RADIO->PREFIX1 & 0x000000FF;
            uint32_t prefix5 = (NRF_RADIO->PREFIX1 & 0x0000FF00) >> 8;
            uint32_t prefix6 = (NRF_RADIO->PREFIX1 & 0x00FF0000) >> 16;
            uint32_t prefix7 = (NRF_RADIO->PREFIX1 & 0xFF000000) >> 24;
            
            if (prefix0 == prefix1 || prefix0 == prefix2 || prefix0 == prefix3 || prefix0 == prefix4 || 
                prefix0 == prefix5 || prefix0 == prefix6 || prefix0 == prefix7)
            {
                // This will cause a 3dBm sensitivity loss, avoid using such address combinations if possible.
                *(volatile uint32_t *) 0x40001774 = ((*(volatile uint32_t *) 0x40001774) & 0xfffffffe) | 0x01000000; 
            }
        }
    }
    return NRF_SUCCESS;
}
#endif


uint32_t nrf_esb_init_promiscuous_mode() {
    if (!m_esb_initialized) return NRF_ERROR_INVALID_STATE;

    uint32_t err_code;
    nrf_esb_config_t esb_config = NRF_ESB_PROMISCUOUS_CONFIG;

    esb_config.event_handler = m_event_handler;

    err_code = nrf_esb_init(&esb_config);
    VERIFY_SUCCESS(err_code);

    while (nrf_esb_flush_rx() != NRF_SUCCESS) {}; //assure we have no frames pending, which have been captured in non-PRX_PASSIVE mode and could get mis-interpreted

    VERIFY_SUCCESS(err_code);
    m_config_local.mode = NRF_ESB_MODE_PROMISCOUS;
    return NRF_SUCCESS;
}

uint32_t nrf_esb_init_sniffer_mode() {
    if (!m_esb_initialized) return NRF_ERROR_INVALID_STATE;

    uint32_t err_code;
    
    nrf_esb_config_t esb_config = NRF_ESB_SNIFF_CONFIG;
    esb_config.event_handler = m_event_handler;

    err_code = nrf_esb_init(&esb_config);
    VERIFY_SUCCESS(err_code);


    // if old mode was promiscuous mode, flush RX
    if (m_config_local.mode == NRF_ESB_MODE_PROMISCOUS) {
        while (nrf_esb_flush_rx() != NRF_SUCCESS) {}; //assure we have no frames pending, which have been captured in non-PROMISCOUS mode and could get mis-interpreted
    }
    
    m_config_local.mode = NRF_ESB_MODE_SNIFF;
    return NRF_SUCCESS;
}

uint32_t nrf_esb_init_prx_mode() {
    if (!m_esb_initialized) return NRF_ERROR_INVALID_STATE;

    uint32_t err_code;

    nrf_esb_config_t esb_config = NRF_ESB_DEFAULT_CONFIG;
    esb_config.mode = NRF_ESB_MODE_PRX;
    esb_config.event_handler = m_event_handler;

    err_code = nrf_esb_init(&esb_config);
    VERIFY_SUCCESS(err_code);


    // if old mode was promiscuous mode, flush RX
    if (m_config_local.mode == NRF_ESB_MODE_PROMISCOUS) {
        while (nrf_esb_flush_rx() != NRF_SUCCESS) {}; //assure we have no frames pending, which have been captured in non-PROMISCOUS mode and could get mis-interpreted
    }

    m_config_local.mode = NRF_ESB_MODE_PRX;
    return NRF_SUCCESS;
}

uint32_t nrf_esb_init_ptx_mode() {
    if (!m_esb_initialized) return NRF_ERROR_INVALID_STATE;

    uint32_t err_code;

    nrf_esb_config_t esb_config = NRF_ESB_DEFAULT_CONFIG;
    esb_config.event_handler = m_event_handler;
    esb_config.crc = NRF_ESB_CRC_16BIT;
    esb_config.retransmit_count = 2;
    esb_config.retransmit_delay = 3*250; // enough room to receive a full length ack payload before re-transmit timeout occurs

//    esb_config.retransmit_on_all_channels = true;
//    esb_config.retransmit_on_all_channels_loop_count = 1;


    err_code = nrf_esb_init(&esb_config);
    VERIFY_SUCCESS(err_code);

    m_config_local.mode = NRF_ESB_MODE_PTX;
    return NRF_SUCCESS;
}

uint32_t nrf_esb_init_ptx_stay_rx_mode() {
    if (!m_esb_initialized) return NRF_ERROR_INVALID_STATE;

    uint32_t err_code;

    nrf_esb_config_t esb_config = NRF_ESB_DEFAULT_CONFIG;
    esb_config.event_handler = m_event_handler;
    esb_config.crc = NRF_ESB_CRC_16BIT;
    esb_config.retransmit_count = 1;
    esb_config.retransmit_delay = 250; // enough room to receive a full length ack payload before re-transmit timeout occurs
    esb_config.mode = NRF_ESB_MODE_PTX_STAY_RX;
//    esb_config.retransmit_on_all_channels = true;
//    esb_config.retransmit_on_all_channels_loop_count = 1;


    err_code = nrf_esb_init(&esb_config);
    VERIFY_SUCCESS(err_code);

    // stays in idle mode, as no nrf_esb_start_tx() or nrf_esb_start_rx() was called

    // nrf_esb_start_tx would set on_radio_disable ISR
    // - to on_radio_disabled_tx (if payload needs an ack)
    // - to on_radio_disabled_tx_noack (if payload needs no ack)

    // nrf_esb_start_rx would set
    //    on_radio_disabled = on_radio_disabled_rx;
    //    on_radio_end = on_radio_end_rx;


    return NRF_SUCCESS;
}

nrf_esb_mode_t nrf_esb_get_mode() {
    return m_config_local.mode;
}

uint32_t nrf_esb_set_mode(nrf_esb_mode_t mode) {
    if (m_config_local.mode == mode) {
        return NRF_SUCCESS; //no change
    }

    uint32_t err_code;

    switch (mode) {
        case NRF_ESB_MODE_PRX:
            err_code = nrf_esb_init_prx_mode();
            VERIFY_SUCCESS(err_code);
            break;
        case NRF_ESB_MODE_PTX:
            err_code = nrf_esb_init_ptx_mode();
            VERIFY_SUCCESS(err_code);
            break;
        case NRF_ESB_MODE_PTX_STAY_RX:
            err_code = nrf_esb_init_ptx_stay_rx_mode();
            VERIFY_SUCCESS(err_code);
            break;
        case NRF_ESB_MODE_SNIFF:
            err_code = nrf_esb_init_sniffer_mode();
            VERIFY_SUCCESS(err_code);
            break;
        case NRF_ESB_MODE_PROMISCOUS:
            // promiscous mode always needs (re)init, as Packet Format is changed
            //err_code = radioInitPromiscuousMode();
            err_code = nrf_esb_init_promiscuous_mode();
            VERIFY_SUCCESS(err_code);
            break;
        default:
            NRF_LOG_WARNING("nrf_esb_set_mode unhandled mode %d", mode);
            break;
    }

    return 0;
}


static void update_rf_payload_format_esb_dpl(uint32_t payload_length)
{
#if (NRF_ESB_MAX_PAYLOAD_LENGTH <= 32)
    // Using 6 bits for length
    NRF_RADIO->PCNF0 = (0 << RADIO_PCNF0_S0LEN_Pos) |
                       (6 << RADIO_PCNF0_LFLEN_Pos) |
                       (3 << RADIO_PCNF0_S1LEN_Pos) ;
#else
    // Using 8 bits for length
    NRF_RADIO->PCNF0 = (0 << RADIO_PCNF0_S0LEN_Pos) |
                       (8 << RADIO_PCNF0_LFLEN_Pos) |
                       (3 << RADIO_PCNF0_S1LEN_Pos) ;
#endif
    NRF_RADIO->PCNF1 = (RADIO_PCNF1_WHITEEN_Disabled    << RADIO_PCNF1_WHITEEN_Pos) |
                       (RADIO_PCNF1_ENDIAN_Big          << RADIO_PCNF1_ENDIAN_Pos)  |
                       ((m_esb_addr.addr_length - 1)    << RADIO_PCNF1_BALEN_Pos)   |
                       (0                               << RADIO_PCNF1_STATLEN_Pos) |
                       (NRF_ESB_MAX_PAYLOAD_LENGTH      << RADIO_PCNF1_MAXLEN_Pos);
}


//illegal radio settings
static void update_rf_payload_format_esb_promiscuous(uint32_t payload_length)
{
    /**
     * We could set an illegal address length as done by Travis Goodspeed for nRF24.
     * Unfortunately there aren't settings which interpret all bytes after 
     * address+prefix fields as payload. nRF52 series introduced S0,length and S1 hader
     * fields. For a default shockburst packet with a 9 bit PCF where the 6 MSB bits
     * of PCF represent the packet length, S0,length,S1 are set like this:
     *    s0 field length     = 0 bits
     *    length field length = 6 bits
     *    s1 field length     = 3 bits
     * As we want to interpret everything as payload, our settings would have to look
     * like this (with CRC check disabled and static length):
     *    s0 field length     = 0 bits
     *    length field length = 0 bits
     *    s1 field length     = 0 bits
     * Unfortunately, received payloads are missing the first two bytes if settings are chosen
     * like this (and an RF frame matches the address+prefix). It seems, S1 consumes a whole
     * payload byte, which could be reproduced by enabling S1INCL (includes S1 in from radio
     * in RAM). Still one byte is missing, which is likely consumed by S0 (specs state that
     * setting S0LEN to 0 still consumes a single bit, but as we know S1 consumes 8 bits
     * if set to S1LEN of 0 bits). In contrast to the stating in the specs, the PCNF0 register
     * doesn't have a S0INCL flag (only S1INCL, which we already use).
     * 
     * Additionally setting the address with illegal length of 1 byte to the value 0x00 doesn't
     * work.
     * 
     * To sum up:
     * An illegal address+prefix like 0x00+0xaa or 0x00+0x55 couldn't be used. Based on my observations
     * nRF24 radios produce other patterns during ramp up, which could be used, f.e. 0xaf+0xaa.
     * Anyways, we wouldn't be able to recover the first byte following the prefix.
     * 
     * Example:
     * ---------
     * 
     * RX address: 0xaf (illegal length of one byte)
     * RX prefix:  0xaa
     * 
     * On Air frame: 0x00 0x0f 0xaf 0xaa 0x01 0x02 0x03 0x04 0x11 0x20 0x21 0x30 0x30 ... 0x40
     *                         ^-- triggers on chosen address + preamble
     *                              ^-- real preamble
     *                                   ^-- 4 byte real RF address
     *                                                       ^-- real prefix
     *                                                            ^-- 9bit PCF
     *                                                                      ^-- real payload (shl 1)
     * 
     * Resulting payload delivered by radio stack (static length):
     *               0x02 0x03 0x04 0x11 0x20 0x21 0x30 0x30 ... 0x40
     *           ^-- first byte is missing
     *               ^-- second byte only present with S1INCL enabled
     * 
     * I haven't found valid settings to fetch the missing byte, so the approach was changed to listen
     * for 2 byte of alternating 1 and 0 bits. This happens quite often, but real RF frames don't 
     * start in the beginning of the resulting payload. For an address+prefix combination of 0xaa+0xaa
     * with a real RF address+prefix 0x81 0x02 0x03 0x04+0x05, results look something like this:
     * 
     * aaaaaaaaaaaaaa8102030405...
     * aaaaaaaaaaaaa8102030405... //note mis-alligned by one nibble (shifted by 4 bit)
     * aaaaaaaaabdaaab55faa8102030405...
     * aaaaaaaaa8aa8102030405...
     * aaaaaa9faa8102030405...
     * aa85555554aa8102030405...
     * d555555555554aa8102030405...
     * 01155baaaaaaaaaaaaaaaaaaaaaaaaa8102030405...
     * 
     * So with addr+prefix 0xaa+0xaa we could capture RF frames more or less reliably. The nRF52 allows
     * static  payload length' greater than 32 bytes, so the long byte sequences in the beginning are 
     * less of a problem in terms of capturing full RF frames.
     * I tested other frequently occuring address+prefix combinations for capture (0xaafa, 0x5554, 0xaaf9 etc.),
     * but likelyhood of capturing a frame is rather low, compared to 0xaaaa. 
     * 
     * nRF52 allows two base-addresses, where the second base address alllows up to 7 different prefixes.
     * Thus nice address+prefix combinations for capture could be build like this:
     * 
     *                      addr prefix
     * pipe0 (base addr 0)  0x55 0x54
     * pipe1 (base addr 1)  0xaa 0x1f
     * pipe2 (base addr 1)  0xaa 0x9f
     * pipe3 (base addr 1)  0xaa 0xa8
     * pipe4 (base addr 1)  0xaa 0xaf
     * pipe5 (base addr 1)  0xaa 0xa9
     * pipe6 (base addr 1)  0xaa 0x8f
     * pipe7 (base addr 1)  0xaa 0xaa
     * 
     */



    //PCNF0(PLEN=8, LFLEN=0, S0LEN/S1LEN=0, S1INCL=1, CRCINC=1) PCNF1(STATELEN/MAXLEN>=32, [illegal] BALEN=1) -->  2 address octets for given 2 byte address in payload (one missing)
    NRF_RADIO->PCNF0 = (0 << RADIO_PCNF0_S0LEN_Pos) |
                       (0 << RADIO_PCNF0_LFLEN_Pos) |
                       (0 << RADIO_PCNF0_S1LEN_Pos) |
                       (0 << RADIO_PCNF0_CILEN_Pos) |
                       (0 << RADIO_PCNF0_PLEN_Pos) | //preamble 8 bit
                       (0 << RADIO_PCNF0_S1INCL_Pos) | //without this, one payload byte would be missing, still the byte consumed by s0 isn't there
                       //(1 << 22) | //S0INCL???
                       (0 << RADIO_PCNF0_CRCINC_Pos) ;


    NRF_RADIO->PCNF1 = (RADIO_PCNF1_WHITEEN_Disabled    << RADIO_PCNF1_WHITEEN_Pos) |
                       (RADIO_PCNF1_ENDIAN_Big          << RADIO_PCNF1_ENDIAN_Pos)  |
                       ((1)                             << RADIO_PCNF1_BALEN_Pos)   |
                       (60                              << RADIO_PCNF1_STATLEN_Pos) |
                       (60                              << RADIO_PCNF1_MAXLEN_Pos);
//                       (payload_length                  << RADIO_PCNF1_STATLEN_Pos) |
//                       (payload_length                  << RADIO_PCNF1_MAXLEN_Pos);

}

static const uint8_t promiscuous_base_addr_0[4] = {0xa8, 0xa8, 0xa8, 0xa8}; //only one octet used, as address length will be illegal
static const uint8_t promiscuous_base_addr_1[4] = {0xaa, 0xaa, 0xaa, 0xaa}; //only one octet used, as address length will be illegal
static const uint8_t promiscuous_addr_prefix[8] = {0xaa, 0x1f, 0x9f, 0xa8, 0xaf, 0xa9, 0x8f, 0xaa}; //prefix for pipe 0..7    
static void update_radio_addresses(uint8_t update_mask)
{
    if (m_config_local.protocol != NRF_ESB_PROTOCOL_ESB_PROMISCUOUS) {
        if ((update_mask & NRF_ESB_ADDR_UPDATE_MASK_BASE0) != 0)
        {
            NRF_RADIO->BASE0 = addr_conv(m_esb_addr.base_addr_p0);
        }

        if ((update_mask & NRF_ESB_ADDR_UPDATE_MASK_BASE1) != 0)
        {
            NRF_RADIO->BASE1 = addr_conv(m_esb_addr.base_addr_p1);
        }

        if ((update_mask & NRF_ESB_ADDR_UPDATE_MASK_PREFIX) != 0)
        {
            NRF_RADIO->PREFIX0 = bytewise_bit_swap(&m_esb_addr.pipe_prefixes[0]);
            NRF_RADIO->PREFIX1 = bytewise_bit_swap(&m_esb_addr.pipe_prefixes[4]);
        }
    } else {
        if ((update_mask & NRF_ESB_ADDR_UPDATE_MASK_BASE0) != 0)
        {
            NRF_RADIO->BASE0 = addr_conv(promiscuous_base_addr_0);
        }

        if ((update_mask & NRF_ESB_ADDR_UPDATE_MASK_BASE1) != 0)
        {
            NRF_RADIO->BASE1 = addr_conv(promiscuous_base_addr_1);
        }

        if ((update_mask & NRF_ESB_ADDR_UPDATE_MASK_PREFIX) != 0)
        {
            NRF_RADIO->PREFIX0 = bytewise_bit_swap(&promiscuous_addr_prefix[0]);
            NRF_RADIO->PREFIX1 = bytewise_bit_swap(&promiscuous_addr_prefix[4]);
        }
    }
}


static void update_radio_tx_power()
{
    NRF_RADIO->TXPOWER = m_config_local.tx_output_power << RADIO_TXPOWER_TXPOWER_Pos;
}


static bool update_radio_bitrate()
{
    NRF_RADIO->MODE = m_config_local.bitrate << RADIO_MODE_MODE_Pos;

    switch (m_config_local.bitrate)
    {
        case NRF_ESB_BITRATE_2MBPS:
#ifdef NRF52_SERIES
        case NRF_ESB_BITRATE_2MBPS_BLE:
#endif
            m_wait_for_ack_timeout_us = RX_WAIT_FOR_ACK_TIMEOUT_US_2MBPS;
            break;

        case NRF_ESB_BITRATE_1MBPS:
            m_wait_for_ack_timeout_us = RX_WAIT_FOR_ACK_TIMEOUT_US_1MBPS;
            break;

#ifdef NRF51
        case NRF_ESB_BITRATE_250KBPS:
            m_wait_for_ack_timeout_us = RX_WAIT_FOR_ACK_TIMEOUT_US_250KBPS;
            break;
#endif
        
        case NRF_ESB_BITRATE_1MBPS_BLE:
            m_wait_for_ack_timeout_us = RX_WAIT_FOR_ACK_TIMEOUT_US_1MBPS_BLE;
            break;

        default:
            // Should not be reached
            return false;
    }
    return true;
}


static bool update_radio_protocol()
{
    switch (m_config_local.protocol)
    {
        case NRF_ESB_PROTOCOL_ESB_DPL:
            update_rf_payload_format = update_rf_payload_format_esb_dpl;
            break;

        case NRF_ESB_PROTOCOL_ESB_PROMISCUOUS:
            update_rf_payload_format = update_rf_payload_format_esb_promiscuous;
            break;

        default:
            // Should not be reached
            return false;
    }

    // assure address update if proto changed
    NRF_LOG_DEBUG("New proto %d, updating rf_addresses", m_config_local.protocol);
    update_radio_addresses(NRF_ESB_ADDR_UPDATE_MASK_BASE0 | NRF_ESB_ADDR_UPDATE_MASK_BASE1 | NRF_ESB_ADDR_UPDATE_MASK_PREFIX);


    return true;
}


static bool update_radio_crc()
{
    switch(m_config_local.crc)
    {
        case NRF_ESB_CRC_16BIT:
            NRF_RADIO->CRCINIT = 0xFFFFUL;      // Initial value
            NRF_RADIO->CRCPOLY = 0x11021UL;     // CRC poly: x^16+x^12^x^5+1
            break;
        
        case NRF_ESB_CRC_8BIT:
            NRF_RADIO->CRCINIT = 0xFFUL;        // Initial value
            NRF_RADIO->CRCPOLY = 0x107UL;       // CRC poly: x^8+x^2^x^1+1
            break;
        
        case NRF_ESB_CRC_OFF:
            break;
        
        default:
            return false;
    }
    NRF_RADIO->CRCCNF = m_config_local.crc << RADIO_CRCCNF_LEN_Pos;
    return true;
}


static bool update_radio_parameters()
{
    bool params_valid = true;
    update_radio_tx_power();
    params_valid &= update_radio_bitrate();
    params_valid &= update_radio_protocol();
    params_valid &= update_radio_crc();
    update_rf_payload_format(m_config_local.payload_length);
    params_valid &= (m_config_local.retransmit_delay >= NRF_ESB_RETRANSMIT_DELAY_MIN);
    return params_valid;
}


static void reset_fifos()
{
    m_tx_fifo.entry_point = 0;
    m_tx_fifo.exit_point  = 0;
    m_tx_fifo.count       = 0;

//    WAIT_UNLOCK_RX_FIFO;
//    UNLOCK_RX_FIFO;
    m_rx_fifo.entry_point = 0;
    m_rx_fifo.exit_point  = 0;
    m_rx_fifo.count       = 0;
//    LOCK_RX_FIFO;
}


static void initialize_fifos()
{
    reset_fifos();

    for (int i = 0; i < NRF_ESB_TX_FIFO_SIZE; i++)
    {
        m_tx_fifo.p_payload[i] = &m_tx_fifo_payload[i];
    }

    for (int i = 0; i < NRF_ESB_RX_FIFO_SIZE; i++)
    {
        m_rx_fifo.p_payload[i] = &m_rx_fifo_payload[i];
    }
}


uint32_t nrf_esb_skip_tx()
{
    VERIFY_TRUE(m_esb_initialized, NRF_ERROR_INVALID_STATE);
    VERIFY_TRUE(m_tx_fifo.count > 0, NRF_ERROR_BUFFER_EMPTY);

    DISABLE_RF_IRQ();

    m_tx_fifo.count--;
    if (++m_tx_fifo.exit_point >= NRF_ESB_TX_FIFO_SIZE)
    {
        m_tx_fifo.exit_point = 0;
    }

    ENABLE_RF_IRQ();

    return NRF_SUCCESS;
}


void nrf_esb_exec_promiscuous_payload_validation(void *p_event_data, uint16_t event_size) {
    APP_ERROR_CHECK_BOOL(event_size == sizeof(nrf_esb_payload_t));
    nrf_esb_payload_t * p_rx_payload_promiscuous_unvalidated = (nrf_esb_payload_t *)p_event_data;

    if (nrf_esb_validate_promiscuous_esb_payload(p_rx_payload_promiscuous_unvalidated) == NRF_SUCCESS) {
//        WAIT_UNLOCK_RX_FIFO;
//        LOCK_RX_FIFO;

        // push data to RX fifo and generate event for RX 
        if (m_rx_fifo.count < NRF_ESB_RX_FIFO_SIZE) {
            //push frame to queue
            //NRF_LOG_DEBUG("Enqueueing valid pay at entry point %d", m_rx_fifo.entry_point);
            memcpy(m_rx_fifo.p_payload[m_rx_fifo.entry_point], p_rx_payload_promiscuous_unvalidated, sizeof(nrf_esb_payload_t));
            m_rx_fifo.p_payload[m_rx_fifo.entry_point]->validated_promiscuous_frame  = true;
            //adjust queue
            if (++m_rx_fifo.entry_point >= NRF_ESB_RX_FIFO_SIZE) m_rx_fifo.entry_point = 0;
            m_rx_fifo.count++;

            m_interrupt_flags |= NRF_ESB_INT_RX_DATA_RECEIVED_MSK;
            NVIC_SetPendingIRQ(ESB_EVT_IRQ);
        } else {
            NRF_LOG_WARNING("can not re-enqueue valid frame, because RX fifo is full")
        }
//        UNLOCK_RX_FIFO;
    }
}

/** @brief  Function to push the content of the rx_buffer to the RX FIFO.
 *
 *  The module will point the register NRF_RADIO->PACKETPTR to a buffer for receiving packets.
 *  After receiving a packet the module will call this function to copy the received data to
 *  the RX FIFO.
 *
 *  @param  pipe Pipe number to set for the packet.
 *  @param  pid  Packet ID.
 *
 *  @retval true   Operation successful.
 *  @retval false  Operation failed.
 */
static nrf_esb_payload_t m_tmp_payload;
static bool schedule_frame_for_validation_before_pushing_to_rx_fifo(uint8_t pipe, uint8_t pid)
{
    if (m_config_local.mode != NRF_ESB_MODE_PROMISCOUS) return false;
    m_tmp_payload.length = m_config_local.payload_length;

    uint8_t skip = 0;
    while ((60-skip) > 32) {
        if (m_rx_payload_buffer[skip] == 0xaa && m_rx_payload_buffer[skip+1] == 0xaa) {
            skip++; //keep the first 0xaa byte, as it could contain the first address bit
        } else {
            break;
        }
    }

    m_tmp_payload.length = 60-skip;
    memcpy(m_tmp_payload.data, &m_rx_payload_buffer[skip], m_tmp_payload.length);


    m_tmp_payload.pipe  = pipe;
    m_tmp_payload.rssi  = NRF_RADIO->RSSISAMPLE;
    m_tmp_payload.pid   = pid;
    m_tmp_payload.rx_channel_index = m_esb_addr.rf_channel;
    m_tmp_payload.rx_channel = m_esb_addr.channel_to_frequency[m_esb_addr.rf_channel];
    m_tmp_payload.noack = !(m_rx_payload_buffer[1] & 0x01);
    m_tmp_payload.validated_promiscuous_frame = false;

    //send copy of frame to app_scheduler for CRC16 based frame validation (with bitshifting through data to increase chance of hit)
    uint32_t err = app_sched_event_put(&m_tmp_payload, sizeof(nrf_esb_payload_t), nrf_esb_exec_promiscuous_payload_validation); //ignore error
    if (err != NRF_SUCCESS) 
    {
        if (err == NRF_ERROR_NO_MEM) {
            NRF_LOG_WARNING("can't schedule promiscuous mode data for validation ... processing queue full (data arriving too fast)");
            return false;
        }

        NRF_LOG_WARNING("error scheduling event %d", err);
        return false;
    } else {
        return true;
    }
}


/** @brief  Function to push the content of the rx_buffer to the RX FIFO.
 *
 *  The module will point the register NRF_RADIO->PACKETPTR to a buffer for receiving packets.
 *  After receiving a packet the module will call this function to copy the received data to
 *  the RX FIFO.
 *
 *  @param  pipe Pipe number to set for the packet.
 *  @param  pid  Packet ID.
 *
 *  @retval true   Operation successful.
 *  @retval false  Operation failed.
 */
static bool rx_fifo_push_rfbuf(uint8_t pipe, uint8_t pid)
{
//    WAIT_UNLOCK_RX_FIFO;
//    LOCK_RX_FIFO;

    if (m_rx_fifo.count < NRF_ESB_RX_FIFO_SIZE)
    {
        if (m_config_local.protocol == NRF_ESB_PROTOCOL_ESB_DPL)
        {
            if (m_rx_payload_buffer[0] > NRF_ESB_MAX_PAYLOAD_LENGTH)
            {
//                UNLOCK_RX_FIFO;
                return false;
            }

            m_rx_fifo.p_payload[m_rx_fifo.entry_point]->length = m_rx_payload_buffer[0];
        }
        else if (m_config_local.mode == NRF_ESB_MODE_PTX)
        {
            // Received packet is an acknowledgment
            m_rx_fifo.p_payload[m_rx_fifo.entry_point]->length = 0;
        }
        else
        {
            m_rx_fifo.p_payload[m_rx_fifo.entry_point]->length = m_config_local.payload_length;
        }

        if (m_config_local.protocol == NRF_ESB_PROTOCOL_ESB_PROMISCUOUS) {
            uint8_t skip = 0;
            while ((60-skip) > 32) {
                if (m_rx_payload_buffer[skip] == 0xaa && m_rx_payload_buffer[skip+1] == 0xaa) {
                    skip++; //keep the first 0xaa byte, as it could contain the first address bit
                } else {
                    break;
                }
            }

            m_rx_fifo.p_payload[m_rx_fifo.entry_point]->length = 60-skip;
            memcpy(m_rx_fifo.p_payload[m_rx_fifo.entry_point]->data, &m_rx_payload_buffer[skip],
                   m_rx_fifo.p_payload[m_rx_fifo.entry_point]->length);

        } else {
            memcpy(m_rx_fifo.p_payload[m_rx_fifo.entry_point]->data, &m_rx_payload_buffer[2],
                   m_rx_fifo.p_payload[m_rx_fifo.entry_point]->length);
        }

        m_rx_fifo.p_payload[m_rx_fifo.entry_point]->pipe  = pipe;
        m_rx_fifo.p_payload[m_rx_fifo.entry_point]->rssi  = NRF_RADIO->RSSISAMPLE;
        m_rx_fifo.p_payload[m_rx_fifo.entry_point]->pid   = pid;
        m_rx_fifo.p_payload[m_rx_fifo.entry_point]->rx_channel_index = m_esb_addr.rf_channel;
        m_rx_fifo.p_payload[m_rx_fifo.entry_point]->rx_channel = m_esb_addr.channel_to_frequency[ m_esb_addr.rf_channel];
        m_rx_fifo.p_payload[m_rx_fifo.entry_point]->noack = !(m_rx_payload_buffer[1] & 0x01);
        m_rx_fifo.p_payload[m_rx_fifo.entry_point]->validated_promiscuous_frame = false;

/*
        if (NRF_ESB_CHECK_PROMISCUOUS_CRC_WITH_APP_SCHEDULER && m_config_local.mode == NRF_ESB_MODE_PROMISCOUS) {
            //send copy of frame to app_scheduler for CRC16 based frame validation (with bitshifting through data to increase chance of hit)
            uint32_t err = app_sched_event_put(m_rx_fifo.p_payload[m_rx_fifo.entry_point], sizeof(nrf_esb_payload_t), nrf_esb_exec_promiscuous_payload_validation); //ignore error
            if (err != NRF_SUCCESS) NRF_LOG_WARNING("error scheduling event %d", err);
        }
*/
        if (++m_rx_fifo.entry_point >= NRF_ESB_RX_FIFO_SIZE)
        {
            m_rx_fifo.entry_point = 0;
        }
        m_rx_fifo.count++;

//        UNLOCK_RX_FIFO;
        return true;
    }
//    UNLOCK_RX_FIFO;
    return false;
}


static void sys_timer_init()
{
    // Configure the system timer with a 1 MHz base frequency
    NRF_ESB_SYS_TIMER->PRESCALER = 4;
    NRF_ESB_SYS_TIMER->BITMODE   = TIMER_BITMODE_BITMODE_16Bit;
    NRF_ESB_SYS_TIMER->SHORTS    = TIMER_SHORTS_COMPARE1_CLEAR_Msk | TIMER_SHORTS_COMPARE1_STOP_Msk;
}


static void ppi_init()
{
    NRF_PPI->CH[NRF_ESB_PPI_TIMER_START].EEP = (uint32_t)&NRF_RADIO->EVENTS_READY;
    NRF_PPI->CH[NRF_ESB_PPI_TIMER_START].TEP = (uint32_t)&NRF_ESB_SYS_TIMER->TASKS_START;

    NRF_PPI->CH[NRF_ESB_PPI_TIMER_STOP].EEP  = (uint32_t)&NRF_RADIO->EVENTS_ADDRESS;
    NRF_PPI->CH[NRF_ESB_PPI_TIMER_STOP].TEP  = (uint32_t)&NRF_ESB_SYS_TIMER->TASKS_SHUTDOWN;

    NRF_PPI->CH[NRF_ESB_PPI_RX_TIMEOUT].EEP  = (uint32_t)&NRF_ESB_SYS_TIMER->EVENTS_COMPARE[0];
    NRF_PPI->CH[NRF_ESB_PPI_RX_TIMEOUT].TEP  = (uint32_t)&NRF_RADIO->TASKS_DISABLE;

    NRF_PPI->CH[NRF_ESB_PPI_TX_START].EEP    = (uint32_t)&NRF_ESB_SYS_TIMER->EVENTS_COMPARE[1];
    NRF_PPI->CH[NRF_ESB_PPI_TX_START].TEP    = (uint32_t)&NRF_RADIO->TASKS_TXEN;
}


static void start_tx_transaction()
{
    bool ack;

    m_last_tx_attempts = 1;
    // Prepare the payload
    mp_current_payload = m_tx_fifo.p_payload[m_tx_fifo.exit_point];

    if (m_config_local.mode == NRF_ESB_MODE_PTX_STAY_RX) {
        nrf_esb_stop_rx();
    }

    on_radio_end = NULL;

    switch (m_config_local.protocol)
    {

        case NRF_ESB_PROTOCOL_ESB_DPL:
        
            ack = !mp_current_payload->noack || !m_config_local.selective_auto_ack;
            m_tx_payload_buffer[0] = mp_current_payload->length;
            m_tx_payload_buffer[1] = mp_current_payload->pid << 1;
            // note: noack shouldn't be negated befor OR'ing onto S1 field
            //m_tx_payload_buffer[1] |= mp_current_payload->noack ? 0x00 : 0x01;
            m_tx_payload_buffer[1] |= mp_current_payload->noack ? 0x01 : 0x00;
            memcpy(&m_tx_payload_buffer[2], mp_current_payload->data, mp_current_payload->length);

            // Handling ack if noack is set to false or if selective auto ack is turned off
            if (ack)
            {
                NRF_RADIO->SHORTS   = m_radio_shorts_common | RADIO_SHORTS_DISABLED_RXEN_Msk;
                NRF_RADIO->INTENSET = RADIO_INTENSET_DISABLED_Msk | RADIO_INTENSET_READY_Msk;

                //Configure retransmit with channel hopping
                if (m_config_local.retransmit_on_all_channels) {
                    // maximum channel hop count is size of channel table * loop count (only set for first transmission)
                    if (!m_retransmit_all_channels_running_channel_sweep) {
                        m_retransmit_all_channels_remaining_channel_hop_count = (uint32_t) m_config_local.retransmit_on_all_channels_loop_count *  m_esb_addr.channel_to_frequency_len;
                        m_retransmit_all_channels_running_channel_sweep = true;
                    }
                } else {
                    m_retransmit_all_channels_remaining_channel_hop_count = 0;
                }
                

                // Configure the retransmit counter
                m_retransmits_remaining = m_config_local.retransmit_count;
                on_radio_disabled = on_radio_disabled_tx;
                m_nrf_esb_mainstate = NRF_ESB_STATE_PTX_TX_ACK;
            }
            else
            {
                NRF_RADIO->SHORTS   = m_radio_shorts_common;
                NRF_RADIO->INTENSET = RADIO_INTENSET_DISABLED_Msk;
                on_radio_disabled   = on_radio_disabled_tx_noack;
                m_nrf_esb_mainstate = NRF_ESB_STATE_PTX_TX;
            }
            break;

        default:
            // Should not be reached
            break;
    }

    NRF_RADIO->TXADDRESS    = mp_current_payload->pipe;
    NRF_RADIO->RXADDRESSES  = 1 << mp_current_payload->pipe;

    //NRF_RADIO->FREQUENCY    = m_esb_addr.rf_channel;
    NRF_RADIO->FREQUENCY    = m_esb_addr.channel_to_frequency[m_esb_addr.rf_channel];
    NRF_RADIO->PACKETPTR    = (uint32_t)m_tx_payload_buffer;

    NVIC_ClearPendingIRQ(RADIO_IRQn);
    NVIC_EnableIRQ(RADIO_IRQn);

    NRF_RADIO->EVENTS_ADDRESS = 0;
    NRF_RADIO->EVENTS_PAYLOAD = 0;
    NRF_RADIO->EVENTS_DISABLED = 0;

    NRF_RADIO->TASKS_TXEN  = 1;
}


static void on_radio_disabled_tx_noack()
{
//    NRF_LOG_INFO("NRF_ESB_INT_TX_SUCCESS_MSK in on_radio_disabled_tx_noack");
    m_interrupt_flags |= NRF_ESB_INT_TX_SUCCESS_MSK;
    (void) nrf_esb_skip_tx();

    if (m_tx_fifo.count == 0)
    {
        m_nrf_esb_mainstate = NRF_ESB_STATE_IDLE;
        NVIC_SetPendingIRQ(ESB_EVT_IRQ);
    }
    else
    {
        NVIC_SetPendingIRQ(ESB_EVT_IRQ);
        start_tx_transaction();
    }
}

//Called after disable of initial transmission
static void on_radio_disabled_tx()
{
    // Remove the DISABLED -> RXEN shortcut, to make sure the radio stays
    // disabled after the RX window
    NRF_RADIO->SHORTS = m_radio_shorts_common;

    // Make sure the timer is started the next time the radio is ready,
    // and that it will disable the radio automatically if no packet is
    // received by the time defined in m_wait_for_ack_timeout_us
    NRF_ESB_SYS_TIMER->CC[0]    = m_wait_for_ack_timeout_us;
    NRF_ESB_SYS_TIMER->CC[1]    = m_config_local.retransmit_delay - 130;
    NRF_ESB_SYS_TIMER->TASKS_CLEAR = 1;
    NRF_ESB_SYS_TIMER->EVENTS_COMPARE[0] = 0;
    NRF_ESB_SYS_TIMER->EVENTS_COMPARE[1] = 0;

    NRF_PPI->CHENSET            = (1 << NRF_ESB_PPI_TIMER_START) |
                                  (1 << NRF_ESB_PPI_RX_TIMEOUT) |
                                  (1 << NRF_ESB_PPI_TIMER_STOP);
    NRF_PPI->CHENCLR            = (1 << NRF_ESB_PPI_TX_START);
    NRF_RADIO->EVENTS_END       = 0;


    NRF_RADIO->PACKETPTR        = (uint32_t)m_rx_payload_buffer;
    if (m_config_local.mode == NRF_ESB_MODE_PTX_STAY_RX) {
        on_radio_disabled           = on_radio_disabled_tx_wait_for_ack_continue_rx;
        m_nrf_esb_mainstate         = NRF_ESB_STATE_PTX_RX_ACK;
    } else {
        on_radio_disabled           = on_radio_disabled_tx_wait_for_ack;
        m_nrf_esb_mainstate         = NRF_ESB_STATE_PTX_RX_ACK;
    }


}

// called after disable for the RX immediately following a TX which awaits an ACK
// disable could be tasked by timer via PPI (if no ADDRESS event occurs before m_wait_for_ack_timeout_us)
// or could happen, if a full ack payload is received in RX mode (END event before disable)
static void on_radio_disabled_tx_wait_for_ack_continue_rx()
{
    // This marks the completion of a TX_RX sequence (TX with ACK)

    // deactivate all enabled PPI channels
    // Make sure the timer will not deactivate the radio while ACK packet is received
    NRF_PPI->CHENCLR = (1 << NRF_ESB_PPI_TIMER_START) |
                       (1 << NRF_ESB_PPI_RX_TIMEOUT)  |
                       (1 << NRF_ESB_PPI_TIMER_STOP);

    // If the radio has received a packet and the CRC status is OK
    if (NRF_RADIO->EVENTS_END && NRF_RADIO->CRCSTATUS != 0)
    {
        //m_retransmit_all_channels_running_channel_sweep = false;

        NRF_ESB_SYS_TIMER->TASKS_SHUTDOWN = 1; // stop timer for re-transmit
        NRF_PPI->CHENCLR = (1 << NRF_ESB_PPI_TX_START); //stop PPI channel, which tasks TXEN after TIMER->EVENTS_COMPARE[1] (which is retransmit delay)
        m_interrupt_flags |= NRF_ESB_INT_TX_SUCCESS_MSK; //prepare Soft IRQ to report TX_SUCCESS
        m_last_tx_attempts = m_config_local.retransmit_count - m_retransmits_remaining + 1;

        (void) nrf_esb_skip_tx(); //pop TX payload from TX queue
        m_retransmit_all_channels_remaining_channel_hop_count = 0; // reset multi_channel retransmit counter
        m_retransmit_all_channels_running_channel_sweep = false;

        if (m_rx_payload_buffer[0] > 0)
        {
            if (rx_fifo_push_rfbuf((uint8_t)NRF_RADIO->TXADDRESS, m_rx_payload_buffer[1] >> 1))
            {
//                NRF_LOG_INFO("pushed ack RF frame to FIFO");
                m_interrupt_flags |= NRF_ESB_INT_RX_DATA_RECEIVED_MSK;
            }
        }

        if ((m_tx_fifo.count == 0) || (m_config_local.tx_mode == NRF_ESB_TXMODE_MANUAL))
        {
            // return to IDLE if TX queue is empty (or TX mode isn't auto), but if we are in PTX_STAY_RX, we start PRX
            // mode

            m_nrf_esb_mainstate = NRF_ESB_STATE_IDLE;
            NVIC_SetPendingIRQ(ESB_EVT_IRQ);


            if (m_config_local.mode == NRF_ESB_MODE_PTX_STAY_RX) {
                // move from disable to RX mode
                nrf_esb_start_rx();
            }
        }
        else
        {
            NVIC_SetPendingIRQ(ESB_EVT_IRQ);
            start_tx_transaction();
        }
    }
    else
    {
        // if here, TX end was reached, but no ack arrived in time

        if (m_retransmits_remaining-- == 0)
        {
            bool tx_failed = true;
            // no retransmissions left
            if (m_config_local.retransmit_on_all_channels) {
                if (m_retransmit_all_channels_remaining_channel_hop_count > 0) tx_failed=false;
            }

            if (tx_failed) {
                NRF_LOG_DEBUG("tx fail: r-tx count %d, r-tx on %d", m_retransmit_all_channels_remaining_channel_hop_count, m_config_local.retransmit_on_all_channels)

                m_retransmit_all_channels_running_channel_sweep = false;
                NRF_ESB_SYS_TIMER->TASKS_SHUTDOWN = 1;
                NRF_PPI->CHENCLR = (1 << NRF_ESB_PPI_TX_START);
                // All retransmits are expended, and the TX operation is suspended
                m_last_tx_attempts = m_config_local.retransmit_count + 1;
                m_interrupt_flags |= NRF_ESB_INT_TX_FAILED_MSK;

                m_nrf_esb_mainstate = NRF_ESB_STATE_IDLE;
                NVIC_SetPendingIRQ(ESB_EVT_IRQ);
            } else {
                // transfer state back to idle
                m_nrf_esb_mainstate = NRF_ESB_STATE_IDLE;
                //jump to next channel
                nrf_esb_set_rf_channel_next();
                // decrease remaining channel hops
                m_retransmit_all_channels_remaining_channel_hop_count--;
                NRF_LOG_DEBUG("Re-TX on channel %d, remaining hops %d", m_esb_addr.channel_to_frequency[m_esb_addr.rf_channel], m_retransmit_all_channels_remaining_channel_hop_count);
                // restart transmission
                if (nrf_esb_start_tx() != NRF_SUCCESS) tx_failed = true; // we failed restarting TX
            }
        }
        else
        {
            // There are still more retransmits left, TX mode should be
            // entered again as soon as the system timer reaches CC[1].
            NRF_RADIO->SHORTS = m_radio_shorts_common | RADIO_SHORTS_DISABLED_RXEN_Msk;
            update_rf_payload_format(mp_current_payload->length);
            NRF_RADIO->PACKETPTR = (uint32_t)m_tx_payload_buffer;
            on_radio_disabled = on_radio_disabled_tx;
            m_nrf_esb_mainstate = NRF_ESB_STATE_PTX_TX_ACK;
            NRF_ESB_SYS_TIMER->TASKS_START = 1;
            NRF_PPI->CHENSET = (1 << NRF_ESB_PPI_TX_START);
            if (NRF_ESB_SYS_TIMER->EVENTS_COMPARE[1])
            {
                NRF_RADIO->TASKS_TXEN = 1;
            }
        }
    }
}

static void on_radio_disabled_tx_wait_for_ack()
{
    // This marks the completion of a TX_RX sequence (TX with ACK)

    // Make sure the timer will not deactivate the radio if a packet is received
    NRF_PPI->CHENCLR = (1 << NRF_ESB_PPI_TIMER_START) |
                       (1 << NRF_ESB_PPI_RX_TIMEOUT)  |
                       (1 << NRF_ESB_PPI_TIMER_STOP);

    // If the radio has received a packet and the CRC status is OK
    if (NRF_RADIO->EVENTS_END && NRF_RADIO->CRCSTATUS != 0)
    {
        m_retransmit_all_channels_running_channel_sweep = false;

        NRF_ESB_SYS_TIMER->TASKS_SHUTDOWN = 1;
        NRF_PPI->CHENCLR = (1 << NRF_ESB_PPI_TX_START);
//        NRF_LOG_INFO("NRF_ESB_INT_TX_SUCCESS_MSK in on_radio_disabled_tx_wait_for_ack");
        m_interrupt_flags |= NRF_ESB_INT_TX_SUCCESS_MSK;
        m_last_tx_attempts = m_config_local.retransmit_count - m_retransmits_remaining + 1;

        (void) nrf_esb_skip_tx();
        m_retransmit_all_channels_remaining_channel_hop_count = 0; // reset multi_channel retransmit counter
        m_retransmit_all_channels_running_channel_sweep = false;

        if (m_rx_payload_buffer[0] > 0)
        {
            if (rx_fifo_push_rfbuf((uint8_t)NRF_RADIO->TXADDRESS, m_rx_payload_buffer[1] >> 1))
            {
//                NRF_LOG_INFO("pushed ack RF frame to FIFO");
//                NRF_LOG_INFO("DATA_RECEIVED_MSK in on_radio_disabled_tx_wait_for_ack");
                m_interrupt_flags |= NRF_ESB_INT_RX_DATA_RECEIVED_MSK;
            }
        }

        if ((m_tx_fifo.count == 0) || (m_config_local.tx_mode == NRF_ESB_TXMODE_MANUAL))
        {
            m_nrf_esb_mainstate = NRF_ESB_STATE_IDLE;
            NVIC_SetPendingIRQ(ESB_EVT_IRQ);
        }
        else
        {
            NVIC_SetPendingIRQ(ESB_EVT_IRQ);
            start_tx_transaction();
        }
    }
    else
    {
        // if here, TX end was reached, but no ack arrived in time

        if (m_retransmits_remaining-- == 0)
        {
            bool tx_failed = true;
            // no retransmissions left
            if (m_config_local.retransmit_on_all_channels) {
                if (m_retransmit_all_channels_remaining_channel_hop_count > 0) tx_failed=false;                
            } 

            if (tx_failed) {
                NRF_LOG_DEBUG("tx fail: r-tx count %d, r-tx on %d", m_retransmit_all_channels_remaining_channel_hop_count, m_config_local.retransmit_on_all_channels)

                m_retransmit_all_channels_running_channel_sweep = false;
                NRF_ESB_SYS_TIMER->TASKS_SHUTDOWN = 1;
                NRF_PPI->CHENCLR = (1 << NRF_ESB_PPI_TX_START);
                // All retransmits are expended, and the TX operation is suspended
                m_last_tx_attempts = m_config_local.retransmit_count + 1;
                m_interrupt_flags |= NRF_ESB_INT_TX_FAILED_MSK;

                m_nrf_esb_mainstate = NRF_ESB_STATE_IDLE;
                NVIC_SetPendingIRQ(ESB_EVT_IRQ);
            } else {
                // transfer state back to idle
                m_nrf_esb_mainstate = NRF_ESB_STATE_IDLE;
                //jump to next channel
                nrf_esb_set_rf_channel_next();
                // decrease remaining channel hops
                m_retransmit_all_channels_remaining_channel_hop_count--;
                NRF_LOG_DEBUG("Re-TX on channel %d, remaining hops %d", m_esb_addr.channel_to_frequency[m_esb_addr.rf_channel], m_retransmit_all_channels_remaining_channel_hop_count);
                // restart transmission
                if (nrf_esb_start_tx() != NRF_SUCCESS) tx_failed = true; // we failed restarting TX
            }
        }
        else
        {
            // There are still more retransmits left, TX mode should be
            // entered again as soon as the system timer reaches CC[1].
            NRF_RADIO->SHORTS = m_radio_shorts_common | RADIO_SHORTS_DISABLED_RXEN_Msk;
            update_rf_payload_format(mp_current_payload->length);
            NRF_RADIO->PACKETPTR = (uint32_t)m_tx_payload_buffer;
            on_radio_disabled = on_radio_disabled_tx;
            m_nrf_esb_mainstate = NRF_ESB_STATE_PTX_TX_ACK;
            NRF_ESB_SYS_TIMER->TASKS_START = 1;
            NRF_PPI->CHENSET = (1 << NRF_ESB_PPI_TX_START);
            if (NRF_ESB_SYS_TIMER->EVENTS_COMPARE[1])
            {
                NRF_RADIO->TASKS_TXEN = 1;
            }
        }
    }
}


static void clear_events_restart_rx(void)
{
    NRF_RADIO->SHORTS = m_radio_shorts_common;
    update_rf_payload_format(m_config_local.payload_length);
    NRF_RADIO->PACKETPTR = (uint32_t)m_rx_payload_buffer;
    NRF_RADIO->EVENTS_DISABLED = 0;
    NRF_RADIO->TASKS_DISABLE = 1;

    while (NRF_RADIO->EVENTS_DISABLED == 0);

    NRF_RADIO->EVENTS_DISABLED = 0;
    NRF_RADIO->SHORTS = m_radio_shorts_common | RADIO_SHORTS_DISABLED_TXEN_Msk;

    NRF_RADIO->TASKS_RXEN = 1;
}

static void clear_events_do_not_restart_rx(void)
{
    //NRF_RADIO->SHORTS = m_radio_shorts_common;
    update_rf_payload_format(m_config_local.payload_length);
    NRF_RADIO->PACKETPTR = (uint32_t)m_rx_payload_buffer;
    NRF_RADIO->EVENTS_DISABLED = 0;
    NRF_RADIO->TASKS_DISABLE = 1;

    while (NRF_RADIO->EVENTS_DISABLED == 0);

    NRF_RADIO->EVENTS_DISABLED = 0;
    //NRF_RADIO->SHORTS = m_radio_shorts_common | RADIO_SHORTS_DISABLED_TXEN_Msk;

    NRF_RADIO->TASKS_RXEN = 1;
}

static void on_radio_disabled_rx(void)
{
    NRF_LOG_INFO("on_rx_disabled");
    bool            ack                = false;
    bool            retransmit_payload = false;
    bool            send_rx_event      = true;
    pipe_info_t *   p_pipe_info;

    uint8_t field_length = m_rx_payload_buffer[0];
    uint8_t field_s1 = m_rx_payload_buffer[1];  //bit 2..1: PID, bit 0: no_ack flag

    //if (NRF_RADIO->CRCSTATUS == 0 && !m_config_local.disallow_auto_ack) //consume frames with invalid CRC
    if (NRF_RADIO->CRCSTATUS == 0 && m_config_local.protocol != NRF_ESB_PROTOCOL_ESB_PROMISCUOUS)
    {
        if (m_config_local.mode == NRF_ESB_MODE_SNIFF) clear_events_do_not_restart_rx();
        else clear_events_restart_rx();
        return;
    }

    if (m_rx_fifo.count >= NRF_ESB_RX_FIFO_SIZE)
    {
        if (m_config_local.mode == NRF_ESB_MODE_SNIFF || m_config_local.mode == NRF_ESB_MODE_PROMISCOUS) clear_events_do_not_restart_rx();
        else clear_events_restart_rx();
        return;
    }


    p_pipe_info = &m_rx_pipe_info[NRF_RADIO->RXMATCH];
    if (NRF_RADIO->RXCRC == p_pipe_info->crc && (field_s1 >> 1) == p_pipe_info->pid) {
        retransmit_payload = true;
        send_rx_event = false; // if we are in passive rx mode, with auto ack, we fire a receive event, even on pipe mismatch (to collect ACK payloads from real PRX)
    }

    p_pipe_info->pid = field_s1 >> 1;
    p_pipe_info->crc = NRF_RADIO->RXCRC;
    if ((m_config_local.selective_auto_ack == false) || ((field_s1 & 0x01) == 1))
    {
        ack = true;
    }


    if (m_config_local.disallow_auto_ack || m_config_local.mode == NRF_ESB_MODE_SNIFF || m_config_local.mode == NRF_ESB_MODE_PROMISCOUS) ack = false;
    if (m_config_local.mode == NRF_ESB_MODE_SNIFF || m_config_local.mode == NRF_ESB_MODE_PROMISCOUS) {
        // in sniffer and promiscuous mode, we want the radio to immediatley restart RX (start_end short)
        // don't use END->DISABLE shortcut, but END->start
        NRF_RADIO->SHORTS = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_ADDRESS_RSSISTART_Msk | RADIO_SHORTS_DISABLED_RSSISTOP_Msk | RADIO_SHORTS_END_START_Msk | RADIO_SHORTS_DISABLED_RXEN_Msk;
    } 

    if (ack) {
        NRF_RADIO->SHORTS = m_radio_shorts_common | RADIO_SHORTS_DISABLED_RXEN_Msk;

        switch (m_config_local.protocol)
        {
            case NRF_ESB_PROTOCOL_ESB_DPL:
                {
                    if (m_tx_fifo.count > 0 &&
                        (m_tx_fifo.p_payload[m_tx_fifo.exit_point]->pipe == NRF_RADIO->RXMATCH)
                       )
                    {
                        // Pipe stays in ACK with payload until TX FIFO is empty
                        // Do not report TX success on first ack payload or retransmit
                        if (p_pipe_info->ack_payload == true && !retransmit_payload)
                        {
                            if (++m_tx_fifo.exit_point >= NRF_ESB_TX_FIFO_SIZE)
                            {
                                m_tx_fifo.exit_point = 0;
                            }

                            m_tx_fifo.count--;

                            // ACK payloads also require TX_DS
                            // (page 40 of the 'nRF24LE1_Product_Specification_rev1_6.pdf').
//                            NRF_LOG_INFO("NRF_ESB_INT_TX_SUCCESS_MSK in on_radio_disabled_rx");
                            m_interrupt_flags |= NRF_ESB_INT_TX_SUCCESS_MSK;
                        }

                        p_pipe_info->ack_payload = true;

                        mp_current_payload = m_tx_fifo.p_payload[m_tx_fifo.exit_point];

                        update_rf_payload_format(mp_current_payload->length);
                        m_tx_payload_buffer[0] = mp_current_payload->length;
                        memcpy(&m_tx_payload_buffer[2],
                               mp_current_payload->data,
                               mp_current_payload->length);
                    }
                    else
                    {
                        p_pipe_info->ack_payload = false;
                        update_rf_payload_format(0);
                        m_tx_payload_buffer[0] = 0;
                    }

                    m_tx_payload_buffer[1] = field_s1;
                }
                break;

            // should never happen, we don't send ACKs in promiscuous mode               
            default:
                {
                    update_rf_payload_format(0);
                    m_tx_payload_buffer[0] = field_length;
                    m_tx_payload_buffer[1] = 0;
                }
                break;
        }

        m_nrf_esb_mainstate = NRF_ESB_STATE_PRX_SEND_ACK;
        NRF_RADIO->TXADDRESS = NRF_RADIO->RXMATCH;
        NRF_RADIO->PACKETPTR = (uint32_t)m_tx_payload_buffer;
        on_radio_disabled = on_radio_disabled_rx_ack;
    }
    else
    {        
        if (!m_config_local.disallow_auto_ack) {
            if (m_config_local.mode == NRF_ESB_MODE_PROMISCOUS || m_config_local.mode == NRF_ESB_MODE_SNIFF) clear_events_do_not_restart_rx();
            else clear_events_restart_rx();
        }    
    }

    if (send_rx_event)
    {
        if (m_config_local.mode == NRF_ESB_MODE_PROMISCOUS && NRF_ESB_CHECK_PROMISCUOUS_CRC_WITH_APP_SCHEDULER) {
            // schedule frame for validation
            schedule_frame_for_validation_before_pushing_to_rx_fifo(NRF_RADIO->RXMATCH, p_pipe_info->pid);
            // if needed, send invalid frames to fifo, too (validated_promiscuous_frame will be false for resulting frame)
            if (NRF_ESB_CHECK_PROMISCUOUS_CRC_WITH_APP_SCHEDULER_ENQUEUE_INVALID) {
                if (rx_fifo_push_rfbuf(NRF_RADIO->RXMATCH, p_pipe_info->pid))
                {
//                    NRF_LOG_INFO("DATA_RECEIVED_MSK in on_radio_disabled_rx promiscuous branch");
                    m_interrupt_flags |= NRF_ESB_INT_RX_DATA_RECEIVED_MSK;
                    NVIC_SetPendingIRQ(ESB_EVT_IRQ);
                }
            }
        } else {
            // Push the new packet to the RX buffer and trigger a received event if the operation was
            // successful.
            if (rx_fifo_push_rfbuf(NRF_RADIO->RXMATCH, p_pipe_info->pid))
            {
//                NRF_LOG_INFO("DATA_RECEIVED_MSK in on_radio_disabled_rx non promiscuous branch");
                m_interrupt_flags |= NRF_ESB_INT_RX_DATA_RECEIVED_MSK;
                NVIC_SetPendingIRQ(ESB_EVT_IRQ);
            }
        }
    }
}

static void on_radio_end_rx(void)
{
//    NRF_LOG_INFO("on_rx_end");

    bool            send_rx_event      = true;
    pipe_info_t *   p_pipe_info;

//    uint8_t field_length = m_rx_payload_buffer[0];
    uint8_t field_s1 = m_rx_payload_buffer[1];  //bit 2..1: PID, bit 0: no_ack flag

    //if (NRF_RADIO->CRCSTATUS == 0 && !m_config_local.disallow_auto_ack) //consume frames with invalid CRC
    if (NRF_RADIO->CRCSTATUS == 0 && m_config_local.protocol != NRF_ESB_PROTOCOL_ESB_PROMISCUOUS)
    {
        //clear_events_restart_rx(); //ToDo: rework
        clear_events_do_not_restart_rx();
        return;
    }

    if (m_rx_fifo.count >= NRF_ESB_RX_FIFO_SIZE)
    {
        //clear_events_restart_rx(); //ToDo: rework
        clear_events_do_not_restart_rx();
        return;
    }


    p_pipe_info = &m_rx_pipe_info[NRF_RADIO->RXMATCH];
    if (NRF_RADIO->RXCRC == p_pipe_info->crc && (field_s1 >> 1) == p_pipe_info->pid) {
        send_rx_event = false; // if  we are in passive rx mode, with auto ack, we fire a receive event, even on pipe mismatch (to collect ACK payloads from real PRX)
    }

    p_pipe_info->pid = field_s1 >> 1;
    p_pipe_info->crc = NRF_RADIO->RXCRC;



    if (m_config_local.mode == NRF_ESB_MODE_PTX_STAY_RX && (m_interrupt_flags & NRF_ESB_INT_TX_SUCCESS_MSK) != 0) {
        // we switched to PRX after succesfull TX and have the TX_SUCCESS interrupt event still pending, thus we don't
        // add an RX event, as it should be only present, if the on_radio_disabled_tx_wait_for_ack_continue_rx has
        // enabled NRF_ESB_INT_RX_DATA_RECEIVED_MSK (only happens if the ACK frame was with payload)

        // for successive RX data, TX_SUCCESS mask should already be cleared by the ISR and thus we are fine to report
        // RX events

        if (m_rx_payload_buffer[0] == 0) send_rx_event = false; // IF PAYLOAD length is > 0 we report anyways
    }


    if (send_rx_event)
    {
        if (m_config_local.mode == NRF_ESB_MODE_PROMISCOUS && NRF_ESB_CHECK_PROMISCUOUS_CRC_WITH_APP_SCHEDULER) {
            // schedule frame for validation
            schedule_frame_for_validation_before_pushing_to_rx_fifo(NRF_RADIO->RXMATCH, p_pipe_info->pid);
        } else {
            // Push the new packet to the RX buffer and trigger a received event if the operation was
            // successful.
            if (rx_fifo_push_rfbuf(NRF_RADIO->RXMATCH, p_pipe_info->pid))
            {
//                NRF_LOG_INFO("DATA_RECEIVED_MSK in on_radio_end_rx promiscuous");
                m_interrupt_flags |= NRF_ESB_INT_RX_DATA_RECEIVED_MSK;
                NVIC_SetPendingIRQ(ESB_EVT_IRQ);
            }
        }
    }
}


static void on_radio_disabled_rx_ack(void)
{
    NRF_RADIO->SHORTS      = m_radio_shorts_common | RADIO_SHORTS_DISABLED_TXEN_Msk;

    update_rf_payload_format(m_config_local.payload_length);

    NRF_RADIO->PACKETPTR = (uint32_t)m_rx_payload_buffer;
    on_radio_disabled = on_radio_disabled_rx;

    m_nrf_esb_mainstate = NRF_ESB_STATE_PRX;
}


/**@brief Function for clearing pending interrupts.
 *
 * @param[in,out]   p_interrupts        Pointer to the value that holds the current interrupts.
 *
 * @retval  NRF_SUCCESS                     If the interrupts were cleared successfully.
 * @retval  NRF_ERROR_NULL                  If the required parameter was NULL.
 * @retval  NRF_INVALID_STATE               If the module is not initialized.
 */
static uint32_t nrf_esb_get_clear_interrupts(uint32_t * p_interrupts)
{
    VERIFY_TRUE(m_esb_initialized, NRF_ERROR_INVALID_STATE);
    VERIFY_PARAM_NOT_NULL(p_interrupts);

    DISABLE_RF_IRQ();

    *p_interrupts = m_interrupt_flags;
    m_interrupt_flags = 0;

    ENABLE_RF_IRQ();

    return NRF_SUCCESS;
}


void RADIO_IRQHandler()
{
    if (NRF_RADIO->EVENTS_READY && (NRF_RADIO->INTENSET & RADIO_INTENSET_READY_Msk))
    {
        NRF_RADIO->EVENTS_READY = 0;
    }

    if (NRF_RADIO->EVENTS_END && (NRF_RADIO->INTENSET & RADIO_INTENSET_END_Msk))
    {
        NRF_RADIO->EVENTS_END = 0;

        // Call the correct on_radio_end function, depending on the current protocol state
        if (on_radio_end)
        {
            on_radio_end();
        }
    }

    if (NRF_RADIO->EVENTS_DISABLED && (NRF_RADIO->INTENSET & RADIO_INTENSET_DISABLED_Msk))
    {
        NRF_RADIO->EVENTS_DISABLED = 0;

        // Call the correct on_radio_disable function, depending on the current protocol state
        if (on_radio_disabled)
        {
            on_radio_disabled();
        }
    }

}


uint32_t nrf_esb_init(nrf_esb_config_t const * p_config)
{
    NRF_LOG_DEBUG("called nrf_esb_init with mode %d", p_config->mode);
    uint32_t err_code;

    VERIFY_PARAM_NOT_NULL(p_config);


    if (m_esb_initialized)
    {
        err_code = nrf_esb_disable();
        if (err_code != NRF_SUCCESS)
        {
            return err_code;
        }
    }




    m_event_handler = p_config->event_handler;

    memcpy(&m_config_local, p_config, sizeof(nrf_esb_config_t));


    m_interrupt_flags    = 0;

    memset(m_rx_pipe_info, 0, sizeof(m_rx_pipe_info));
    memset(m_pids, 0, sizeof(m_pids));

    VERIFY_TRUE(update_radio_parameters(), NRF_ERROR_INVALID_PARAM);
   
    initialize_fifos();

    sys_timer_init();

    ppi_init();

    NVIC_SetPriority(RADIO_IRQn, m_config_local.radio_irq_priority & ESB_IRQ_PRIORITY_MSK);
    NVIC_SetPriority(ESB_EVT_IRQ, m_config_local.event_irq_priority & ESB_IRQ_PRIORITY_MSK);
    NVIC_EnableIRQ(ESB_EVT_IRQ);

    m_nrf_esb_mainstate = NRF_ESB_STATE_IDLE;
    m_esb_initialized = true;


#ifdef NRF52832_XXAA
if ((NRF_FICR->INFO.VARIANT & 0x0000FF00) == 0x00004500) //Check if the device is an nRF52832 Rev. 2.
    //Workaround for nRF52832 rev 2 errata 182
    *(volatile uint32_t *) 0x4000173C |= (1 << 10);
#endif

    return NRF_SUCCESS;
}


uint32_t nrf_esb_suspend(void)
{
    VERIFY_TRUE(m_nrf_esb_mainstate == NRF_ESB_STATE_IDLE, NRF_ERROR_BUSY);

    // Clear PPI
    NRF_PPI->CHENCLR = (1 << NRF_ESB_PPI_TIMER_START) |
                       (1 << NRF_ESB_PPI_TIMER_STOP)  |
                       (1 << NRF_ESB_PPI_RX_TIMEOUT)  |
                       (1 << NRF_ESB_PPI_TX_START);

    m_nrf_esb_mainstate = NRF_ESB_STATE_IDLE;

    return NRF_SUCCESS;
}


uint32_t nrf_esb_disable(void)
{
    // Clear PPI
    NRF_PPI->CHENCLR = (1 << NRF_ESB_PPI_TIMER_START) |
                       (1 << NRF_ESB_PPI_TIMER_STOP)  |
                       (1 << NRF_ESB_PPI_RX_TIMEOUT)  |
                       (1 << NRF_ESB_PPI_TX_START);

    m_nrf_esb_mainstate = NRF_ESB_STATE_IDLE;
    m_esb_initialized = false;

    reset_fifos();

    memset(m_rx_pipe_info, 0, sizeof(m_rx_pipe_info));
    memset(m_pids, 0, sizeof(m_pids));

    // Disable the radio
    NVIC_DisableIRQ(ESB_EVT_IRQ);
    NRF_RADIO->SHORTS = RADIO_SHORTS_READY_START_Enabled << RADIO_SHORTS_READY_START_Pos |
                        RADIO_SHORTS_END_DISABLE_Enabled << RADIO_SHORTS_END_DISABLE_Pos;

    return NRF_SUCCESS;
}


bool nrf_esb_is_idle(void)
{
    return m_nrf_esb_mainstate == NRF_ESB_STATE_IDLE;
}


void ESB_EVT_IRQHandler(void)
{

    ret_code_t      err_code;
    uint32_t        interrupts;
    nrf_esb_evt_t   event;

    event.tx_attempts = m_last_tx_attempts;

    err_code = nrf_esb_get_clear_interrupts(&interrupts);
    if (err_code == NRF_SUCCESS && m_event_handler != 0)
    {
        //NRF_LOG_INFO("RF INT %.8X", interrupts);

        
        if (interrupts == (NRF_ESB_INT_TX_SUCCESS_MSK | NRF_ESB_INT_RX_DATA_RECEIVED_MSK)) {
            // TX success and RX_RECEIVED --> means ACK payload with len > 0 in RX queue
                event.evt_id = NRF_ESB_EVENT_TX_SUCCESS_ACK_PAY;
                m_event_handler(&event);

        } else {
            if (interrupts & NRF_ESB_INT_TX_SUCCESS_MSK)
            {
                event.evt_id = NRF_ESB_EVENT_TX_SUCCESS;
                m_event_handler(&event);
            }
            if (interrupts & NRF_ESB_INT_TX_FAILED_MSK)
            {
                event.evt_id = NRF_ESB_EVENT_TX_FAILED;
                m_event_handler(&event);
            }
            if (interrupts & NRF_ESB_INT_RX_DATA_RECEIVED_MSK)
            {
                event.evt_id = NRF_ESB_EVENT_RX_RECEIVED;
                m_event_handler(&event);
            }
            if (interrupts & NRF_ESB_INT_RX_PROMISCUOUS_DATA_RECEIVED_MSK)
            {
                /*
                event.evt_id = NRF_ESB_EVENT_RX_RECEIVED_PROMISCUOUS_UNVALIDATED;
                m_event_handler(&event);
                */
            }
        }
    }
//    NRF_LOG_INFO("ISR consumed NRF_ESB_INT_RX_DATA_RECEIVED_MSK and NRF_ESB_INT_TX_SUCCESS_MSK");

}

uint32_t nrf_esb_write_payload(nrf_esb_payload_t const * p_payload)
{
    VERIFY_TRUE(m_esb_initialized, NRF_ERROR_INVALID_STATE);
    VERIFY_PARAM_NOT_NULL(p_payload);
    VERIFY_PAYLOAD_LENGTH(p_payload);
    VERIFY_FALSE(m_tx_fifo.count >= NRF_ESB_TX_FIFO_SIZE, NRF_ERROR_NO_MEM);
    VERIFY_TRUE(p_payload->pipe < NRF_ESB_PIPE_COUNT, NRF_ERROR_INVALID_PARAM);

    DISABLE_RF_IRQ();

    memcpy(m_tx_fifo.p_payload[m_tx_fifo.entry_point], p_payload, sizeof(nrf_esb_payload_t));

    m_pids[p_payload->pipe] = (m_pids[p_payload->pipe] + 1) % (NRF_ESB_PID_MAX + 1);
    m_tx_fifo.p_payload[m_tx_fifo.entry_point]->pid = m_pids[p_payload->pipe];

    if (++m_tx_fifo.entry_point >= NRF_ESB_TX_FIFO_SIZE)
    {
        m_tx_fifo.entry_point = 0;
    }

    m_tx_fifo.count++;

    ENABLE_RF_IRQ();


    if ((m_config_local.mode == NRF_ESB_MODE_PTX &&
        m_config_local.tx_mode == NRF_ESB_TXMODE_AUTO &&
        m_nrf_esb_mainstate == NRF_ESB_STATE_IDLE) ||
        (m_config_local.mode == NRF_ESB_MODE_PTX_STAY_RX &&
        m_config_local.tx_mode == NRF_ESB_TXMODE_AUTO))
    {
        start_tx_transaction();
    }

    return NRF_SUCCESS;
}


uint32_t nrf_esb_read_rx_payload(nrf_esb_payload_t * p_payload)
{
    VERIFY_TRUE(m_esb_initialized, NRF_ERROR_INVALID_STATE);
    VERIFY_PARAM_NOT_NULL(p_payload);

//    WAIT_UNLOCK_RX_FIFO;
//    LOCK_RX_FIFO;

    if (m_rx_fifo.count == 0)
    {
//        UNLOCK_RX_FIFO;
        return NRF_ERROR_NOT_FOUND;
    }

    DISABLE_RF_IRQ();
      
    p_payload->length = m_rx_fifo.p_payload[m_rx_fifo.exit_point]->length;
    p_payload->pipe   = m_rx_fifo.p_payload[m_rx_fifo.exit_point]->pipe;
    p_payload->rssi   = m_rx_fifo.p_payload[m_rx_fifo.exit_point]->rssi;
    p_payload->pid    = m_rx_fifo.p_payload[m_rx_fifo.exit_point]->pid;
    p_payload->noack  = m_rx_fifo.p_payload[m_rx_fifo.exit_point]->noack;
    p_payload->validated_promiscuous_frame = m_rx_fifo.p_payload[m_rx_fifo.exit_point]->validated_promiscuous_frame;
    p_payload->rx_channel = m_rx_fifo.p_payload[m_rx_fifo.exit_point]->rx_channel;
    p_payload->rx_channel_index = m_rx_fifo.p_payload[m_rx_fifo.exit_point]->rx_channel_index;

    if (m_config_local.protocol == NRF_ESB_PROTOCOL_ESB_PROMISCUOUS) {
        memcpy(p_payload->data, m_rx_fifo.p_payload[m_rx_fifo.exit_point]->data, 60);
    } else {
        memcpy(p_payload->data, m_rx_fifo.p_payload[m_rx_fifo.exit_point]->data, p_payload->length);
    }

    if (++m_rx_fifo.exit_point >= NRF_ESB_RX_FIFO_SIZE)
    {
        m_rx_fifo.exit_point = 0;
    }

    m_rx_fifo.count--;

//    UNLOCK_RX_FIFO;
    ENABLE_RF_IRQ();

    return NRF_SUCCESS;
}


uint32_t nrf_esb_start_tx(void)
{
    if (m_config_local.mode != NRF_ESB_MODE_PTX_STAY_RX) {
        VERIFY_TRUE(m_nrf_esb_mainstate == NRF_ESB_STATE_IDLE, NRF_ERROR_BUSY);
    }

    if (m_tx_fifo.count == 0)
    {
        return NRF_ERROR_BUFFER_EMPTY;
    }

    start_tx_transaction();

    return NRF_SUCCESS;
}


uint32_t nrf_esb_start_rx(void)
{
    VERIFY_TRUE(m_nrf_esb_mainstate == NRF_ESB_STATE_IDLE, NRF_ERROR_BUSY);

//    NRF_LOG_INFO("start RX");

    NRF_RADIO->INTENCLR = 0xFFFFFFFF; //disable all radio IRQs
    NRF_RADIO->EVENTS_DISABLED = 0; //enable RADIO events
    on_radio_disabled = on_radio_disabled_rx; //set ISR for radio DISABLE event
    on_radio_end = on_radio_end_rx; // set ISR for radio END event
    

    if (m_config_local.mode == NRF_ESB_MODE_SNIFF || m_config_local.mode == NRF_ESB_MODE_PTX_STAY_RX || m_config_local.mode == NRF_ESB_MODE_PROMISCOUS) {
        // don't use END->DISABLE shortcut, but END->start
        NRF_RADIO->SHORTS = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_ADDRESS_RSSISTART_Msk | RADIO_SHORTS_DISABLED_RSSISTOP_Msk | RADIO_SHORTS_END_START_Msk | RADIO_SHORTS_DISABLED_RXEN_Msk;
    } else {
        NRF_RADIO->SHORTS      = m_radio_shorts_common | RADIO_SHORTS_DISABLED_TXEN_Msk;
    } 

    switch (m_config_local.mode) {
        case NRF_ESB_MODE_SNIFF:
        case NRF_ESB_MODE_PROMISCOUS:
        case NRF_ESB_MODE_PTX_STAY_RX:
            NRF_RADIO->INTENSET    = RADIO_INTENSET_DISABLED_Msk | RADIO_INTENSET_END_Msk;        
            break;
        default:    
            NRF_RADIO->INTENSET    = RADIO_INTENSET_DISABLED_Msk;
    }
    
    m_nrf_esb_mainstate    = NRF_ESB_STATE_PRX;

    if (m_config_local.protocol == NRF_ESB_PROTOCOL_ESB_PROMISCUOUS) NRF_RADIO->RXADDRESSES  = 0xff; //use all pipe addresses in promiscuous mode
    else NRF_RADIO->RXADDRESSES  = m_esb_addr.rx_pipes_enabled;
    //NRF_RADIO->FREQUENCY    = m_esb_addr.rf_channel;
    NRF_RADIO->FREQUENCY    = m_esb_addr.channel_to_frequency[m_esb_addr.rf_channel];
    NRF_RADIO->PACKETPTR    = (uint32_t)m_rx_payload_buffer;

    NVIC_ClearPendingIRQ(RADIO_IRQn); //clear all pending IRQs
    NVIC_EnableIRQ(RADIO_IRQn);

    NRF_RADIO->EVENTS_ADDRESS = 0;
    NRF_RADIO->EVENTS_PAYLOAD = 0;
    NRF_RADIO->EVENTS_DISABLED = 0;
    NRF_RADIO->EVENTS_END = 0;

    NRF_RADIO->TASKS_RXEN  = 1;


//    NRF_LOG_INFO("RXEN");
    return NRF_SUCCESS;
}


uint32_t nrf_esb_stop_rx(void)
{
    if (m_nrf_esb_mainstate == NRF_ESB_STATE_PRX)
    {
        NRF_RADIO->SHORTS = 0;
        NRF_RADIO->INTENCLR = 0xFFFFFFFF;
        on_radio_disabled = NULL;
        NRF_RADIO->EVENTS_DISABLED = 0;
        NRF_RADIO->TASKS_DISABLE = 1;
        while (NRF_RADIO->EVENTS_DISABLED == 0);
        m_nrf_esb_mainstate = NRF_ESB_STATE_IDLE;

        //m_config_local.disallow_auto_ack = false; //no special functionality for radio_on_disable_rx
        return NRF_SUCCESS;
    }

    return NRF_ESB_ERROR_NOT_IN_RX_MODE;
}


uint32_t nrf_esb_flush_tx(void)
{
    VERIFY_TRUE(m_esb_initialized, NRF_ERROR_INVALID_STATE);

    DISABLE_RF_IRQ();

    m_tx_fifo.count = 0;
    m_tx_fifo.entry_point = 0;
    m_tx_fifo.exit_point = 0;

    ENABLE_RF_IRQ();

    return NRF_SUCCESS;
}


uint32_t nrf_esb_pop_tx(void)
{
    VERIFY_TRUE(m_esb_initialized, NRF_ERROR_INVALID_STATE);
    VERIFY_TRUE(m_tx_fifo.count > 0, NRF_ERROR_BUFFER_EMPTY);

    DISABLE_RF_IRQ();

    if (--m_tx_fifo.entry_point >= NRF_ESB_TX_FIFO_SIZE)
    {
        m_tx_fifo.entry_point = 0;
    }
    m_tx_fifo.count--;

    ENABLE_RF_IRQ();

    return NRF_SUCCESS;
}


uint32_t nrf_esb_flush_rx(void)
{
    VERIFY_TRUE(m_esb_initialized, NRF_ERROR_INVALID_STATE);

    DISABLE_RF_IRQ();
//    WAIT_UNLOCK_RX_FIFO;
//    LOCK_RX_FIFO;

    m_rx_fifo.count = 0;
    m_rx_fifo.entry_point = 0;
    m_rx_fifo.exit_point = 0;

//    UNLOCK_RX_FIFO;

    memset(m_rx_pipe_info, 0, sizeof(m_rx_pipe_info));

    ENABLE_RF_IRQ();

    return NRF_SUCCESS;
}


uint32_t nrf_esb_set_address_length(uint8_t length)
{
    VERIFY_TRUE(m_nrf_esb_mainstate == NRF_ESB_STATE_IDLE, NRF_ERROR_BUSY);
    VERIFY_TRUE(length > 2 && length < 6, NRF_ERROR_INVALID_PARAM);
    
#ifdef NRF52832_XXAA
    uint32_t base_address_mask = length == 5 ? 0xFFFF0000 : 0xFF000000;
    if ((NRF_FICR->INFO.VARIANT & 0x0000FF00) == 0x00004200)  //Check if the device is an nRF52832 Rev. 1.
    {
        /* 
        Workaround for nRF52832 Rev 1 Errata 107
        Check if pipe 0 or pipe 1-7 has a 'zero address'.
        Avoid using access addresses in the following pattern (where X is don't care): 
        ADDRLEN=5 
        BASE0 = 0x0000XXXX, PREFIX0 = 0xXXXXXX00 
        BASE1 = 0x0000XXXX, PREFIX0 = 0xXXXX00XX 
        BASE1 = 0x0000XXXX, PREFIX0 = 0xXX00XXXX 
        BASE1 = 0x0000XXXX, PREFIX0 = 0x00XXXXXX 
        BASE1 = 0x0000XXXX, PREFIX1 = 0xXXXXXX00 
        BASE1 = 0x0000XXXX, PREFIX1 = 0xXXXX00XX 
        BASE1 = 0x0000XXXX, PREFIX1 = 0xXX00XXXX 
        BASE1 = 0x0000XXXX, PREFIX1 = 0x00XXXXXX 

        ADDRLEN=4 
        BASE0 = 0x00XXXXXX, PREFIX0 = 0xXXXXXX00 
        BASE1 = 0x00XXXXXX, PREFIX0 = 0xXXXX00XX 
        BASE1 = 0x00XXXXXX, PREFIX0 = 0xXX00XXXX 
        BASE1 = 0x00XXXXXX, PREFIX0 = 0x00XXXXXX 
        BASE1 = 0x00XXXXXX, PREFIX1 = 0xXXXXXX00 
        BASE1 = 0x00XXXXXX, PREFIX1 = 0xXXXX00XX 
        BASE1 = 0x00XXXXXX, PREFIX1 = 0xXX00XXXX 
        BASE1 = 0x00XXXXXX, PREFIX1 = 0x00XXXXXX
        */
        if ((NRF_RADIO->BASE0 & base_address_mask) == 0 && (NRF_RADIO->PREFIX0 & 0x000000FF) == 0)
        {
            return NRF_ERROR_INVALID_PARAM;
        }
        if ((NRF_RADIO->BASE1 & base_address_mask) == 0 && ((NRF_RADIO->PREFIX0 & 0x0000FF00) == 0 ||(NRF_RADIO->PREFIX0 & 0x00FF0000) == 0 || (NRF_RADIO->PREFIX0 & 0xFF000000) == 0 ||
           (NRF_RADIO->PREFIX1 & 0xFF000000) == 0 || (NRF_RADIO->PREFIX1 & 0x00FF0000) == 0 ||(NRF_RADIO->PREFIX1 & 0x0000FF00) == 0 || (NRF_RADIO->PREFIX1 & 0x000000FF) == 0))
        {
            return NRF_ERROR_INVALID_PARAM;
        }
    }
#endif

    m_esb_addr.addr_length = length;

    update_rf_payload_format(m_config_local.payload_length);

#ifdef NRF52832_XXAA
    if ((NRF_FICR->INFO.VARIANT & 0x0000FF00) == 0x00004500)  //Check if the device is an nRF52832 Rev. 2.
    {
        return apply_address_workarounds();
    }
    else
    {
        return NRF_SUCCESS;
    }
#else
    return NRF_SUCCESS;
#endif
}

uint32_t nrf_esb_set_base_address_0(uint8_t const * p_addr)
{
    VERIFY_TRUE(m_nrf_esb_mainstate == NRF_ESB_STATE_IDLE, NRF_ERROR_BUSY);
    VERIFY_PARAM_NOT_NULL(p_addr);

#ifdef NRF52832_XXAA
    if ((NRF_FICR->INFO.VARIANT & 0x0000FF00) == 0x00004200)  //Check if the device is an nRF52832 Rev. 1.
    {
        /*
        Workaround for nRF52832 Rev 1 Errata 107
        Check if pipe 0 or pipe 1-7 has a 'zero address'.
        Avoid using access addresses in the following pattern (where X is don't care): 
        ADDRLEN=5 
        BASE0 = 0x0000XXXX, PREFIX0 = 0xXXXXXX00 
        BASE1 = 0x0000XXXX, PREFIX0 = 0xXXXX00XX 
        BASE1 = 0x0000XXXX, PREFIX0 = 0xXX00XXXX 
        BASE1 = 0x0000XXXX, PREFIX0 = 0x00XXXXXX 
        BASE1 = 0x0000XXXX, PREFIX1 = 0xXXXXXX00 
        BASE1 = 0x0000XXXX, PREFIX1 = 0xXXXX00XX 
        BASE1 = 0x0000XXXX, PREFIX1 = 0xXX00XXXX 
        BASE1 = 0x0000XXXX, PREFIX1 = 0x00XXXXXX 

        ADDRLEN=4 
        BASE0 = 0x00XXXXXX, PREFIX0 = 0xXXXXXX00 
        BASE1 = 0x00XXXXXX, PREFIX0 = 0xXXXX00XX 
        BASE1 = 0x00XXXXXX, PREFIX0 = 0xXX00XXXX 
        BASE1 = 0x00XXXXXX, PREFIX0 = 0x00XXXXXX 
        BASE1 = 0x00XXXXXX, PREFIX1 = 0xXXXXXX00 
        BASE1 = 0x00XXXXXX, PREFIX1 = 0xXXXX00XX 
        BASE1 = 0x00XXXXXX, PREFIX1 = 0xXX00XXXX 
        BASE1 = 0x00XXXXXX, PREFIX1 = 0x00XXXXXX
        */
        uint32_t base_address_mask = m_esb_addr.addr_length == 5 ? 0xFFFF0000 : 0xFF000000;
        if ((addr_conv(p_addr) & base_address_mask) == 0 && (NRF_RADIO->PREFIX0 & 0x000000FF) == 0)
        {
            return NRF_ERROR_INVALID_PARAM;
        }
    }
#endif



    memcpy(m_esb_addr.base_addr_p0, p_addr, 4);

    update_radio_addresses(NRF_ESB_ADDR_UPDATE_MASK_BASE0);
#ifdef NRF52832_XXAA
    return apply_address_workarounds();
#else
    return NRF_SUCCESS;
#endif
}


uint32_t nrf_esb_set_base_address_1(uint8_t const * p_addr)
{
    VERIFY_TRUE(m_nrf_esb_mainstate == NRF_ESB_STATE_IDLE, NRF_ERROR_BUSY);
    VERIFY_PARAM_NOT_NULL(p_addr);

#ifdef NRF52832_XXAA
    if ((NRF_FICR->INFO.VARIANT & 0x0000FF00) == 0x00004200)  //Check if the device is an nRF52832 Rev. 1.
    {
        /*
        Workaround for nRF52832 Rev 1 Errata 107
        Check if pipe 0 or pipe 1-7 has a 'zero address'.
        Avoid using access addresses in the following pattern (where X is don't care): 
        ADDRLEN=5 
        BASE0 = 0x0000XXXX, PREFIX0 = 0xXXXXXX00
        BASE1 = 0x0000XXXX, PREFIX0 = 0xXXXX00XX
        BASE1 = 0x0000XXXX, PREFIX0 = 0xXX00XXXX
        BASE1 = 0x0000XXXX, PREFIX0 = 0x00XXXXXX
        BASE1 = 0x0000XXXX, PREFIX1 = 0xXXXXXX00
        BASE1 = 0x0000XXXX, PREFIX1 = 0xXXXX00XX
        BASE1 = 0x0000XXXX, PREFIX1 = 0xXX00XXXX
        BASE1 = 0x0000XXXX, PREFIX1 = 0x00XXXXXX

        ADDRLEN=4 
        BASE0 = 0x00XXXXXX, PREFIX0 = 0xXXXXXX00
        BASE1 = 0x00XXXXXX, PREFIX0 = 0xXXXX00XX
        BASE1 = 0x00XXXXXX, PREFIX0 = 0xXX00XXXX
        BASE1 = 0x00XXXXXX, PREFIX0 = 0x00XXXXXX
        BASE1 = 0x00XXXXXX, PREFIX1 = 0xXXXXXX00
        BASE1 = 0x00XXXXXX, PREFIX1 = 0xXXXX00XX
        BASE1 = 0x00XXXXXX, PREFIX1 = 0xXX00XXXX
        BASE1 = 0x00XXXXXX, PREFIX1 = 0x00XXXXXX
        */
        uint32_t base_address_mask = m_esb_addr.addr_length == 5 ? 0xFFFF0000 : 0xFF000000;
        if ((addr_conv(p_addr) & base_address_mask) == 0 &&
            ((NRF_RADIO->PREFIX0 & 0x0000FF00) == 0 ||(NRF_RADIO->PREFIX0 & 0x00FF0000) == 0 ||
            (NRF_RADIO->PREFIX0 & 0xFF000000) == 0 || (NRF_RADIO->PREFIX1 & 0xFF000000) == 0 ||
            (NRF_RADIO->PREFIX1 & 0x00FF0000) == 0 ||(NRF_RADIO->PREFIX1 & 0x0000FF00) == 0 ||
            (NRF_RADIO->PREFIX1 & 0x000000FF) == 0))
        {
            return NRF_ERROR_INVALID_PARAM;
        }
    }
#endif

    memcpy(m_esb_addr.base_addr_p1, p_addr, 4);

    update_radio_addresses(NRF_ESB_ADDR_UPDATE_MASK_BASE1);

#ifdef NRF52832_XXAA
    return apply_address_workarounds();
#else
    return NRF_SUCCESS;
#endif
}


uint32_t nrf_esb_set_prefixes(uint8_t const * p_prefixes, uint8_t num_pipes)
{
    VERIFY_TRUE(m_nrf_esb_mainstate == NRF_ESB_STATE_IDLE, NRF_ERROR_BUSY);
    VERIFY_PARAM_NOT_NULL(p_prefixes);
    VERIFY_TRUE(num_pipes <= NRF_ESB_PIPE_COUNT, NRF_ERROR_INVALID_PARAM);
    
#ifdef NRF52832_XXAA
    if ((NRF_FICR->INFO.VARIANT & 0x0000FF00) == 0x00004200)  //Check if the device is an nRF52832 Rev. 1.
    {
        /*
        Workaround for nRF52832 Rev 1 Errata 107
        Check if pipe 0 or pipe 1-7 has a 'zero address'.
        Avoid using access addresses in the following pattern (where X is don't care): 
        ADDRLEN=5 
        BASE0 = 0x0000XXXX, PREFIX0 = 0xXXXXXX00 
        BASE1 = 0x0000XXXX, PREFIX0 = 0xXXXX00XX 
        BASE1 = 0x0000XXXX, PREFIX0 = 0xXX00XXXX 
        BASE1 = 0x0000XXXX, PREFIX0 = 0x00XXXXXX 
        BASE1 = 0x0000XXXX, PREFIX1 = 0xXXXXXX00 
        BASE1 = 0x0000XXXX, PREFIX1 = 0xXXXX00XX 
        BASE1 = 0x0000XXXX, PREFIX1 = 0xXX00XXXX 
        BASE1 = 0x0000XXXX, PREFIX1 = 0x00XXXXXX 

        ADDRLEN=4 
        BASE0 = 0x00XXXXXX, PREFIX0 = 0xXXXXXX00 
        BASE1 = 0x00XXXXXX, PREFIX0 = 0xXXXX00XX 
        BASE1 = 0x00XXXXXX, PREFIX0 = 0xXX00XXXX 
        BASE1 = 0x00XXXXXX, PREFIX0 = 0x00XXXXXX 
        BASE1 = 0x00XXXXXX, PREFIX1 = 0xXXXXXX00 
        BASE1 = 0x00XXXXXX, PREFIX1 = 0xXXXX00XX 
        BASE1 = 0x00XXXXXX, PREFIX1 = 0xXX00XXXX 
        BASE1 = 0x00XXXXXX, PREFIX1 = 0x00XXXXXX
        */
        uint32_t base_address_mask = m_esb_addr.addr_length == 5 ? 0xFFFF0000 : 0xFF000000;
        if (num_pipes >= 1 && (NRF_RADIO->BASE0 & base_address_mask) == 0 && p_prefixes[0] == 0)
        {
            return NRF_ERROR_INVALID_PARAM;
        }

        if ((NRF_RADIO->BASE1 & base_address_mask) == 0)
        {
            for (uint8_t i = 1; i < num_pipes; i++)
            {
                if (p_prefixes[i] == 0)
                {
                    return NRF_ERROR_INVALID_PARAM;
                }
            }
        }
    }
#endif
    
    memcpy(m_esb_addr.pipe_prefixes, p_prefixes, num_pipes);
    m_esb_addr.num_pipes = num_pipes;
    m_esb_addr.rx_pipes_enabled = BIT_MASK_UINT_8(num_pipes);

    update_radio_addresses(NRF_ESB_ADDR_UPDATE_MASK_PREFIX);

#ifdef NRF52832_XXAA
    return apply_address_workarounds();
#else
    return NRF_SUCCESS;
#endif
}


uint32_t nrf_esb_update_prefix(uint8_t pipe, uint8_t prefix)
{
    VERIFY_TRUE(m_nrf_esb_mainstate == NRF_ESB_STATE_IDLE, NRF_ERROR_BUSY);
    VERIFY_TRUE(pipe < NRF_ESB_PIPE_COUNT, NRF_ERROR_INVALID_PARAM);
    
#ifdef NRF52832_XXAA
    if ((NRF_FICR->INFO.VARIANT & 0x0000FF00) == 0x00004200)  //Check if the device is an nRF52832 Rev. 1.
    {
        /*
        Workaround for nRF52832 Rev 1 Errata 107
        Check if pipe 0 or pipe 1-7 has a 'zero address'.
        Avoid using access addresses in the following pattern (where X is don't care): 
        ADDRLEN=5 
        BASE0 = 0x0000XXXX, PREFIX0 = 0xXXXXXX00 
        BASE1 = 0x0000XXXX, PREFIX0 = 0xXXXX00XX 
        BASE1 = 0x0000XXXX, PREFIX0 = 0xXX00XXXX 
        BASE1 = 0x0000XXXX, PREFIX0 = 0x00XXXXXX 
        BASE1 = 0x0000XXXX, PREFIX1 = 0xXXXXXX00 
        BASE1 = 0x0000XXXX, PREFIX1 = 0xXXXX00XX 
        BASE1 = 0x0000XXXX, PREFIX1 = 0xXX00XXXX 
        BASE1 = 0x0000XXXX, PREFIX1 = 0x00XXXXXX 

        ADDRLEN=4 
        BASE0 = 0x00XXXXXX, PREFIX0 = 0xXXXXXX00 
        BASE1 = 0x00XXXXXX, PREFIX0 = 0xXXXX00XX 
        BASE1 = 0x00XXXXXX, PREFIX0 = 0xXX00XXXX 
        BASE1 = 0x00XXXXXX, PREFIX0 = 0x00XXXXXX 
        BASE1 = 0x00XXXXXX, PREFIX1 = 0xXXXXXX00 
        BASE1 = 0x00XXXXXX, PREFIX1 = 0xXXXX00XX 
        BASE1 = 0x00XXXXXX, PREFIX1 = 0xXX00XXXX 
        BASE1 = 0x00XXXXXX, PREFIX1 = 0x00XXXXXX
        */
        uint32_t base_address_mask = m_esb_addr.addr_length == 5 ? 0xFFFF0000 : 0xFF000000;
        if (pipe == 0)
        {
            if ((NRF_RADIO->BASE0 & base_address_mask) == 0 && prefix == 0)
            {
                return NRF_ERROR_INVALID_PARAM;
            }
        }
        else
        {
            if ((NRF_RADIO->BASE1 & base_address_mask) == 0 && prefix == 0)
            {
                return NRF_ERROR_INVALID_PARAM;
            }
        }
    }
#endif
    m_esb_addr.pipe_prefixes[pipe] = prefix;

    update_radio_addresses(NRF_ESB_ADDR_UPDATE_MASK_PREFIX);

#ifdef NRF52832_XXAA
    return apply_address_workarounds();
#else
    return NRF_SUCCESS;
#endif
}


uint32_t nrf_esb_enable_pipes(uint8_t enable_mask)
{
    VERIFY_TRUE(m_nrf_esb_mainstate == NRF_ESB_STATE_IDLE, NRF_ERROR_BUSY);
    VERIFY_TRUE((enable_mask | BIT_MASK_UINT_8(NRF_ESB_PIPE_COUNT)) == BIT_MASK_UINT_8(NRF_ESB_PIPE_COUNT), NRF_ERROR_INVALID_PARAM);

    m_esb_addr.rx_pipes_enabled = enable_mask;

#ifdef NRF52832_XXAA
    return apply_address_workarounds();
#else
    return NRF_SUCCESS;
#endif
}


uint32_t nrf_esb_set_rf_channel(uint32_t channel)
{
    //if (m_config_local.mode == NRF_ESB_MODE_PROMISCOUS || m_config_local.mode == NRF_ESB_MODE_SNIFF) {
    if ((m_config_local.mode == NRF_ESB_MODE_PROMISCOUS || m_config_local.mode == NRF_ESB_MODE_SNIFF) && (m_nrf_esb_mainstate == NRF_ESB_STATE_PRX)) {
        //VERIFY_TRUE(channel <= 100, NRF_ERROR_INVALID_PARAM);
        VERIFY_TRUE(channel < m_esb_addr.channel_to_frequency_len, NRF_ERROR_INVALID_PARAM);

        nrf_esb_stop_rx();
        m_esb_addr.rf_channel = channel;
        nrf_esb_start_rx();

        return NRF_SUCCESS;

    } else {
        VERIFY_TRUE(m_nrf_esb_mainstate == NRF_ESB_STATE_IDLE, NRF_ERROR_BUSY);
        //VERIFY_TRUE(channel <= 100, NRF_ERROR_INVALID_PARAM);
        VERIFY_TRUE(channel < m_esb_addr.channel_to_frequency_len, NRF_ERROR_INVALID_PARAM);

        m_esb_addr.rf_channel = channel;

        return NRF_SUCCESS;
    }
}


uint32_t nrf_esb_get_rf_channel(uint32_t * p_channel)
{
    VERIFY_PARAM_NOT_NULL(p_channel);

    *p_channel = m_esb_addr.rf_channel;

    return NRF_SUCCESS;
}


uint32_t nrf_esb_set_tx_power(nrf_esb_tx_power_t tx_output_power)
{
    VERIFY_TRUE(m_nrf_esb_mainstate == NRF_ESB_STATE_IDLE, NRF_ERROR_BUSY);

    if ( m_config_local.tx_output_power != tx_output_power )
    {
        m_config_local.tx_output_power = tx_output_power;
        update_radio_tx_power();
    }

    return NRF_SUCCESS;
}


uint32_t nrf_esb_set_retransmit_delay(uint16_t delay)
{
    VERIFY_TRUE(m_nrf_esb_mainstate == NRF_ESB_STATE_IDLE, NRF_ERROR_BUSY);
    VERIFY_TRUE(delay >= NRF_ESB_RETRANSMIT_DELAY_MIN, NRF_ERROR_INVALID_PARAM);

    m_config_local.retransmit_delay = delay;
    return NRF_SUCCESS;
}


uint32_t nrf_esb_set_retransmit_count(uint16_t count)
{
    VERIFY_TRUE(m_nrf_esb_mainstate == NRF_ESB_STATE_IDLE, NRF_ERROR_BUSY);

    m_config_local.retransmit_count = count;
    return NRF_SUCCESS;
}


uint32_t nrf_esb_set_bitrate(nrf_esb_bitrate_t bitrate)
{
    VERIFY_TRUE(m_nrf_esb_mainstate == NRF_ESB_STATE_IDLE, NRF_ERROR_BUSY);

    m_config_local.bitrate = bitrate;
    return update_radio_bitrate() ? NRF_SUCCESS : NRF_ERROR_INVALID_PARAM;
}


uint32_t nrf_esb_reuse_pid(uint8_t pipe)
{
    VERIFY_TRUE(m_nrf_esb_mainstate == NRF_ESB_STATE_IDLE, NRF_ERROR_BUSY);
    VERIFY_TRUE(pipe < NRF_ESB_PIPE_COUNT, NRF_ERROR_INVALID_PARAM);

    m_pids[pipe] = (m_pids[pipe] + NRF_ESB_PID_MAX) % (NRF_ESB_PID_MAX + 1);
    return NRF_SUCCESS;
}


#ifdef NRF52832_XXAA
// Workaround neccessary on nRF52832 Rev. 1.
void NRF_ESB_BUGFIX_TIMER_IRQHandler(void)
{
    if (NRF_ESB_BUGFIX_TIMER->EVENTS_COMPARE[0])
    {
        NRF_ESB_BUGFIX_TIMER->EVENTS_COMPARE[0] = 0;

        // If the timeout timer fires and we are in the PTX receive ACK state, disable the radio
        if (m_nrf_esb_mainstate == NRF_ESB_STATE_PTX_RX_ACK)
        {
            NRF_RADIO->TASKS_DISABLE = 1;
        }
    }
}
#endif









bool nrf_esb_validate_promiscuous_frame(uint8_t * p_array, uint8_t addrlen) {
    uint8_t framelen = p_array[addrlen] >> 2; //PCF after address, PCF is 9 bits in size, first 6 bits are length field ... thus we shift to right to get length
#ifndef LOGITECH_FILTER    
    if (framelen > 32) {
#else        
    if (framelen != 22 && framelen != 0 && framelen != 10 && framelen != 5) { // logitech
#endif    
        return false; // early out if ESB frame has a length > 32, this only accounts for "old style" ESB which is bound to 32 byte max payload length
    }
    uint8_t crclen = addrlen + 1 + framelen + 2; //+1 for PCF (we ignore one bit), +2 for crc16

    return helper_array_check_crc16(p_array, crclen);
}


#define VALIDATION_SHIFT_BUF_SIZE 64
uint32_t nrf_esb_validate_promiscuous_esb_payload(nrf_esb_payload_t * p_payload) {
    uint8_t assumed_addrlen = 5; //Validation has to take RF address length of raw frames into account, thus we assume the given length in byte
    uint8_t tmpData[VALIDATION_SHIFT_BUF_SIZE];
    uint8_t tmpDataLen = p_payload->length;

    memcpy(tmpData, p_payload->data, tmpDataLen);

    // if processing takes too long RF frames are discarded
    // the shift value in the following for loop controls how often a received
    // frame is shifted for CRC check. If this value is too large, frames are dropped,
    // if it is too low, chance for detecting valid frames decreases.
    // The nrf_esb_validate_promiscuous_frame function has an early out, if determined ESB frame length
    // exceeds 32 byte, which avoids unnecessary CRC16 calculations.


    uint8_t * p_crc_match_array = NULL;

    for (uint8_t bitshift=0; bitshift<8; bitshift++) {
        for (uint8_t byteshift=0; byteshift<NRF_ESB_PROMISCUOUS_PAYLOAD_ADDITIONAL_LENGTH; byteshift++) {
            if (nrf_esb_validate_promiscuous_frame(&tmpData[byteshift], assumed_addrlen)) {
                NRF_LOG_DEBUG("Promiscuous ESB CRC match: (bitshift %d, byteshift %d)", bitshift, byteshift);
                p_crc_match_array = &tmpData[byteshift];

                break;
            }
        }

        if (p_crc_match_array != NULL) break;
        helper_array_shl(tmpData, tmpDataLen, 1); // shift whole array left by one bit
    }



    if (p_crc_match_array != NULL) {
        //wipe out old rx data
        memset(p_payload->data, 0, p_payload->length);

        uint8_t esb_len = p_crc_match_array[assumed_addrlen] >> 2; //extract length bits from the assumed Packet Control Field, which starts right behind the RF address

        //correct RF frame length if CRC match
        p_payload->length = assumed_addrlen + 1 + esb_len + 2; //final payload (5 byte address, ESB payload with dynamic length, higher 8 PCF bits, 2 byte CRC)

        //byte allign payload (throw away no_ack bit of PCF, keep the other 8 bits)
        helper_array_shl(&p_crc_match_array[assumed_addrlen+1], esb_len, 1); //shift left all bytes behind the PCF field by one bit (we loose the lowest PCF bit, which is "no ack", and thus not of interest)

#ifdef LOGITECH_FILTER
        // additional check of 8 bit unifying payload CRC (not CRC16 of ESB frame)
        if ((esb_len > 0) && !logiteacker_unifying_payload_validate_checksum(&p_crc_match_array[assumed_addrlen + 1],
                                                                             esb_len)) {
            NRF_LOG_INFO("dropped promiscuous frame with wrong logitech checksum")
            return NRF_ERROR_INVALID_DATA;
        }
#endif    


        /*
        //zero out rest of report
        memset(&tmpData[p_payload->length], 0, VALIDATION_SHIFT_BUF_SIZE - p_payload->length);
        */
        memcpy(&p_payload->data[2], p_crc_match_array, p_payload->length);
        p_payload->length += 2;
        p_payload->data[0] = p_payload->pipe; //encode rx pipe
        p_payload->data[1] = esb_len; //encode real ESB payload length

        NRF_LOG_DEBUG("Validated payload %d", p_payload->length);
        NRF_LOG_HEXDUMP_DEBUG(p_payload->data, p_payload->length);

        return NRF_SUCCESS;
    } else {
        return NRF_ERROR_INVALID_DATA;
    }
}

bool nrf_esb_is_in_promiscuous_mode() {
    return m_config_local.mode == NRF_ESB_MODE_PROMISCOUS;
}

uint32_t nrf_esb_convert_pipe_to_address(uint8_t pipeNum, uint8_t *p_dst) {
    if (pipeNum > 8) return NRF_ERROR_INVALID_PARAM;
    if (p_dst == NULL) return NRF_ERROR_INVALID_PARAM;

    //ToDo: account for address length, currently length 5 is assumed

    if (pipeNum == 0) {
        p_dst[0] = m_esb_addr.base_addr_p0[3];
        p_dst[1] = m_esb_addr.base_addr_p0[2];
        p_dst[2] = m_esb_addr.base_addr_p0[1];
        p_dst[3] = m_esb_addr.base_addr_p0[0];
    } else {
        p_dst[0] = m_esb_addr.base_addr_p1[3];
        p_dst[1] = m_esb_addr.base_addr_p1[2];
        p_dst[2] = m_esb_addr.base_addr_p1[1];
        p_dst[3] = m_esb_addr.base_addr_p1[0];
    }

    p_dst[4] = m_esb_addr.pipe_prefixes[pipeNum];

    return NRF_SUCCESS;
}

uint32_t nrf_esb_update_channel_frequency_table(uint8_t * values, uint8_t length) {
    VERIFY_TRUE(m_nrf_esb_mainstate == NRF_ESB_STATE_IDLE, NRF_ERROR_BUSY);
    VERIFY_TRUE(length <= 100, NRF_ERROR_INVALID_PARAM);
    memcpy(m_esb_addr.channel_to_frequency, values, length);
    m_esb_addr.channel_to_frequency_len = length;

    // reset frequency to first channel
    m_esb_addr.rf_channel = 0;

    return NRF_SUCCESS;
}

uint32_t nrf_esb_update_channel_frequency_table_unifying() {
    uint8_t unifying_frequencies[25] = { 5,8,11,14,17,20,23,26,29,32,35,38,41,44,47,50,53,56,59,62,65,68,71,74,77 };
    uint8_t unifying_frequencies_len = 25;
    return nrf_esb_update_channel_frequency_table(unifying_frequencies, unifying_frequencies_len);
}

uint32_t nrf_esb_update_channel_frequency_table_unifying_reduced() {
    uint8_t unifying_frequencies[12] = { 5,8,14,17,32,35,41,44,62,65,71,74 };
    uint8_t unifying_frequencies_len = 25;
    return nrf_esb_update_channel_frequency_table(unifying_frequencies, unifying_frequencies_len);
}

uint32_t nrf_esb_update_channel_frequency_table_unifying_pairing() {
    uint8_t unifying_frequencies[11] = { 62,8,35,65,14,41,71,17,44,74,5 };
    uint8_t unifying_frequencies_len = 11;
    return nrf_esb_update_channel_frequency_table(unifying_frequencies, unifying_frequencies_len);
}

uint32_t nrf_esb_update_channel_frequency_table_all() {
    uint8_t all_frequencies[101] = { 0 };
    for (uint8_t i; i<sizeof(all_frequencies); i++) all_frequencies[i] = i;
    return nrf_esb_update_channel_frequency_table(all_frequencies, sizeof(all_frequencies));
}

uint32_t nrf_esb_set_rf_channel_next() {
    uint32_t next_channel = (m_esb_addr.rf_channel + 1) % m_esb_addr.channel_to_frequency_len;
    return nrf_esb_set_rf_channel(next_channel);
}

uint32_t nrf_esb_get_rf_frequency(uint32_t * p_frequency)
{
    VERIFY_PARAM_NOT_NULL(p_frequency);

    *p_frequency = m_esb_addr.channel_to_frequency[m_esb_addr.rf_channel];

    return NRF_SUCCESS;
}


uint32_t nrf_esb_enable_all_channel_tx_failover(bool enabled) {
    VERIFY_TRUE(m_nrf_esb_mainstate == NRF_ESB_STATE_IDLE, NRF_ERROR_BUSY);
    m_config_local.retransmit_on_all_channels = enabled;
    return NRF_SUCCESS;
}

uint32_t nrf_esb_set_all_channel_tx_failover_loop_count(uint8_t loop_count) {
    VERIFY_TRUE(m_nrf_esb_mainstate == NRF_ESB_STATE_IDLE, NRF_ERROR_BUSY);
    m_config_local.retransmit_on_all_channels_loop_count = loop_count;
    return NRF_SUCCESS;
}

uint32_t nrf_esb_set_retransmit_count_and_delay_for_single_tx(uint16_t count, uint16_t delay) {
    VERIFY_TRUE(m_nrf_esb_mainstate == NRF_ESB_STATE_IDLE, NRF_ERROR_BUSY);
    m_config_local.retransmit_count = count;
    m_config_local.retransmit_delay = delay;
    return NRF_SUCCESS;
}
