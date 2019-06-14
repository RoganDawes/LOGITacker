#ifndef LOGITACKER_TX_PAYLOAD_PROVIDER_STRING_TO_ALTKEYS_H
#define LOGITACKER_TX_PAYLOAD_PROVIDER_STRING_TO_ALTKEYS_H

#include "logitacker_tx_payload_provider.h"
#include "logitacker_devices.h"

logitacker_tx_payload_provider_t * new_payload_provider_altstring(logitacker_devices_unifying_device_t * p_device_caps, char const *str);

#endif //LOGITACKER_TX_PAYLOAD_PROVIDER_STRING_TO_ALTKEYS_H