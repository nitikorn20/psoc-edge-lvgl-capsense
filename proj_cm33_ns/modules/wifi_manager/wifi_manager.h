#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "ipc_communication.h"
#include "wifi_scanner_types.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum
{
  WIFI_MANAGER_EVENT_SCAN_RESULT = 0U,   /* data = wifi_info_t[], count = n. */
  WIFI_MANAGER_EVENT_SCAN_COMPLETE = 1U, /* data = ipc_wifi_scan_complete_t. */
  WIFI_MANAGER_EVENT_STATUS = 2U         /* data = ipc_wifi_status_t. */
} wifi_manager_event_t;

typedef void (*wifi_manager_event_cb_t)(wifi_manager_event_t event, const void *data, uint32_t count, void *user_data);

/** Initializes internal state. Call before start. Returns true. */
bool wifi_manager_init(void);

/** Starts manager task, scanner, and queue. Idempotent. Returns true on success. */
bool wifi_manager_start(void);

/** Registers event callback and user_data. Replaces previous. Returns true. */
bool wifi_manager_set_event_callback(wifi_manager_event_cb_t callback, void *user_data);

/** Queues scan request. Fails if connected. Returns true if queued. */
bool wifi_manager_request_scan(const ipc_wifi_scan_request_t *request);

/** Queues connect request. Returns true if queued. */
bool wifi_manager_request_connect(const ipc_wifi_connect_request_t *request);

/** Queues disconnect request. Returns true if queued. */
bool wifi_manager_request_disconnect(void);

/** Queues status request; callback receives current status. Returns true if queued. */
bool wifi_manager_request_status(void);

/** Queues request to clear saved Wi-Fi profile from NVM. Returns true if queued. */
bool wifi_manager_request_clear_profile(void);

#endif
