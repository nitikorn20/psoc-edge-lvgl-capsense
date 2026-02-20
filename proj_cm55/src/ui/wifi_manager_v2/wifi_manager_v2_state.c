#include "wifi_manager_v2_state.h"

#include "cm55_ipc_app.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct
{
  ipc_wifi_status_t status;
  wifi_info_t live_list[WIFI_MANAGER_V2_AP_MAX];
  uint32_t live_count;
  wifi_info_t cache_list[WIFI_MANAGER_V2_AP_MAX];
  uint32_t cache_count;
  char debug_text[WIFI_MANAGER_V2_DEBUG_TEXT_MAX];
  uint32_t debug_sequence;
} wifi_manager_v2_store_t;

static wifi_manager_v2_store_t s_store;

static void copy_wifi_list(wifi_info_t *dst, const wifi_info_t *src, uint32_t count)
{
  if ((NULL == dst) || (NULL == src))
  {
    return;
  }

  if (count > WIFI_MANAGER_V2_AP_MAX)
  {
    count = WIFI_MANAGER_V2_AP_MAX;
  }

  if (count > 0U)
  {
    (void)memcpy(dst, src, sizeof(wifi_info_t) * count);
  }
}

static uint32_t security_from_text(const char *security_text)
{
  /* CM33 currently treats security=0 as default WPA2.
   * For OPEN AP we send security=0 with empty password and CM33 maps it to OPEN.
   */
  (void)security_text;
  return 0U;
}

void wifi_manager_v2_state_init(void)
{
  (void)memset(&s_store, 0, sizeof(s_store));
  s_store.status.state = (uint8_t)IPC_WIFI_LINK_DISCONNECTED;
  s_store.status.reason = (uint16_t)IPC_WIFI_REASON_NONE;
  s_store.status.rssi = -127;
}

void wifi_manager_v2_state_poll(wifi_manager_v2_snapshot_t *snapshot)
{
  ipc_wifi_status_t latest_status;
  wifi_info_t latest_list[WIFI_MANAGER_V2_AP_MAX];
  uint32_t latest_count;
  uint32_t debug_seq;

  if (NULL == snapshot)
  {
    return;
  }

  (void)memset(snapshot, 0, sizeof(*snapshot));

  if (cm55_get_wifi_status(&latest_status))
  {
    if (0 != memcmp(&latest_status, &s_store.status, sizeof(latest_status)))
    {
      s_store.status = latest_status;
      snapshot->status_changed = true;
    }
  }

  latest_count = 0U;
  if (cm55_get_wifi_list(latest_list, WIFI_MANAGER_V2_AP_MAX, &latest_count))
  {
    if (latest_count > WIFI_MANAGER_V2_AP_MAX)
    {
      latest_count = WIFI_MANAGER_V2_AP_MAX;
    }

    (void)memset(s_store.live_list, 0, sizeof(s_store.live_list));
    (void)memset(s_store.cache_list, 0, sizeof(s_store.cache_list));
    copy_wifi_list(s_store.live_list, latest_list, latest_count);
    copy_wifi_list(s_store.cache_list, latest_list, latest_count);
    s_store.live_count = latest_count;
    s_store.cache_count = latest_count;
    snapshot->scan_changed = true;
  }

  debug_seq = cm55_get_wifi_debug_sequence();
  if (debug_seq != s_store.debug_sequence)
  {
    (void)cm55_get_wifi_debug_text(s_store.debug_text, sizeof(s_store.debug_text));
    s_store.debug_sequence = debug_seq;
    snapshot->debug_changed = true;
  }

  snapshot->status = s_store.status;
  snapshot->live_count = s_store.live_count;
  snapshot->cache_count = s_store.cache_count;
  copy_wifi_list(snapshot->live_list, s_store.live_list, s_store.live_count);
  copy_wifi_list(snapshot->cache_list, s_store.cache_list, s_store.cache_count);
  (void)memcpy(snapshot->debug_text, s_store.debug_text, sizeof(snapshot->debug_text));
}

void wifi_manager_v2_request_scan(void)
{
  cm55_trigger_scan_all();
  cm55_trigger_status_request();
}

void wifi_manager_v2_request_disconnect(void)
{
  cm55_trigger_disconnect();
  cm55_trigger_status_request();
}

void wifi_manager_v2_request_status(void)
{
  cm55_trigger_status_request();
}

void wifi_manager_v2_request_clear_profile(void)
{
  cm55_trigger_profile_clear();
  cm55_trigger_status_request();
}

void wifi_manager_v2_request_connect(const char *ssid, const char *password, const char *security_text)
{
  uint32_t security = security_from_text(security_text);
  cm55_trigger_connect(ssid, password, security);
  cm55_trigger_status_request();
}

bool wifi_manager_v2_is_connected(void)
{
  return ((uint8_t)IPC_WIFI_LINK_CONNECTED == s_store.status.state);
}

bool wifi_manager_v2_is_scanning(void)
{
  return ((uint8_t)IPC_WIFI_LINK_SCANNING == s_store.status.state);
}

bool wifi_manager_v2_is_connecting(void)
{
  return ((uint8_t)IPC_WIFI_LINK_CONNECTING == s_store.status.state);
}
