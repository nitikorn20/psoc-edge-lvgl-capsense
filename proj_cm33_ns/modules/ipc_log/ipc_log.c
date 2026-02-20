/*******************************************************************************
 * File Name        : ipc_log.c
 *
 * Description      : IPC logging queue and printf-style API for CM33.
 *
 * Author           : Asst.Prof.Santi Nuratch, Ph.D
 *                    Thailand Embedded Systems Association (TESA)
 *
 *******************************************************************************/

#include "ipc_log.h"

#ifndef DISABLE_IPC_LOGGING

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

QueueHandle_t log_queue = NULL;

bool ipc_log_init(void)
{
  if (NULL == log_queue)
  {
    log_queue = xQueueCreate(LOG_QUEUE_LENGTH, sizeof(log_msg_t));
  }
  return (NULL != log_queue);
}

void ipc_log_printf(const char *format, ...)
{
  if (NULL == log_queue)
  {
    return;
  }
  log_msg_t msg;
  va_list args;
  va_start(args, format);
  vsnprintf(msg.message, LOG_MESSAGE_SIZE, format, args);
  va_end(args);
  (void)xQueueSend(log_queue, &msg, portMAX_DELAY);
}

#endif
