#include "app_events.h"
#include "cm33_ipc_pipe.h"
#include "cm33_system.h"
#include "cy_pdl.h"
#include "cy_time.h"
#include "cyabs_rtos.h"
#include "cyabs_rtos_impl.h"
#include "cybsp.h"
#include "error_handler.h"
#include "examples.h"
#include "gyro_task.h"
#include "ipc_communication.h"
#include "ipc_log.h"
#include "ipc_log_transport.h"
#include "retarget_io_init.h"
#include "sensorhub_manager.h"
#include "user_buttons.h"
#include <FreeRTOS.h>
#include <stdio.h>
#include <task.h>

int main()
{
  if (!cm33_system_init())
  {
    handle_error(NULL);
  }

  if (!ipc_log_transport_init())
  {
    handle_error(NULL);
  }

  printf("***************************************\n"
         "CM33: SensorHub + IPC PIPE Data Stream\n"
         "***************************************\n");

  if (!sensorhub_manager_init())
  {
    handle_error(NULL);
  }

  if (!sensorhub_manager_start())
  {
    handle_error(NULL);
  }

  Cy_GPIO_ClearInterrupt(CYBSP_USER_BTN1_PORT, CYBSP_USER_BTN1_PIN);
#ifdef CYBSP_USER_BTN2_ENABLED
  Cy_GPIO_ClearInterrupt(CYBSP_USER_BTN2_PORT, CYBSP_USER_BTN2_PIN);
#endif
  NVIC_ClearPendingIRQ(CYBSP_USER_BTN_IRQ);

  if (!user_buttons_init())
  {
    handle_error(NULL);
  }

  if (!cm33_ipc_pipe_start())
  {
    handle_error(NULL);
  }

  cm33_system_enable_cm55();

  vTaskStartScheduler();
  handle_error(NULL);
}
