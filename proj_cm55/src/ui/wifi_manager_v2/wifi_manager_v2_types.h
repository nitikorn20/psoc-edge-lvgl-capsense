#ifndef WIFI_MANAGER_V2_TYPES_H
#define WIFI_MANAGER_V2_TYPES_H

#include "ipc_communication.h"

#include <stdbool.h>
#include <stdint.h>

#define WIFI_MANAGER_V2_AP_MAX (32U)
#define WIFI_MANAGER_V2_DEBUG_TEXT_MAX (1800U)

typedef struct
{
  ipc_wifi_status_t status;
  wifi_info_t live_list[WIFI_MANAGER_V2_AP_MAX];
  uint32_t live_count;
  wifi_info_t cache_list[WIFI_MANAGER_V2_AP_MAX];
  uint32_t cache_count;
  char debug_text[WIFI_MANAGER_V2_DEBUG_TEXT_MAX];
  bool status_changed;
  bool scan_changed;
  bool debug_changed;
} wifi_manager_v2_snapshot_t;

#endif /* WIFI_MANAGER_V2_TYPES_H */
