#ifndef LOGITACKER_TX_PAYLOAD_PROVIDER_H
#define LOGITACKER_TX_PAYLOAD_PROVIDER_H

#include "logitacker_radio.h"
#include "nrf_esb_illegalmod.h"
#include "bsp.h"


typedef struct logitacker_tx_payload_provider_struct logitacker_tx_payload_provider_t;

/*
typedef void (*p_logitacker_processor_reset_func)(logitacker_processor_t *p_processor);
typedef void (*p_logitacker_processor_init_func)(logitacker_processor_t *p_processor); //does nothing but taking and assigning the context
typedef void (*p_logitacker_processor_deinit_func)(logitacker_processor_t *p_processor); //does nothing but taking and assigning the context
typedef void (*p_logitacker_processor_timer_handler)(logitacker_processor_t *p_processor, void *p_timer_ctx);
typedef void (*p_logitacker_processor_esb_handler)(logitacker_processor_t *p_processor, nrf_esb_evt_t *p_esb_event);
typedef void (*p_logitacker_processor_radio_handler)(logitacker_processor_t *p_processor, radio_evt_t const *p_event);
typedef void (*p_logitacker_processor_bsp_handler)(logitacker_processor_t *p_processor, bsp_event_t event);
*/

typedef bool (*p_logitacker_tx_payload_provider_get_next)(logitacker_tx_payload_provider_t * self, nrf_esb_payload_t *p_next_payload);
typedef void (*p_logitacker_tx_payload_provider_reset)(logitacker_tx_payload_provider_t * self);

typedef struct logitacker_tx_payload_provider_struct{
    void *p_ctx;
    p_logitacker_tx_payload_provider_get_next p_get_next;
    p_logitacker_tx_payload_provider_reset p_reset;
} logitacker_tx_payload_provider_t;

#endif //LOGITACKER_TX_PAYLOAD_PROVIDER_H
