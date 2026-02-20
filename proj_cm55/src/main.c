#include "retarget_io_init.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "FreeRTOS.h"
#include "cy_time.h"
#include "cyabs_rtos.h"
#include "cyabs_rtos_impl.h"
#include "task.h"

#include "tesa/utils/tesa_datetime.h"
#include "tesa_datetime.h"
#include "tesa_display.h"
#include "tesa_rtos_stats.h"
#include "test_lvgl.h"
#include "ui/tabview/tabview.h"

#include "tesa/event_bus/examples.h"

#include "cm55_ipc_app.h"
#include "cm55_system.h"
#include "error_handler.h"
#include "ipc_communication.h"

#define STARTUP_WIFI_DELAY_MS (3000U)
#define STARTUP_WIFI_TASK_STACK (256U)
#define CM55_STARTUP_AUTO_CONNECT (0U)
#define CM55_STARTUP_SSID "TERNION"
#define CM55_STARTUP_PASS "111122134"

static void display_tick_cb(const system_tick_hook_params_t *params)
{
  (void)params;
  tesa_display_tick();
}

#if (CM55_STARTUP_AUTO_CONNECT != 0U)
static void startup_wifi_task(void *arg)
{
  (void)arg;
  vTaskDelay(pdMS_TO_TICKS(STARTUP_WIFI_DELAY_MS));
  cm55_trigger_connect(CM55_STARTUP_SSID, CM55_STARTUP_PASS, 0U);
  cm55_trigger_status_request();
  vTaskDelete(NULL);
}
#endif

int main(void)
{
  if (!cm55_system_init())
  {
    cm55_handle_error(NULL);
  }

  system_register_tick_hook(display_tick_cb, NULL);

  /* Setup IPC communication for CM55 */
  if (!cm55_ipc_app_init())
  {
    cm55_handle_error(NULL);
  }

  /* ANSI ESC sequence for clear screen */
  // printf("\x1b[2J\x1b[;H");

  /* Delay for 100ms */
  Cy_SysLib_Delay(100);

  printf("Initialize the display\r\n\n");

  /* Initialize the display */
  if (pdPASS != tesa_display_init())
  {
    cm55_handle_error(NULL);
  }

  printf("PSOC Edge MCU: LVGL + IPC\r\n\n");

#if (CM55_STARTUP_AUTO_CONNECT != 0U)
  if (xTaskCreate(startup_wifi_task, "StartupWiFi", STARTUP_WIFI_TASK_STACK, NULL,
                  tskIDLE_PRIORITY + 1U, NULL) != pdPASS)
  {
  }
#endif

  /* Start the scheduler */
  vTaskStartScheduler();
  cm55_handle_error(NULL);
}

/* [] END OF FILE */
