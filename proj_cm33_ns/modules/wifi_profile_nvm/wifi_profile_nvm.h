#ifndef WIFI_PROFILE_NVM_H
#define WIFI_PROFILE_NVM_H

#include "ipc_communication.h"

#include <stdbool.h>

bool wifi_profile_nvm_load(ipc_wifi_connect_request_t *out_profile);
bool wifi_profile_nvm_save(const ipc_wifi_connect_request_t *profile);
bool wifi_profile_nvm_clear(void);

#endif /* WIFI_PROFILE_NVM_H */
