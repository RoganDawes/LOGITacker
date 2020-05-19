#ifndef LOGITACKER_PROCESSOR_COVERT_CHANNEL_H
#define LOGITACKER_PROCESSOR_COVERT_CHANNEL_H

#include "logitacker_processor.h"
#include "app_timer.h"

#define COVERT_CHANNEL_TX_DELAY_MS_UNIFYING 20
#define COVERT_CHANNEL_TX_DELAY_MS_G900_G700 2
#define COVERT_CHANNEL_DATA_MARKER 0xba


typedef struct {
    uint8_t len;
    uint8_t data[16];

} covert_channel_payload_data_t;

logitacker_processor_t * new_processor_covert_channel(uint8_t *rf_address, app_timer_id_t timer_next_action, nrf_cli_t const * p_cli);

uint32_t logitacker_processor_covert_channel_push_tx_data(logitacker_processor_t *p_processor_covert_channel, covert_channel_payload_data_t const * p_tx_data);

#endif //LOGITACKER_PROCESSOR_COVERT_CHANNEL_H
