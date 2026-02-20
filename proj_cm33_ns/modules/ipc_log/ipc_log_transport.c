/*******************************************************************************
 * File Name        : ipc_log_transport.c
 *
 * Description      : IPC log transport task; forwards log queue to CM55.
 *
 * Author           : Asst.Prof.Santi Nuratch, Ph.D
 *                    Thailand Embedded Systems Association (TESA)
 *
 *******************************************************************************/

#include "ipc_log_transport.h"

#ifndef DISABLE_IPC_LOGGING

#include "ipc_communication.h"
#include "ipc_log.h"
#include "task.h"
#include <stdint.h>
#include <string.h>

#define LOG_TASK_STACK_SIZE (2048U)   /* Stack size for transport task. */
#define LOG_TASK_PRIORITY (configMAX_PRIORITIES - 1)  /* Highest priority. */

CY_SECTION_SHAREDMEM static ipc_msg_t cm33_log_msg;

static void log_ipc_dispatch_worker(void *pvParameters)
{
  (void)pvParameters;
  log_msg_t msg;
  cy_en_ipc_pipe_status_t status;

  while (true)
  {
    if (pdPASS == xQueueReceive(log_queue, &msg, portMAX_DELAY))
    {
      cm33_log_msg.client_id = CM55_IPC_PIPE_CLIENT_ID;
      cm33_log_msg.intr_mask = CY_IPC_CYPIPE_INTR_MASK_EP1;
      cm33_log_msg.cmd = (uint8_t)IPC_CMD_LOG;
      cm33_log_msg.value = 0U;
      strncpy(cm33_log_msg.data, msg.message, IPC_DATA_MAX_LEN - 1UL);
      cm33_log_msg.data[IPC_DATA_MAX_LEN - 1UL] = '\0';

      do
      {
        status = Cy_IPC_Pipe_SendMessage(CM55_IPC_PIPE_EP_ADDR,
                                         CM33_IPC_PIPE_EP_ADDR,
                                         (void *)&cm33_log_msg, 0);
        if (CY_IPC_PIPE_ERROR_SEND_BUSY == status)
        {
          vTaskDelay(pdMS_TO_TICKS(1));
        }
      }
      while (CY_IPC_PIPE_ERROR_SEND_BUSY == status);
    }
  }
}

/**
 * Initializes IPC log queue and spawns the transport task. Returns true on success.
 */
bool ipc_log_transport_init(void)
{
  if (!ipc_log_init())
  {
    return false;
  }
  if (pdPASS != xTaskCreate(log_ipc_dispatch_worker, "Log IPC Worker",
                            LOG_TASK_STACK_SIZE, NULL, LOG_TASK_PRIORITY,
                            NULL))
  {
    return false;
  }
  return true;
}

#endif
