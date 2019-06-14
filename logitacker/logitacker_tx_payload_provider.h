#ifndef LOGITACKER_TX_PAYLOAD_PROVIDER_H
#define LOGITACKER_TX_PAYLOAD_PROVIDER_H

#include "logitacker_radio.h"
#include "nrf_esb_illegalmod.h"
#include "bsp.h"


typedef struct logitacker_tx_payload_provider_struct logitacker_tx_payload_provider_t;


typedef bool (*p_logitacker_tx_payload_provider_get_next)(logitacker_tx_payload_provider_t * self, nrf_esb_payload_t *p_next_payload);
typedef void (*p_logitacker_tx_payload_provider_reset)(logitacker_tx_payload_provider_t * self);

typedef struct logitacker_tx_payload_provider_struct{
    void *p_ctx;
    p_logitacker_tx_payload_provider_get_next p_get_next;
    p_logitacker_tx_payload_provider_reset p_reset;
} logitacker_tx_payload_provider_t;

#endif //LOGITACKER_TX_PAYLOAD_PROVIDER_H
