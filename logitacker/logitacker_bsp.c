#include "logitacker.h"
#include "logitacker_bsp.h"
#include "nrf.h"
#include "bsp.h"

#define NRF_LOG_MODULE_NAME LOGITACKER_BSP
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();


void logitacker_bsp_init(bsp_event_callback_t main_bsp_handler) {
    ret_code_t ret;
    ret = bsp_init(BSP_INIT_BUTTONS, main_bsp_handler);
    APP_ERROR_CHECK(ret);
    bsp_event_to_button_action_assign(BTN_TRIGGER_ACTION, BSP_BUTTON_ACTION_RELEASE, BSP_USER_EVENT_RELEASE_0);
    bsp_event_to_button_action_assign(BTN_TRIGGER_ACTION, BSP_BUTTON_ACTION_LONG_PUSH, BSP_USER_EVENT_LONG_PRESS_0);

    /* Configure LEDs */
    bsp_board_init(BSP_INIT_LEDS);

}

