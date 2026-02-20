#ifndef SENSORHUB_MANAGER_H
#define SENSORHUB_MANAGER_H

#include "ipc_communication.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
  SENSORHUB_MANAGER_EVENT_SAMPLE = 0U,
  SENSORHUB_MANAGER_EVENT_STATUS = 1U
} sensorhub_manager_event_t;

typedef void (*sensorhub_manager_event_cb_t)(sensorhub_manager_event_t event, const void *data, uint32_t count,
                                             void *user_data);

bool sensorhub_manager_init(void);
bool sensorhub_manager_start(void);
bool sensorhub_manager_set_event_callback(sensorhub_manager_event_cb_t callback, void *user_data);

bool sensorhub_manager_request_start(const ipc_sensorhub_start_request_t *request);
bool sensorhub_manager_request_stop(void);
bool sensorhub_manager_request_status(void);

#endif /* SENSORHUB_MANAGER_H */
