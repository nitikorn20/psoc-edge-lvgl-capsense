#ifndef WIFI_MANAGER_V2_STATE_H
#define WIFI_MANAGER_V2_STATE_H

#include "wifi_manager_v2_types.h"

#include <stdbool.h>
#include <stdint.h>

void wifi_manager_v2_state_init(void);
void wifi_manager_v2_state_poll(wifi_manager_v2_snapshot_t *snapshot);

void wifi_manager_v2_request_scan(void);
void wifi_manager_v2_request_disconnect(void);
void wifi_manager_v2_request_status(void);
void wifi_manager_v2_request_clear_profile(void);
void wifi_manager_v2_request_connect(const char *ssid, const char *password, const char *security_text);

bool wifi_manager_v2_is_connected(void);
bool wifi_manager_v2_is_scanning(void);
bool wifi_manager_v2_is_connecting(void);

#endif /* WIFI_MANAGER_V2_STATE_H */
