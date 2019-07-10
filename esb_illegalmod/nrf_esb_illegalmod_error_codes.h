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

#ifndef __NRF_ESB_ILLEGAL_ERROR_CODES_H__
#define __NRF_ESB_ILLEGAL_ERROR_CODES_H__

#ifdef __cplusplus
extern "C" {
#endif

#define     NRF_ERROR_BUFFER_EMPTY              (0x0100)

#define     NRF_ESB_ERROR_NOT_IN_RX_MODE        (0x0101)


#ifdef __cplusplus
}
#endif

#endif
