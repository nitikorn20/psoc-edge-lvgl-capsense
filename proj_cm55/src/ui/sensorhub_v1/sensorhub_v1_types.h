#ifndef SENSORHUB_V1_TYPES_H
#define SENSORHUB_V1_TYPES_H

#include "ipc_communication.h"

#include <stdbool.h>

#define SENSORHUB_V1_DEBUG_TEXT_MAX (4096U)

typedef struct
{
  ipc_sensorhub_status_t status;
  ipc_sensorhub_sample_t pot_sample;
  ipc_sensorhub_sample_t bmi270_sample;
  ipc_sensorhub_sample_t capsense_sample;
  ipc_sensorhub_sample_t touch_sample;
  ipc_sensorhub_sample_t bmm350_sample;
  bool has_pot;
  bool has_bmi270;
  bool has_capsense;
  bool has_touch;
  bool has_bmm350;
  bool status_changed;
  bool pot_changed;
  bool bmi270_changed;
  bool capsense_changed;
  bool touch_changed;
  bool bmm350_changed;
  bool debug_changed;
  char debug_text[SENSORHUB_V1_DEBUG_TEXT_MAX];
} sensorhub_v1_snapshot_t;

#endif /* SENSORHUB_V1_TYPES_H */
