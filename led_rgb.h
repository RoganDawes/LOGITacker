#ifndef LED_RGB_H__
#define LED_RGB_H__

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


#endif //LED_RGB_H__
