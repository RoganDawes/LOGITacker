#ifndef LOGITACKER_TX_PAYLOAD_PROVIDER_PRESS_TO_KEYS_H
#define OGITACKER_TX_PAYLOAD_PROVIDER_PRESS_TO_KEYS_H

#include "logitacker_tx_payload_provider.h"
#include "logitacker_keyboard_map.h"
#include "logitacker_devices.h"

logitacker_tx_payload_provider_t * new_payload_provider_press(logitacker_devices_unifying_device_t * p_device_caps, logitacker_keyboard_map_lang_t lang, char const *str);

#endif //LOGITACKER_TX_PAYLOAD_PROVIDER_PRESS_TO_KEYS_H
