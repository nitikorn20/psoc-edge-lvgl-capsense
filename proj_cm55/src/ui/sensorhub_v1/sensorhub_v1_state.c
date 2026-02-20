#include "sensorhub_v1_state.h"

#include "cm55_ipc_app.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

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
  char debug_text[SENSORHUB_V1_DEBUG_TEXT_MAX];
  uint32_t debug_sequence;
} sensorhub_v1_store_t;

static sensorhub_v1_store_t s_store;

void sensorhub_v1_state_init(void)
{
  (void)memset(&s_store, 0, sizeof(s_store));
  s_store.status.state = (uint8_t)IPC_SENSORHUB_STATE_STOPPED;
  s_store.status.reason = (uint16_t)IPC_SENSORHUB_REASON_NONE;
}

void sensorhub_v1_state_poll(sensorhub_v1_snapshot_t *snapshot)
{
  ipc_sensorhub_status_t latest_status;
  ipc_sensorhub_sample_t latest_sample;
  uint32_t debug_sequence;

  if (NULL == snapshot)
  {
    return;
  }

  (void)memset(snapshot, 0, sizeof(*snapshot));

  if (cm55_get_sensorhub_status(&latest_status))
  {
    if (0 != memcmp(&latest_status, &s_store.status, sizeof(latest_status)))
    {
      s_store.status = latest_status;
      snapshot->status_changed = true;
    }
  }

  if (cm55_get_sensorhub_latest_pot(&latest_sample))
  {
    if ((!s_store.has_pot) || (latest_sample.sequence != s_store.pot_sample.sequence))
    {
      s_store.pot_sample = latest_sample;
      s_store.has_pot = true;
      snapshot->pot_changed = true;
    }
  }

  if (cm55_get_sensorhub_latest_bmi270(&latest_sample))
  {
    if ((!s_store.has_bmi270) || (latest_sample.sequence != s_store.bmi270_sample.sequence))
    {
      s_store.bmi270_sample = latest_sample;
      s_store.has_bmi270 = true;
      snapshot->bmi270_changed = true;
    }
  }

  if (cm55_get_sensorhub_latest_capsense(&latest_sample))
  {
    if ((!s_store.has_capsense) || (latest_sample.sequence != s_store.capsense_sample.sequence))
    {
      s_store.capsense_sample = latest_sample;
      s_store.has_capsense = true;
      snapshot->capsense_changed = true;
    }
  }

  if (cm55_get_sensorhub_latest_touch(&latest_sample))
  {
    if ((!s_store.has_touch) || (latest_sample.sequence != s_store.touch_sample.sequence))
    {
      s_store.touch_sample = latest_sample;
      s_store.has_touch = true;
      snapshot->touch_changed = true;
    }
  }

  if (cm55_get_sensorhub_latest_bmm350(&latest_sample))
  {
    if ((!s_store.has_bmm350) || (latest_sample.sequence != s_store.bmm350_sample.sequence))
    {
      s_store.bmm350_sample = latest_sample;
      s_store.has_bmm350 = true;
      snapshot->bmm350_changed = true;
    }
  }

  debug_sequence = cm55_get_sensorhub_debug_sequence();
  if (debug_sequence != s_store.debug_sequence)
  {
    (void)cm55_get_sensorhub_debug_text(s_store.debug_text, sizeof(s_store.debug_text));
    s_store.debug_sequence = debug_sequence;
    snapshot->debug_changed = true;
  }

  snapshot->status = s_store.status;
  snapshot->pot_sample = s_store.pot_sample;
  snapshot->bmi270_sample = s_store.bmi270_sample;
  snapshot->capsense_sample = s_store.capsense_sample;
  snapshot->touch_sample = s_store.touch_sample;
  snapshot->bmm350_sample = s_store.bmm350_sample;
  snapshot->has_pot = s_store.has_pot;
  snapshot->has_bmi270 = s_store.has_bmi270;
  snapshot->has_capsense = s_store.has_capsense;
  snapshot->has_touch = s_store.has_touch;
  snapshot->has_bmm350 = s_store.has_bmm350;
  (void)memcpy(snapshot->debug_text, s_store.debug_text, sizeof(snapshot->debug_text));
}

void sensorhub_v1_request_start(uint8_t sensors_mask, uint16_t period_ms)
{
  cm55_trigger_sensorhub_start(sensors_mask, period_ms);
  cm55_trigger_sensorhub_status_request();
}

void sensorhub_v1_request_stop(void)
{
  cm55_trigger_sensorhub_stop();
  cm55_trigger_sensorhub_status_request();
}

void sensorhub_v1_request_status(void)
{
  cm55_trigger_sensorhub_status_request();
}

bool sensorhub_v1_is_running(void)
{
  return ((uint8_t)IPC_SENSORHUB_STATE_RUNNING == s_store.status.state);
}
