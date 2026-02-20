#include "wifi_manager.h"

#include "wifi_connect.h"
#include "wifi_profile_nvm.h"
#include "wifi_scanner.h"

#include "FreeRTOS.h"
#include "cy_wcm.h"
#include "task.h"
#include <stdio.h>
#include <string.h>

#define WIFI_MANAGER_TASK_STACK (2048U)      /* Task stack size. */
#define WIFI_MANAGER_TASK_PRIO (2U)
#define WIFI_MANAGER_QUEUE_LEN (16U)         /* Command queue length. */
#define WIFI_MANAGER_STATUS_POLL_MS (1000U)  /* Poll interval when idle. */
#define WIFI_MANAGER_RECONNECT_MS (5000U)    /* Auto-reconnect interval after unexpected link loss. */
#define WIFI_MANAGER_RSSI_FAIL_MAX (2U)      /* Consecutive AP info read failures before forcing disconnect state. */
#define WIFI_MANAGER_LINK_PROBE_FAIL_MAX (8U)
#define WIFI_MANAGER_LINK_PROBE_TIMEOUT_MS (800U)

typedef enum
{
  WIFI_MANAGER_CMD_SCAN = 0U,
  WIFI_MANAGER_CMD_CONNECT = 1U,
  WIFI_MANAGER_CMD_DISCONNECT = 2U,
  WIFI_MANAGER_CMD_STATUS = 3U,
  WIFI_MANAGER_CMD_CONNECTED = 4U,
  WIFI_MANAGER_CMD_DISCONNECTED = 5U,
  WIFI_MANAGER_CMD_CLEAR_PROFILE = 6U
} wifi_manager_cmd_t;

typedef struct
{
  wifi_manager_cmd_t cmd;
  ipc_wifi_scan_request_t scan_request;
  ipc_wifi_connect_request_t connect_request;
} wifi_manager_msg_t;

typedef struct
{
  QueueHandle_t queue;               /* Command queue. */
  TaskHandle_t task;                 /* Manager task. */
  wifi_connect_t *wifi_connect;      /* Lazy-initialized connect handle. */
  wifi_connect_config_t connect_config;
  wifi_connect_callbacks_t connect_callbacks;
  wifi_manager_event_cb_t callback;  /* Event callback. */
  void *callback_user_data;
  ipc_wifi_status_t status;          /* Current status. */
  bool started;                      /* True if start() succeeded. */
  ipc_wifi_connect_request_t last_connect_request;
  bool has_last_connect_request;
  uint8_t rssi_fail_count;
  uint8_t link_probe_fail_count;
} wifi_manager_ctx_t;

static wifi_manager_ctx_t s_ctx;

static void wifi_manager_emit(wifi_manager_event_t event, const void *data, uint32_t count)
{
  if (NULL != s_ctx.callback)
  {
    s_ctx.callback(event, data, count, s_ctx.callback_user_data);
  }
}

static void wifi_manager_emit_status(ipc_wifi_reason_t reason)
{
  s_ctx.status.reason = (uint16_t)reason;
  wifi_manager_emit(WIFI_MANAGER_EVENT_STATUS, &s_ctx.status, 1U);
}

static void wifi_manager_mark_disconnected(ipc_wifi_reason_t reason, const char *why)
{
  s_ctx.status.state = (uint8_t)IPC_WIFI_LINK_DISCONNECTED;
  s_ctx.status.rssi = -127;
  s_ctx.rssi_fail_count = 0U;
  s_ctx.link_probe_fail_count = 0U;
  if (NULL != why)
  {
    (void)printf("[CM33] Link lost (%s)\n", why);
  }
  wifi_manager_emit_status(reason);
}

static bool wifi_manager_sync_connecting_state(void)
{
  if ((NULL == s_ctx.wifi_connect) || !wifi_connect_is_connecting(s_ctx.wifi_connect))
  {
    return false;
  }

  if ((uint8_t)IPC_WIFI_LINK_CONNECTING != s_ctx.status.state)
  {
    s_ctx.status.state = (uint8_t)IPC_WIFI_LINK_CONNECTING;
    s_ctx.status.rssi = -127;
    wifi_manager_emit_status(IPC_WIFI_REASON_NONE);
    return true;
  }
  return false;
}

static void wifi_manager_update_rssi(void)
{
  cy_rslt_t rslt;
  cy_wcm_ip_address_t gateway_addr;
  uint32_t elapsed_ms = 0U;

  if (IPC_WIFI_LINK_CONNECTED != (ipc_wifi_link_state_t)s_ctx.status.state)
  {
    s_ctx.rssi_fail_count = 0U;
    s_ctx.link_probe_fail_count = 0U;
    return;
  }

  if (0U == cy_wcm_is_connected_to_ap())
  {
    wifi_manager_mark_disconnected(IPC_WIFI_REASON_DISCONNECTED, "cy_wcm_is_connected_to_ap=0");
    return;
  }

  cy_wcm_associated_ap_info_t ap_info;
  (void)memset(&ap_info, 0, sizeof(ap_info));
  rslt = cy_wcm_get_associated_ap_info(&ap_info);
  if (CY_RSLT_SUCCESS == rslt)
  {
    s_ctx.rssi_fail_count = 0U;
    s_ctx.link_probe_fail_count = 0U;
    s_ctx.status.rssi = ap_info.signal_strength;
    wifi_manager_emit_status(IPC_WIFI_REASON_NONE);
    return;
  }

  if (s_ctx.rssi_fail_count < 0xFFU)
  {
    s_ctx.rssi_fail_count++;
  }

  /* Probe path is used only when AP info read fails, to avoid false disconnects during normal link state. */
  (void)memset(&gateway_addr, 0, sizeof(gateway_addr));
  rslt = cy_wcm_get_gateway_ip_address(CY_WCM_INTERFACE_TYPE_STA, &gateway_addr);
  if (CY_RSLT_SUCCESS == rslt)
  {
    rslt = cy_wcm_ping(CY_WCM_INTERFACE_TYPE_STA, &gateway_addr, WIFI_MANAGER_LINK_PROBE_TIMEOUT_MS, &elapsed_ms);
  }

  if (CY_RSLT_SUCCESS == rslt)
  {
    s_ctx.link_probe_fail_count = 0U;
    wifi_manager_emit_status(IPC_WIFI_REASON_NONE);
    return;
  }

  if (s_ctx.link_probe_fail_count < 0xFFU)
  {
    s_ctx.link_probe_fail_count++;
  }

  if ((s_ctx.rssi_fail_count >= WIFI_MANAGER_RSSI_FAIL_MAX) &&
      (s_ctx.link_probe_fail_count >= WIFI_MANAGER_LINK_PROBE_FAIL_MAX))
  {
    wifi_manager_mark_disconnected(IPC_WIFI_REASON_DISCONNECTED, "probe failure");
    return;
  }

  wifi_manager_emit_status(IPC_WIFI_REASON_NONE);
}

static void wifi_manager_scan_complete_cb(void *user_data, const wifi_info_t *results, uint32_t count)
{
  (void)user_data;

  if ((NULL != results) && (count > 0U))
  {
    wifi_manager_emit(WIFI_MANAGER_EVENT_SCAN_RESULT, results, count);
  }

  ipc_wifi_scan_complete_t scan_complete;
  scan_complete.total_count = (uint16_t)count;
  scan_complete.status = 0U;
  wifi_manager_emit(WIFI_MANAGER_EVENT_SCAN_COMPLETE, &scan_complete, 1U);

  s_ctx.status.state = (uint8_t)IPC_WIFI_LINK_DISCONNECTED;
  s_ctx.status.rssi = -127;
  wifi_manager_emit_status(IPC_WIFI_REASON_NONE);
}

static void wifi_manager_on_connected(wifi_connect_t *wifi, const cy_wcm_ip_address_t *ip_addr, void *user_ctx)
{
  (void)wifi;
  (void)ip_addr;
  (void)user_ctx;

  if ((NULL != s_ctx.queue) && (NULL != s_ctx.task))
  {
    wifi_manager_msg_t msg;
    (void)memset(&msg, 0, sizeof(msg));
    msg.cmd = WIFI_MANAGER_CMD_CONNECTED;
    (void)xQueueSend(s_ctx.queue, &msg, 0U);
  }
}

static void wifi_manager_on_disconnected(wifi_connect_t *wifi, void *user_ctx)
{
  (void)wifi;
  (void)user_ctx;

  if ((NULL != s_ctx.queue) && (NULL != s_ctx.task))
  {
    wifi_manager_msg_t msg;
    (void)memset(&msg, 0, sizeof(msg));
    msg.cmd = WIFI_MANAGER_CMD_DISCONNECTED;
    (void)xQueueSend(s_ctx.queue, &msg, 0U);
  }
}

static bool wifi_manager_ensure_connect_handle(void)
{
  if (NULL != s_ctx.wifi_connect)
  {
    return true;
  }

  s_ctx.connect_config.reconnect_interval_ms = WIFI_MANAGER_RECONNECT_MS;
  s_ctx.connect_callbacks.on_connected = wifi_manager_on_connected;
  s_ctx.connect_callbacks.on_disconnected = wifi_manager_on_disconnected;
  s_ctx.connect_callbacks.user_ctx = NULL;

  if (CY_RSLT_SUCCESS != wifi_connect_init(&s_ctx.wifi_connect, &s_ctx.connect_config, &s_ctx.connect_callbacks))
  {
    s_ctx.status.state = (uint8_t)IPC_WIFI_LINK_ERROR;
    s_ctx.status.rssi = -127;
    wifi_manager_emit_status(IPC_WIFI_REASON_CONNECT_FAILED);
    return false;
  }

  return true;
}

static void wifi_manager_handle_scan(const ipc_wifi_scan_request_t *request)
{
  if (IPC_WIFI_LINK_DISCONNECTED != (ipc_wifi_link_state_t)s_ctx.status.state)
  {
    wifi_manager_emit_status(IPC_WIFI_REASON_SCAN_BLOCKED_CONNECTED);
    return;
  }

  if (NULL == request)
  {
    return;
  }

  s_ctx.status.state = (uint8_t)IPC_WIFI_LINK_SCANNING;
  s_ctx.status.rssi = -127;
  wifi_manager_emit_status(IPC_WIFI_REASON_NONE);

  if (false == wifi_scanner_scan(request->use_filter ? (wifi_filter_config_t *)&request->filter : NULL))
  {
    s_ctx.status.state = (uint8_t)IPC_WIFI_LINK_ERROR;
    s_ctx.status.rssi = -127;
    wifi_manager_emit_status(IPC_WIFI_REASON_SCAN_FAILED);
  }
}

static void wifi_manager_handle_connect(const ipc_wifi_connect_request_t *request)
{
  if (NULL == request)
  {
    return;
  }

  if (false == wifi_manager_ensure_connect_handle())
  {
    return;
  }

  wifi_connect_params_t params;
  (void)memset(&params, 0, sizeof(params));
  (void)strncpy(params.ssid, request->ssid, sizeof(params.ssid) - 1U);
  (void)strncpy(params.password, request->password, sizeof(params.password) - 1U);
  if ((0U == request->security) && ('\0' == request->password[0]))
  {
    params.security = CY_WCM_SECURITY_OPEN;
  }
  else if (0U == request->security)
  {
    params.security = CY_WCM_SECURITY_WPA2_AES_PSK;
  }
  else
  {
    params.security = (cy_wcm_security_t)request->security;
  }

  s_ctx.status.state = (uint8_t)IPC_WIFI_LINK_CONNECTING;
  s_ctx.status.rssi = -127;
  (void)strncpy(s_ctx.status.ssid, request->ssid, sizeof(s_ctx.status.ssid) - 1U);
  s_ctx.status.ssid[sizeof(s_ctx.status.ssid) - 1U] = '\0';
  wifi_manager_emit_status(IPC_WIFI_REASON_NONE);

  if (CY_RSLT_SUCCESS != wifi_connect_start(s_ctx.wifi_connect, &params))
  {
    s_ctx.status.state = (uint8_t)IPC_WIFI_LINK_ERROR;
    s_ctx.status.rssi = -127;
    wifi_manager_emit_status(IPC_WIFI_REASON_CONNECT_FAILED);
  }
}

static void wifi_manager_handle_disconnect(void)
{
  if (NULL != s_ctx.wifi_connect)
  {
    (void)wifi_connect_stop(s_ctx.wifi_connect);
  }
  s_ctx.status.state = (uint8_t)IPC_WIFI_LINK_DISCONNECTED;
  s_ctx.status.rssi = -127;
  s_ctx.rssi_fail_count = 0U;
  s_ctx.link_probe_fail_count = 0U;
  wifi_manager_emit_status(IPC_WIFI_REASON_DISCONNECTED);
}

static void wifi_manager_handle_clear_profile(void)
{
  if (wifi_profile_nvm_clear())
  {
    (void)printf("[CM33] WiFi profile cleared from NVM\n");
  }
  else
  {
    (void)printf("[CM33] WiFi profile clear failed\n");
  }
}

static void wifi_manager_task(void *arg)
{
  (void)arg;
  wifi_manager_msg_t msg;

  while (true)
  {
    if (xQueueReceive(s_ctx.queue, &msg, pdMS_TO_TICKS(WIFI_MANAGER_STATUS_POLL_MS)) == pdPASS)
    {
      switch (msg.cmd)
      {
      case WIFI_MANAGER_CMD_SCAN:
        wifi_manager_handle_scan(&msg.scan_request);
        break;
      case WIFI_MANAGER_CMD_CONNECT:
        wifi_manager_handle_connect(&msg.connect_request);
        break;
      case WIFI_MANAGER_CMD_DISCONNECT:
        wifi_manager_handle_disconnect();
        break;
      case WIFI_MANAGER_CMD_STATUS:
        if (!wifi_manager_sync_connecting_state())
        {
          wifi_manager_emit_status(IPC_WIFI_REASON_NONE);
        }
        break;
      case WIFI_MANAGER_CMD_CONNECTED:
        s_ctx.status.state = (uint8_t)IPC_WIFI_LINK_CONNECTED;
        s_ctx.rssi_fail_count = 0U;
        s_ctx.link_probe_fail_count = 0U;
        wifi_manager_update_rssi();
        wifi_manager_emit_status(IPC_WIFI_REASON_NONE);
        (void)printf("[CM33] Connected event: has_last_connect_request=%u\n",
                     (unsigned int)(s_ctx.has_last_connect_request ? 1U : 0U));
        if (s_ctx.has_last_connect_request)
        {
          if (wifi_profile_nvm_save(&s_ctx.last_connect_request))
          {
            (void)printf("[CM33] WiFi profile saved to NVM: %s\n", s_ctx.last_connect_request.ssid);
          }
          else
          {
            (void)printf("[CM33] WiFi profile save failed\n");
          }
        }
        break;
      case WIFI_MANAGER_CMD_DISCONNECTED:
      {
        ipc_wifi_reason_t reason = IPC_WIFI_REASON_DISCONNECTED;
        if ((uint8_t)IPC_WIFI_LINK_CONNECTING == s_ctx.status.state)
        {
          reason = IPC_WIFI_REASON_CONNECT_FAILED;
        }
        s_ctx.status.state = (uint8_t)IPC_WIFI_LINK_DISCONNECTED;
        s_ctx.status.rssi = -127;
        s_ctx.rssi_fail_count = 0U;
        s_ctx.link_probe_fail_count = 0U;
        wifi_manager_emit_status(reason);
      }
        break;
      case WIFI_MANAGER_CMD_CLEAR_PROFILE:
        wifi_manager_handle_clear_profile();
        break;
      default:
        break;
      }
    }
    else
    {
      wifi_manager_update_rssi();
      wifi_manager_sync_connecting_state();
    }
  }
}

bool wifi_manager_init(void)
{
  (void)memset(&s_ctx, 0, sizeof(s_ctx));
  s_ctx.status.state = (uint8_t)IPC_WIFI_LINK_DISCONNECTED;
  s_ctx.status.rssi = -127;
  s_ctx.status.reason = (uint16_t)IPC_WIFI_REASON_NONE;
  s_ctx.rssi_fail_count = 0U;
  s_ctx.link_probe_fail_count = 0U;
  return true;
}

bool wifi_manager_start(void)
{
  wifi_scanner_config_t scanner_config;
  ipc_wifi_connect_request_t saved_profile;

  if (s_ctx.started)
  {
    return true;
  }

  s_ctx.queue = xQueueCreate(WIFI_MANAGER_QUEUE_LEN, sizeof(wifi_manager_msg_t));
  if (NULL == s_ctx.queue)
  {
    return false;
  }

  (void)memset(&scanner_config, 0, sizeof(scanner_config));
  scanner_config.task_priority = 1U;
  scanner_config.task_stack_size = (1024U * 4U);
  scanner_config.filter_mode = WIFI_FILTER_MODE_NONE;

  if (false == wifi_scanner_setup(&scanner_config, true))
  {
    return false;
  }

  if (false == wifi_scanner_on_scan_complete(wifi_manager_scan_complete_cb, NULL))
  {
    return false;
  }

  if (xTaskCreate(wifi_manager_task, "wifi_manager", WIFI_MANAGER_TASK_STACK, NULL, WIFI_MANAGER_TASK_PRIO,
                  &s_ctx.task) != pdPASS)
  {
    return false;
  }

  s_ctx.started = true;

  if (wifi_profile_nvm_load(&saved_profile))
  {
    (void)printf("[CM33] Auto-connect from saved profile: %s\n", saved_profile.ssid);
    (void)wifi_manager_request_connect(&saved_profile);
  }
  else
  {
    (void)printf("[CM33] No valid saved WiFi profile in NVM\n");
  }

  return true;
}

bool wifi_manager_set_event_callback(wifi_manager_event_cb_t callback, void *user_data)
{
  s_ctx.callback = callback;
  s_ctx.callback_user_data = user_data;
  return true;
}

bool wifi_manager_request_scan(const ipc_wifi_scan_request_t *request)
{
  wifi_manager_msg_t msg;

  if ((NULL == s_ctx.queue) || (NULL == request))
  {
    return false;
  }

  (void)memset(&msg, 0, sizeof(msg));
  msg.cmd = WIFI_MANAGER_CMD_SCAN;
  msg.scan_request = *request;
  return (pdPASS == xQueueSend(s_ctx.queue, &msg, 0U));
}

bool wifi_manager_request_connect(const ipc_wifi_connect_request_t *request)
{
  wifi_manager_msg_t msg;

  if ((NULL == s_ctx.queue) || (NULL == request))
  {
    return false;
  }

  (void)memset(&msg, 0, sizeof(msg));
  msg.cmd = WIFI_MANAGER_CMD_CONNECT;
  msg.connect_request = *request;
  s_ctx.last_connect_request = *request;
  s_ctx.last_connect_request.ssid[sizeof(s_ctx.last_connect_request.ssid) - 1U] = '\0';
  s_ctx.last_connect_request.password[sizeof(s_ctx.last_connect_request.password) - 1U] = '\0';
  s_ctx.has_last_connect_request = true;
  (void)printf("[CM33] Queued connect request: ssid=%s\n", s_ctx.last_connect_request.ssid);
  return (pdPASS == xQueueSend(s_ctx.queue, &msg, 0U));
}

bool wifi_manager_request_disconnect(void)
{
  wifi_manager_msg_t msg;

  if (NULL == s_ctx.queue)
  {
    return false;
  }

  (void)memset(&msg, 0, sizeof(msg));
  msg.cmd = WIFI_MANAGER_CMD_DISCONNECT;
  return (pdPASS == xQueueSend(s_ctx.queue, &msg, 0U));
}

bool wifi_manager_request_status(void)
{
  wifi_manager_msg_t msg;

  if (NULL == s_ctx.queue)
  {
    return false;
  }

  (void)memset(&msg, 0, sizeof(msg));
  msg.cmd = WIFI_MANAGER_CMD_STATUS;
  return (pdPASS == xQueueSend(s_ctx.queue, &msg, 0U));
}

bool wifi_manager_request_clear_profile(void)
{
  wifi_manager_msg_t msg;

  if (NULL == s_ctx.queue)
  {
    return false;
  }

  (void)memset(&msg, 0, sizeof(msg));
  msg.cmd = WIFI_MANAGER_CMD_CLEAR_PROFILE;
  s_ctx.has_last_connect_request = false;
  return (pdPASS == xQueueSend(s_ctx.queue, &msg, 0U));
}
