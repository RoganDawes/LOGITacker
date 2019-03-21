/**
* Copyright (c) 2018 makerdiary
* All rights reserved.
* 
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*
* * Redistributions of source code must retain the above copyright
*   notice, this list of conditions and the following disclaimer.
*
* * Redistributions in binary form must reproduce the above
*   copyright notice, this list of conditions and the following
*   disclaimer in the documentation and/or other materials provided
*   with the distribution.

* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
* OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*/
#ifndef NRF52840_MDK_H
#define NRF52840_MDK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "nrf_gpio.h"

// LEDs definitions for nRF52840-MDK
#define LEDS_NUMBER    3

#define LED_1          NRF_GPIO_PIN_MAP(0,22)
#define LED_2          NRF_GPIO_PIN_MAP(0,23)
#define LED_3          NRF_GPIO_PIN_MAP(0,24)
#define LED_START      LED_1
#define LED_STOP       LED_3

#define LEDS_ACTIVE_STATE 0

#define LEDS_LIST { LED_1, LED_2, LED_3 }

#define LEDS_INV_MASK  LEDS_MASK

#define BSP_LED_0      22
#define BSP_LED_1      23
#define BSP_LED_2      24

#define BUTTONS_NUMBER         1
#define BUTTON_1               NRF_GPIO_PIN_MAP(1,0)  // on board
#define BUTTON_PULL            NRF_GPIO_PIN_PULLUP
#define BUTTONS_ACTIVE_STATE   0

#define BUTTONS_LIST { BUTTON_1 }

#define BSP_BUTTON_0   BUTTON_1

#define RX_PIN_NUMBER  19
#define TX_PIN_NUMBER  20
#define CTS_PIN_NUMBER 0xFFFFFFFF  //NC
#define RTS_PIN_NUMBER 0xFFFFFFFF  //NC
#define HWFC           false

#define BSP_QSPI_SCK_PIN   NRF_GPIO_PIN_MAP(1,3)
#define BSP_QSPI_CSN_PIN   NRF_GPIO_PIN_MAP(1,6)
#define BSP_QSPI_IO0_PIN   NRF_GPIO_PIN_MAP(1,5)
#define BSP_QSPI_IO1_PIN   NRF_GPIO_PIN_MAP(1,4)
#define BSP_QSPI_IO2_PIN   NRF_GPIO_PIN_MAP(1,2)
#define BSP_QSPI_IO3_PIN   NRF_GPIO_PIN_MAP(1,1)


#ifdef __cplusplus
}
#endif

#endif // NRF52840_MDK_H