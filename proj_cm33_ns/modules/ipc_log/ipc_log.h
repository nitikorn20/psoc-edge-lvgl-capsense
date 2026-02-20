/*******************************************************************************
 * File Name        : ipc_log.h
 *
 * Description      : IPC logging API and message types for CM33.
 *
 * Author           : Asst.Prof.Santi Nuratch, Ph.D
 *                    Thailand Embedded Systems Association (TESA)
 *
 *******************************************************************************/

#ifndef IPC_LOG_H
#define IPC_LOG_H

/*
 * Toggle this define to disable the IPC logging module.
 * When disabled:
 * - No logging task is created.
 * - No IPC messages are sent.
 * - log_queue_printf calls become empty stubs.
 */

// #define DISABLE_IPC_LOGGING

#ifndef DISABLE_IPC_LOGGING

#include "FreeRTOS.h"
#include "queue.h"
#include <stdbool.h>
#include <stdio.h>

#define LOG_QUEUE_LENGTH (16U)   /* Max queued log messages before blocking. */
#define LOG_MESSAGE_SIZE (128U)  /* Max characters per message. */

typedef struct
{
  char message[LOG_MESSAGE_SIZE];  /* Null-terminated log text. */
} log_msg_t;

extern QueueHandle_t log_queue;

/**
 * Initializes the log queue. Safe to call multiple times. Returns true on success.
 */
bool ipc_log_init(void);

/**
 * Queues a printf-style message for transport to CM55. Blocks if queue full.
 */
void ipc_log_printf(const char *format, ...);

/* Redirect standard printf to IPC log for CM33_NS */
#define printf(...) ipc_log_printf(__VA_ARGS__)

#else

#include <stdbool.h>

#define ipc_log_init() (true)

/* Stub for ipc_log_printf that does nothing and consumes arguments */
static inline void ipc_log_printf(const char *format, ...) { (void)format; }

#endif

#endif /* LOG_QUEUE_H */
