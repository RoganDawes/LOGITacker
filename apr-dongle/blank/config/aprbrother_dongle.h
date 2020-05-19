#ifndef APRBROTHER_DONGLE_H
#define APRBROTHER_DONGLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "nrf_gpio.h"

// LED definitions for APRBROTHER dongle
// Each LED color is considered a separate LED
#define LEDS_NUMBER    4

#define LED1_G         NRF_GPIO_PIN_MAP(0,6)
#define LED2_R         NRF_GPIO_PIN_MAP(0,8)
#define LED2_G         NRF_GPIO_PIN_MAP(1,9)
#define LED2_B         NRF_GPIO_PIN_MAP(0,12)

#define LED_1          LED1_G
#define LED_2          LED2_R
#define LED_3          LED2_G
#define LED_4          LED2_B

#define LEDS_ACTIVE_STATE 0

#define LEDS_LIST { LED_1, LED_2, LED_3, LED_4 }

#define LEDS_INV_MASK  LEDS_MASK

// Totally messed, no RED at all
#define BSP_LED_0      LED_1 // blue, bright
#define BSP_LED_1      LED_2 // blue, barely noticeable
#define BSP_LED_2      LED_3 // green, only if LED_2 is on, too
#define BSP_LED_3      LED_4 // seems to toggle LED_2 somehow, without changing LED_2

// There is only one button for the application
#define BUTTONS_NUMBER 1

#define BUTTON_1       NRF_GPIO_PIN_MAP(1,6)
#define BUTTON_PULL    NRF_GPIO_PIN_PULLUP

#define BUTTONS_ACTIVE_STATE 0

#define BUTTONS_LIST { BUTTON_1 }

#define BSP_BUTTON_0   BUTTON_1

#define BSP_SELF_PINRESET_PIN NRF_GPIO_PIN_MAP(0,19)

#define HWFC           true

#ifdef __cplusplus
}
#endif

#endif // APRBROTHER_DONGLE_H