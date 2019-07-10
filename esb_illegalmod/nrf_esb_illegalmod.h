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

#ifndef __NRF_ESB_ILLEGALMOD_H
#define __NRF_ESB_ILLEGALMOD_H

#include <stdbool.h>
#include <stdint.h>
#include "nrf.h"
#include "app_util.h"


#ifdef __cplusplus
extern "C" {
#endif

#define NRF_ESB_PROMISCUOUS_PAYLOAD_ADDITIONAL_LENGTH 18

/*
 * In promiscuous mode, all captured frames produce a NRF_ESB_EVENT_RX_RECEIVED event
 *
 * If NRF_ESB_CHECK_PROMISCUOUS_CRC_WITH_APP_SCHEDULER is enabled, promiscuous mode frames are additionally
 * processed to check CRC16 and bitshift the whole frame several times (and check CRC again) to raise the
 * chance of capturing valid frames. This requires that the main loop processes app_scheduler. ONLY in case a valid frame
 * is found, an NRF_ESB_EVENT_RX_RECEIVED event is generated in promiscuous mode and the validated frame
 * gets queued in the RX fifo (shifted corerectly).
 * 
*/
#define     NRF_ESB_CHECK_PROMISCUOUS_CRC_WITH_APP_SCHEDULER        true // if true, promiscuous mode frames are validated using app_scheduler ()
#define     NRF_ESB_CHECK_PROMISCUOUS_SCHED_EVENT_DATA_SIZE         sizeof(nrf_esb_payload_t)
#define     NRF_ESB_CHECK_PROMISCUOUS_CRC_WITH_APP_SCHEDULER_ENQUEUE_INVALID true // if enabled, invalid frames are send to RX fifo, too (but validated_promiscuous_frame flag won't be set)

#define     NRF_ESB_RETRANSMIT_DELAY_MIN        135

// Hardcoded parameters - change if necessary
#ifndef NRF_ESB_MAX_PAYLOAD_LENGTH
#define     NRF_ESB_MAX_PAYLOAD_LENGTH          32                  //!< The maximum size of the payload. Valid values are 1 to 252.
#endif

#define     NRF_ESB_TX_FIFO_SIZE                8                   //!< The size of the transmission first-in, first-out buffer.
#define     NRF_ESB_RX_FIFO_SIZE                16                   //!< The size of the reception first-in, first-out buffer.

// 252 is the largest possible payload size according to the nRF5 architecture.
STATIC_ASSERT(NRF_ESB_MAX_PAYLOAD_LENGTH <= 252);

#define     NRF_ESB_SYS_TIMER                   NRF_TIMER2          //!< The timer that is used by the module.
#define     NRF_ESB_SYS_TIMER_IRQ_Handler       TIMER2_IRQHandler   //!< The handler that is used by @ref NRF_ESB_SYS_TIMER.

#define     NRF_ESB_PPI_TIMER_START             10                  //!< The PPI channel used for starting the timer.
#define     NRF_ESB_PPI_TIMER_STOP              11                  //!< The PPI channel used for stopping the timer.
#define     NRF_ESB_PPI_RX_TIMEOUT              12                  //!< The PPI channel used for RX time-out.
#define     NRF_ESB_PPI_TX_START                13                  //!< The PPI channel used for starting TX.

#ifndef NRF_ESB_PIPE_COUNT
#define     NRF_ESB_PIPE_COUNT                  8                   //!< The maximum number of pipes allowed in the API, can be used if you need to restrict the number of pipes used. Must be 8 or lower because of architectural limitations.
#endif
STATIC_ASSERT(NRF_ESB_PIPE_COUNT <= 8);

/**@cond NO_DOXYGEN */
#ifdef NRF52832_XXAA
// nRF52 address fix timer and PPI defines
#define     NRF_ESB_PPI_BUGFIX1                 9
#define     NRF_ESB_PPI_BUGFIX2                 8
#define     NRF_ESB_PPI_BUGFIX3                 7

#define     NRF_ESB_BUGFIX_TIMER                NRF_TIMER3
#define     NRF_ESB_BUGFIX_TIMER_IRQn           TIMER3_IRQn
#define     NRF_ESB_BUGFIX_TIMER_IRQHandler     TIMER3_IRQHandler
#endif

/** @endcond */

// Interrupt flags
#define     NRF_ESB_INT_TX_SUCCESS_MSK          0x01                //!< The flag used to indicate a success since the last event.
#define     NRF_ESB_INT_TX_FAILED_MSK           0x02                //!< The flag used to indicate a failure since the last event.
#define     NRF_ESB_INT_RX_DR_MSK               0x04                //!< The flag used to indicate that a packet was received since the last event.

#define     NRF_ESB_PID_RESET_VALUE             0xFF                //!< Invalid PID value that is guaranteed to not collide with any valid PID value.
#define     NRF_ESB_PID_MAX                     3                   //!< The maximum value for PID.
#define     NRF_ESB_CRC_RESET_VALUE             0xFFFF              //!< The CRC reset value.

#define     ESB_EVT_IRQ                         SWI0_IRQn           //!< The ESB event IRQ number when running on an nRF5 device.
#define     ESB_EVT_IRQHandler                  SWI0_IRQHandler     //!< The handler for @ref ESB_EVT_IRQ when running on an nRF5 device.

#if defined(NRF52)
#define ESB_IRQ_PRIORITY_MSK                    0x07                //!< The mask used to enforce a valid IRQ priority.
#else
#define ESB_IRQ_PRIORITY_MSK                    0x03                //!< The mask used to enforce a valid IRQ priority.
#endif

/** @brief Default address configuration for ESB. 
 *  @details Roughly equal to the nRF24Lxx default (except for the number of pipes, because more pipes are supported). */
#define NRF_ESB_ADDR_DEFAULT                                                    \
{                                                                               \
    .base_addr_p0       = { 0xE7, 0xE7, 0xE7, 0xE7 },                           \
    .base_addr_p1       = { 0xC2, 0xC2, 0xC2, 0xC2 },                           \
    .pipe_prefixes      = { 0xE7, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8 },   \
    .addr_length        = 5,                                                    \
    .num_pipes          = NRF_ESB_PIPE_COUNT,                                   \
    .rf_channel         = 2,                                                    \
    .rx_pipes_enabled   = 0xFF,                                                 \
    .channel_to_frequency = {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x5b,0x5c,0x5d,0x5e,0x5f,0x60,0x61,0x62,0x63,0x64}, \
    .channel_to_frequency_len = 101 \
}


/** @brief Default radio parameters. 
 *  @details Roughly equal to the nRF24Lxx default parameters (except for CRC, which is set to 16 bit, and protocol, which is set to DPL). */
#define NRF_ESB_DEFAULT_CONFIG {.protocol               = NRF_ESB_PROTOCOL_ESB_DPL,         \
                                .mode                   = NRF_ESB_MODE_PTX,                 \
                                .event_handler          = 0,                                \
                                .bitrate                = NRF_ESB_BITRATE_2MBPS,            \
                                .crc                    = NRF_ESB_CRC_16BIT,                \
                                .tx_output_power        = NRF_ESB_TX_POWER_8DBM,            \
                                .retransmit_delay       = 250,                              \
                                .retransmit_count       = 3,                                \
                                .tx_mode                = NRF_ESB_TXMODE_AUTO,              \
                                .radio_irq_priority     = 1,                                \
                                .event_irq_priority     = 2,                                \
                                .payload_length         = 32,                               \
                                .selective_auto_ack     = false,                            \
                                .disallow_auto_ack      = false                             \
}

#define NRF_ESB_SNIFF_CONFIG   {.protocol               = NRF_ESB_PROTOCOL_ESB_DPL,         \
                                .mode                   = NRF_ESB_MODE_SNIFF,               \
                                .event_handler          = 0,                                \
                                .bitrate                = NRF_ESB_BITRATE_2MBPS,            \
                                .crc                    = NRF_ESB_CRC_16BIT,                \
                                .tx_output_power        = NRF_ESB_TX_POWER_8DBM,            \
                                .retransmit_delay       = 250,                              \
                                .retransmit_count       = 3,                                \
                                .tx_mode                = NRF_ESB_TXMODE_AUTO,              \
                                .radio_irq_priority     = 1,                                \
                                .event_irq_priority     = 2,                                \
                                .payload_length         = 32,                               \
                                .selective_auto_ack     = true,                            \
                                .disallow_auto_ack      = true                             \
}


#define NRF_ESB_PROMISCUOUS_CONFIG {.protocol               = NRF_ESB_PROTOCOL_ESB_PROMISCUOUS,     \
                                .mode                   = NRF_ESB_MODE_PROMISCOUS,          \
                                .event_handler          = 0,                                \
                                .bitrate                = NRF_ESB_BITRATE_2MBPS,            \
                                .crc                    = NRF_ESB_CRC_OFF,                  \
                                .tx_output_power        = NRF_ESB_TX_POWER_8DBM,            \
                                .retransmit_delay       = 250,                              \
                                .retransmit_count       = 3,                                \
                                .tx_mode                = NRF_ESB_TXMODE_AUTO,              \
                                .radio_irq_priority     = 1,                                \
                                .event_irq_priority     = 2,                                \
                                .payload_length         = 32 + NRF_ESB_PROMISCUOUS_PAYLOAD_ADDITIONAL_LENGTH,                               \
                                .selective_auto_ack     = true,                             \
                                .disallow_auto_ack      = true                              \
}


/** @brief Macro to create an initializer for a TX data packet.
 *
 * @details This macro generates an initializer. Using the initializer is more efficient
 *          than setting the individual parameters dynamically.
 *
 * @param[in]   _pipe   The pipe to use for the data packet.
 * @param[in]   ...     Comma separated list of character data to put in the TX buffer.
 *                      Supported values consist of 1 to 63 characters.
 *
 * @return  Initializer that sets up the pipe, length, and byte array for content of the TX data.
 */
#define NRF_ESB_CREATE_PAYLOAD(_pipe, ...)                                                  \
        {.pipe = _pipe, .length = NUM_VA_ARGS(__VA_ARGS__), .data = {__VA_ARGS__}};         \
        STATIC_ASSERT(NUM_VA_ARGS(__VA_ARGS__) > 0 && NUM_VA_ARGS(__VA_ARGS__) <= 63)


/**@brief Enhanced ShockBurst protocols. */
typedef enum {
    NRF_ESB_PROTOCOL_ESB_DPL,   /**< Enhanced ShockBurst with dynamic payload length.                                          */
    NRF_ESB_PROTOCOL_ESB_PROMISCUOUS   /**< illegal addr len 1, static payload length                                          */
} nrf_esb_protocol_t;


/**@brief Enhanced ShockBurst modes. */
typedef enum {
    NRF_ESB_MODE_PTX,          /**< Primary transmitter mode. */
    NRF_ESB_MODE_PTX_STAY_RX,  // Works like PTX but stays in RX mode after successful TX (and reports RX_RECEIVED events)
    NRF_ESB_MODE_PRX,           /**< Primary receiver mode.    */
    NRF_ESB_MODE_PROMISCOUS,
    NRF_ESB_MODE_SNIFF
} nrf_esb_mode_t;


/**@brief Enhanced ShockBurst bitrate modes. */
typedef enum {
    NRF_ESB_BITRATE_2MBPS     = RADIO_MODE_MODE_Nrf_2Mbit,      /**< 2 Mb radio mode.                                                */
    NRF_ESB_BITRATE_1MBPS     = RADIO_MODE_MODE_Nrf_1Mbit,      /**< 1 Mb radio mode.                                                */
#if !(defined(NRF52840_XXAA) || defined(NRF52810_XXAA))
    NRF_ESB_BITRATE_250KBPS   = RADIO_MODE_MODE_Nrf_250Kbit,    /**< 250 Kb radio mode.                                              */
#endif //NRF52840_XXAA
    NRF_ESB_BITRATE_1MBPS_BLE = RADIO_MODE_MODE_Ble_1Mbit,      /**< 1 Mb radio mode using @e Bluetooth low energy radio parameters. */
#if defined(NRF52_SERIES)
    NRF_ESB_BITRATE_2MBPS_BLE = 4                               /**< 2 Mb radio mode using @e Bluetooth low energy radio parameters. */
#endif
} nrf_esb_bitrate_t;


/**@brief Enhanced ShockBurst CRC modes. */
typedef enum {
    NRF_ESB_CRC_16BIT = RADIO_CRCCNF_LEN_Two,                   /**< Use two-byte CRC. */
    NRF_ESB_CRC_8BIT  = RADIO_CRCCNF_LEN_One,                   /**< Use one-byte CRC. */
    NRF_ESB_CRC_OFF   = RADIO_CRCCNF_LEN_Disabled               /**< Disable CRC.      */
} nrf_esb_crc_t;


/**@brief Enhanced ShockBurst radio transmission power modes. */
typedef enum {
    NRF_ESB_TX_POWER_8DBM     = RADIO_TXPOWER_TXPOWER_Pos8dBm,  /**< 4 dBm radio transmit power.   */
    NRF_ESB_TX_POWER_4DBM     = RADIO_TXPOWER_TXPOWER_Pos4dBm,  /**< 4 dBm radio transmit power.   */
#if defined(NRF52)
    NRF_ESB_TX_POWER_3DBM     = RADIO_TXPOWER_TXPOWER_Pos3dBm,  /**< 3 dBm radio transmit power.   */
#endif
    NRF_ESB_TX_POWER_0DBM     = RADIO_TXPOWER_TXPOWER_0dBm,     /**< 0 dBm radio transmit power.   */
    NRF_ESB_TX_POWER_NEG4DBM  = RADIO_TXPOWER_TXPOWER_Neg4dBm,  /**< -4 dBm radio transmit power.  */
    NRF_ESB_TX_POWER_NEG8DBM  = RADIO_TXPOWER_TXPOWER_Neg8dBm,  /**< -8 dBm radio transmit power.  */
    NRF_ESB_TX_POWER_NEG12DBM = RADIO_TXPOWER_TXPOWER_Neg12dBm, /**< -12 dBm radio transmit power. */
    NRF_ESB_TX_POWER_NEG16DBM = RADIO_TXPOWER_TXPOWER_Neg16dBm, /**< -16 dBm radio transmit power. */
    NRF_ESB_TX_POWER_NEG20DBM = RADIO_TXPOWER_TXPOWER_Neg20dBm, /**< -20 dBm radio transmit power. */
    NRF_ESB_TX_POWER_NEG30DBM = RADIO_TXPOWER_TXPOWER_Neg30dBm, /**< -30 dBm radio transmit power. */
    NRF_ESB_TX_POWER_NEG40DBM = RADIO_TXPOWER_TXPOWER_Neg40dBm  /**< -40 dBm radio transmit power. */
} nrf_esb_tx_power_t;


/**@brief Enhanced ShockBurst transmission modes. */
typedef enum {
    NRF_ESB_TXMODE_AUTO,        /**< Automatic TX mode: When the TX FIFO contains packets and the radio is idle, packets are sent automatically. */
    NRF_ESB_TXMODE_MANUAL,      /**< Manual TX mode: Packets are not sent until @ref nrf_esb_start_tx is called. This mode can be used to ensure consistent packet timing. */
    NRF_ESB_TXMODE_MANUAL_START /**< Manual start TX mode: Packets are not sent until @ref nrf_esb_start_tx is called. Then, transmission continues automatically until the TX FIFO is empty. */
} nrf_esb_tx_mode_t;


/**@brief Enhanced ShockBurst event IDs used to indicate the type of the event. */
typedef enum
{
    NRF_ESB_EVENT_TX_SUCCESS,   /**< Event triggered on TX success.     */
    NRF_ESB_EVENT_TX_FAILED,    /**< Event triggered on TX failure.     */
    NRF_ESB_EVENT_RX_RECEIVED,   /**< Event triggered on RX received.    */
    NRF_ESB_EVENT_TX_SUCCESS_ACK_PAY, // TX success with ack pay len > 0
    //NRF_ESB_EVENT_RX_RECEIVED_PROMISCUOUS_UNVALIDATED   /**< Event triggered on RX in PROMISCUOUS mode.    */
} nrf_esb_evt_id_t;


/**@brief Enhanced ShockBurst payload.
 *
 * @details The payload is used both for transmissions and for acknowledging a
 *          received packet with a payload.
*/
typedef struct
{
    uint8_t length;                                 //!< Length of the packet (maximum value is @ref NRF_ESB_MAX_PAYLOAD_LENGTH).
    uint8_t pipe;                                   //!< Pipe used for this payload.
    int8_t  rssi;                                   //!< RSSI for the received packet.
    uint8_t noack;                                  //!< Flag indicating that this packet will not be acknowledgement. Flag is ignored when selective auto ack is enabled.
    uint8_t pid;                                    //!< PID assigned during communication.
    uint8_t rx_channel_index;
    uint8_t rx_channel;
    uint8_t data[NRF_ESB_MAX_PAYLOAD_LENGTH + 30];       //!< The payload data.
    bool    validated_promiscuous_frame;
} nrf_esb_payload_t;


/**@brief Enhanced ShockBurst event. */
typedef struct
{
    nrf_esb_evt_id_t    evt_id;                     //!< Enhanced ShockBurst event ID.
    uint32_t            tx_attempts;                //!< Number of TX retransmission attempts.
} nrf_esb_evt_t;


/**@brief Definition of the event handler for the module. */
//typedef void (* nrf_esb_event_handler_t)(nrf_esb_evt_t const * p_event);
typedef void (* nrf_esb_event_handler_t)(nrf_esb_evt_t * p_event);


/**@brief Main configuration structure for the module. */
typedef struct
{
    nrf_esb_protocol_t      protocol;               //!< Enhanced ShockBurst protocol.
    nrf_esb_mode_t          mode;                   //!< Enhanced ShockBurst mode.
    nrf_esb_event_handler_t event_handler;          //!< Enhanced ShockBurst event handler.

    // General RF parameters
    nrf_esb_bitrate_t       bitrate;                //!< Enhanced ShockBurst bitrate mode.
    nrf_esb_crc_t           crc;                    //!< Enhanced ShockBurst CRC mode.

    nrf_esb_tx_power_t      tx_output_power;        //!< Enhanced ShockBurst radio transmission power mode.

    uint16_t                retransmit_delay;       //!< The delay between each retransmission of unacknowledged packets.
    uint16_t                retransmit_count;       //!< The number of retransmission attempts before transmission fail.

    // Control settings
    nrf_esb_tx_mode_t       tx_mode;                //!< Enhanced ShockBurst transmission mode.

    uint8_t                 radio_irq_priority;     //!< nRF radio interrupt priority.
    uint8_t                 event_irq_priority;     //!< ESB event interrupt priority.
    uint8_t                 payload_length;         //!< Length of the payload (maximum length depends on the platforms that are used on each side).

    bool                    retransmit_on_all_channels; //if enabled, TX_FAILED event is only fired, if TX was tried on all channels, radio remains on last channel in use 
    uint8_t                 retransmit_on_all_channels_loop_count; //how often retransmission should iterate over channels

    bool                    selective_auto_ack;     //!< Enable or disable selective auto acknowledgement. When this feature is disabled, all packets will be acknowledged ignoring the noack field.
    bool                    disallow_auto_ack;      //!< If enabled nor ack is sent back in PRX mode, even if the noack field of received frame was not set.
} nrf_esb_config_t;


/**@brief Function for initializing the Enhanced ShockBurst module.
 *
 * @param  p_config     Parameters for initializing the module.
 *
 * @retval  NRF_SUCCESS             If initialization was successful.
 * @retval  NRF_ERROR_NULL          If the @p p_config argument was NULL.
 * @retval  NRF_ERROR_BUSY          If the function failed because the radio is busy.
 */
uint32_t nrf_esb_init(nrf_esb_config_t const * p_config);


/**@brief Function for suspending the Enhanced ShockBurst module.
 *
 * Calling this function stops ongoing communications without changing the queues.
 *
 * @retval  NRF_SUCCESS             If Enhanced ShockBurst was suspended.
 * @retval  NRF_ERROR_BUSY          If the function failed because the radio is busy.
 */
uint32_t nrf_esb_suspend(void);


/**@brief Function for disabling the Enhanced ShockBurst module.
 *
 * Calling this function disables the Enhanced ShockBurst module immediately.
 * Doing so might stop ongoing communications.
 *
 * @note All queues are flushed by this function.
 *
 * @retval  NRF_SUCCESS             If Enhanced ShockBurst was disabled.
 */
uint32_t nrf_esb_disable(void);


/**@brief Function for checking if the Enhanced ShockBurst module is idle.
 *
 * @retval true                     If the module is idle.
 * @retval false                    If the module is busy.
 */
bool nrf_esb_is_idle(void);


/**@brief Function for writing a payload for transmission or acknowledgement.
 *
 * This function writes a payload that is added to the queue. When the module is in PTX mode, the
 * payload is queued for a regular transmission. When the module is in PRX mode, the payload
 * is queued for when a packet is received that requires an acknowledgement with payload.
 *
 * @param[in]   p_payload     Pointer to the structure that contains information and state of the payload.
 *
 * @retval  NRF_SUCCESS                     If the payload was successfully queued for writing.
 * @retval  NRF_ERROR_NULL                  If the required parameter was NULL.
 * @retval  NRF_INVALID_STATE               If the module is not initialized.
 * @retval  NRF_ERROR_NO_MEM                If the TX FIFO is full.
 * @retval  NRF_ERROR_INVALID_LENGTH        If the payload length was invalid (zero or larger than the allowed maximum).
 */
uint32_t nrf_esb_write_payload(nrf_esb_payload_t const * p_payload);


/**@brief Function for reading an RX payload.
 *
 * @param[in,out]   p_payload   Pointer to the structure that contains information and state of the payload.
 *
 * @retval  NRF_SUCCESS                     If the data was read successfully.
 * @retval  NRF_ERROR_NULL                  If the required parameter was NULL.
 * @retval  NRF_INVALID_STATE               If the module is not initialized.
 */
uint32_t nrf_esb_read_rx_payload(nrf_esb_payload_t * p_payload);


/**@brief Function for starting transmission.
 *
 * @retval  NRF_SUCCESS                     If the TX started successfully.
 * @retval  NRF_ERROR_BUFFER_EMPTY          If the TX did not start because the FIFO buffer is empty.
 * @retval  NRF_ERROR_BUSY                  If the function failed because the radio is busy.
 */
uint32_t nrf_esb_start_tx(void);


/**@brief Function for starting to transmit data from the FIFO buffer.
 *
 * @retval  NRF_SUCCESS                     If the transmission was started successfully.
 * @retval  NRF_ERROR_BUSY                  If the function failed because the radio is busy.
 */
uint32_t nrf_esb_start_rx(void);


/** @brief Function for stopping data reception.
 *
 * @retval  NRF_SUCCESS                     If data reception was stopped successfully.
 * @retval  NRF_ESB_ERROR_NOT_IN_RX_MODE    If the function failed because the module is not in RX mode.
 */
uint32_t nrf_esb_stop_rx(void);


/**@brief Function for removing remaining items from the TX buffer.
 *
 * This function clears the TX FIFO buffer.
 *
 * @retval  NRF_SUCCESS                     If pending items in the TX buffer were successfully cleared.
 * @retval  NRF_INVALID_STATE               If the module is not initialized.
 */
uint32_t nrf_esb_flush_tx(void);


/**@brief Function for removing the newest entry from the TX buffer.
 *
 * This function will remove the most recently added element from the FIFO queue.
 *
 * @retval  NRF_SUCCESS                     If the operation completed successfully.
 * @retval  NRF_INVALID_STATE               If the module is not initialized.
 * @retval  NRF_ERROR_BUFFER_EMPTY          If there are no items in the queue to remove.
 */
uint32_t nrf_esb_pop_tx(void);


/**@brief Function for removing the oldest entry from the TX buffer.
 *
 * This function will remove the next element scheduled to be sent from the TX FIFO queue.
 * This is useful if you want to skip a packet which was never acknowledged.
 *
 * @retval  NRF_SUCCESS                     If the operation completed successfully.
 * @retval  NRF_INVALID_STATE               If the module is not initialized.
 * @retval  NRF_ERROR_BUFFER_EMPTY          If there are no items in the queue to remove.
 */
uint32_t nrf_esb_skip_tx(void);


/**@brief Function for removing remaining items from the RX buffer.
 *
 * @retval  NRF_SUCCESS                     If the pending items in the RX buffer were successfully cleared.
 * @retval  NRF_INVALID_STATE               If the module is not initialized.
 */
uint32_t nrf_esb_flush_rx(void);


/**@brief Function for setting the length of the address.
 *
 * @param[in]       length              Length of the ESB address (in bytes).
 *
 * @retval  NRF_SUCCESS                      If the address length was set successfully.
 * @retval  NRF_ERROR_INVALID_PARAM          If the address length was invalid.
 * @retval  NRF_ERROR_BUSY                   If the function failed because the radio is busy.
 */
uint32_t nrf_esb_set_address_length(uint8_t length);

uint32_t nrf_esb_set_address_length_special(uint8_t length);


/**@brief Function for setting the base address for pipe 0.
 *
 * @param[in]       p_addr      Pointer to the address data.
 *
 * @retval  NRF_SUCCESS                     If the base address was set successfully.
 * @retval  NRF_ERROR_BUSY                  If the function failed because the radio is busy.
 * @retval  NRF_ERROR_INVALID_PARAM         If the function failed because the address given was too close to a zero address.
 * @retval  NRF_ERROR_NULL                  If the required parameter was NULL.
 */
uint32_t nrf_esb_set_base_address_0(uint8_t const * p_addr);


/**@brief Function for setting the base address for pipe 1 to pipe 7.
 *
 * @param[in]       p_addr      Pointer to the address data.
 *
 * @retval  NRF_SUCCESS                     If the base address was set successfully.
 * @retval  NRF_ERROR_BUSY                  If the function failed because the radio is busy.
 * @retval  NRF_ERROR_INVALID_PARAM         If the function failed because the address given was too close to a zero address.
 * @retval  NRF_ERROR_NULL                  If the required parameter was NULL.
 */
uint32_t nrf_esb_set_base_address_1(uint8_t const * p_addr);


/**@brief Function for setting the number of pipes and the pipe prefix addresses.
 *
 * This function configures the number of available pipes, enables the pipes,
 * and sets their prefix addresses.
 *
 * @param[in]   p_prefixes      Pointer to a char array that contains the prefix for each pipe.
 * @param[in]   num_pipes       Number of pipes. Must be less than or equal to @ref NRF_ESB_PIPE_COUNT.
 *
 * @retval  NRF_SUCCESS                     If the prefix addresses were set successfully.
 * @retval  NRF_ERROR_BUSY                  If the function failed because the radio is busy.
 * @retval  NRF_ERROR_NULL                  If a required parameter was NULL.
 * @retval  NRF_ERROR_INVALID_PARAM         If an invalid number of pipes was given or if the address given was too close to a zero address.
 */
uint32_t nrf_esb_set_prefixes(uint8_t const * p_prefixes, uint8_t num_pipes);


/**@brief Function for enabling pipes.
 *
 * The @p enable_mask parameter must contain the same number of pipes as has been configured
 * with @ref nrf_esb_set_prefixes. This number may not be greater than the number defined by
 * @ref NRF_ESB_PIPE_COUNT
 *
 * @param   enable_mask         Bitfield mask to enable or disable pipes. Setting a bit to
 *                              0 disables the pipe. Setting a bit to 1 enables the pipe.
 *
 * @retval  NRF_SUCCESS                     If the pipes were enabled and disabled successfully.
 * @retval  NRF_ERROR_BUSY                  If the function failed because the radio is busy.
 * @retval  NRF_ERROR_INVALID_PARAM         If the function failed because the address given was too close to a zero address.
 */
uint32_t nrf_esb_enable_pipes(uint8_t enable_mask);


/**@brief Function for updating the prefix for a pipe.
 *
 * @param   pipe    Pipe for which to set the prefix.
 * @param   prefix  Prefix to set for the pipe.
 *
 * @retval  NRF_SUCCESS                         If the operation completed successfully.
 * @retval  NRF_ERROR_BUSY                      If the function failed because the radio is busy.
 * @retval  NRF_ERROR_INVALID_PARAM             If the given pipe number was invalid or if the address given was too close to a zero address.
 */
uint32_t nrf_esb_update_prefix(uint8_t pipe, uint8_t prefix);


/** @brief Function for setting the channel to use for the radio.
 *
 * The module must be in an idle state to call this function. As a PTX, the
 * application must wait for an idle state and as a PRX, the application must stop RX
 * before changing the channel. After changing the channel, operation can be resumed.
 *
 * @param[in]   channel                         Channel to use for radio.
 *
 * @retval  NRF_SUCCESS                         If the operation completed successfully.
 * @retval  NRF_INVALID_STATE                   If the module is not initialized.
 * @retval  NRF_ERROR_BUSY                      If the module was not in idle state.
 * @retval  NRF_ERROR_INVALID_PARAM             If the channel is invalid (larger than 100).
 */
uint32_t nrf_esb_set_rf_channel(uint32_t channel);


/**@brief Function for getting the current radio channel.
 *
 * @param[in, out] p_channel    Pointer to the channel data.
 *
 * @retval  NRF_SUCCESS                         If the operation completed successfully.
 * @retval  NRF_ERROR_NULL                      If the required parameter was NULL.
 */
uint32_t nrf_esb_get_rf_channel(uint32_t * p_channel);


/**@brief Function for setting the radio output power.
 *
 * @param[in]   tx_output_power    Output power.
 *
 * @retval  NRF_SUCCESS                         If the operation completed successfully.
 * @retval  NRF_ERROR_BUSY                      If the function failed because the radio is busy.
 */
uint32_t nrf_esb_set_tx_power(nrf_esb_tx_power_t tx_output_power);


/**@brief Function for setting the packet retransmit delay.
 *
 * @param[in]   delay                           Delay between retransmissions.
 *
 * @retval  NRF_SUCCESS                         If the operation completed successfully.
 * @retval  NRF_ERROR_BUSY                      If the function failed because the radio is busy.
 */
uint32_t nrf_esb_set_retransmit_delay(uint16_t delay);


/**@brief Function for setting the number of retransmission attempts.
 *
 * @param[in]   count                           Number of retransmissions.
 *
 * @retval  NRF_SUCCESS                         If the operation completed successfully.
 * @retval  NRF_ERROR_BUSY                      If the function failed because the radio is busy.
 */
uint32_t nrf_esb_set_retransmit_count(uint16_t count);


/**@brief Function for setting the radio bitrate.
 *
 * @param[in]   bitrate                         Radio bitrate.
 *
 * @retval  NRF_SUCCESS                         If the operation completed successfully.
 * @retval  NRF_ERROR_BUSY                      If the function failed because the radio is busy.
 */
uint32_t nrf_esb_set_bitrate(nrf_esb_bitrate_t bitrate);


/**@brief Function for reusing a packet ID for a specific pipe.
 *
 * The ESB protocol uses a 2-bit sequence number (packet ID) to identify
 * retransmitted packets. By default, the packet ID is incremented for every 
 * uploaded packet. Use this function to prevent this and send two different 
 * packets with the same packet ID.
 *
 * @param[in]   pipe                            Pipe.
 *
 * @retval  NRF_SUCCESS                         If the operation completed successfully.
 * @retval  NRF_ERROR_BUSY                      If the function failed because the radio is busy.
 */
uint32_t nrf_esb_reuse_pid(uint8_t pipe);

/** @} */

uint32_t nrf_esb_validate_promiscuous_esb_payload(nrf_esb_payload_t * p_payload);
bool nrf_esb_is_in_promiscuous_mode();
uint32_t nrf_esb_convert_pipe_to_address(uint8_t pipeNum, uint8_t *p_dst);


uint32_t nrf_esb_init_promiscuous_mode();
uint32_t nrf_esb_init_sniffer_mode();
uint32_t nrf_esb_init_ptx_mode();
uint32_t nrf_esb_set_mode(nrf_esb_mode_t mode);
nrf_esb_mode_t nrf_esb_get_mode();

// channel and frequency handling (works with translation tables)
uint32_t nrf_esb_update_channel_frequency_table(uint8_t * values, uint8_t length);
uint32_t nrf_esb_update_channel_frequency_table_unifying();
uint32_t nrf_esb_update_channel_frequency_table_unifying_reduced();
uint32_t nrf_esb_update_channel_frequency_table_unifying_pairing();
uint32_t nrf_esb_update_channel_frequency_table_all();
uint32_t nrf_esb_set_rf_channel_next();
uint32_t nrf_esb_get_rf_frequency(uint32_t * p_frequency);

uint32_t nrf_esb_enable_all_channel_tx_failover(bool enabled);
uint32_t nrf_esb_set_all_channel_tx_failover_loop_count(uint8_t loop_count); //how often re-transmission should loop over available channels
uint32_t nrf_esb_set_retransmit_count_and_delay_for_single_tx(uint16_t count, uint16_t delay);

#ifdef __cplusplus
}
#endif

#endif // NRF_ESB
