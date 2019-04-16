#ifndef LOGITACKER_BSP_H__
#define LOGITACKER_BSP_H__

#include <stdint.h>
#include "nrf_esb_illegalmod.h"
#include "bsp.h"

#ifdef NRF52840_MDK_DONGLE
//makerdiary nRF52840 MDK USB dongle
#define LED_R       BSP_BOARD_LED_1
#define LED_G       BSP_BOARD_LED_0
#define LED_B       BSP_BOARD_LED_2

#elif NRF52840_MDK
//makerdiary nRF52840 MDK
#define LED_R       BSP_BOARD_LED_1
#define LED_G       BSP_BOARD_LED_0
#define LED_B       BSP_BOARD_LED_2

#elif BOARD_PCA10059
// Nordic nRF52840 USB dongle
#define LED_R       BSP_BOARD_LED_1
#define LED_G       BSP_BOARD_LED_2
#define LED_B       BSP_BOARD_LED_3

#else
#error "No supported board found"
#endif

#define BTN_TRIGGER_ACTION   0

// custom key events
 enum {
    BSP_USER_EVENT_RELEASE_0 = BSP_EVENT_KEY_LAST + 1, // Button 0 released
    BSP_USER_EVENT_LONG_PRESS_0,                       // Button 0 long press
};


void logitacker_bsp_init(bsp_event_callback_t main_bsp_handler);

#endif