#ifndef SENSORHUB_V1_STATE_H
#define SENSORHUB_V1_STATE_H

#include "sensorhub_v1_types.h"

#include <stdbool.h>
#include <stdint.h>

void sensorhub_v1_state_init(void);
void sensorhub_v1_state_poll(sensorhub_v1_snapshot_t *snapshot);

void sensorhub_v1_request_start(uint8_t sensors_mask, uint16_t period_ms);
void sensorhub_v1_request_stop(void);
void sensorhub_v1_request_status(void);

bool sensorhub_v1_is_running(void);

#endif /* SENSORHUB_V1_STATE_H */
