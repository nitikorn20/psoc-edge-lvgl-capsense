/*******************************************************************************
 * File Name        : ipc_log_transport.h
 *
 * Description      : IPC log transport init and API.
 *
 * Author           : Asst.Prof.Santi Nuratch, Ph.D
 *                    Thailand Embedded Systems Association (TESA)
 *
 *******************************************************************************/

#ifndef IPC_LOG_TRANSPORT_H
#define IPC_LOG_TRANSPORT_H

#include "ipc_log.h"
#include <stdbool.h>

#ifndef DISABLE_IPC_LOGGING
/**
 * Initializes IPC log queue and transport task. Must be called before logging.
 */
bool ipc_log_transport_init(void);
#else
#define ipc_log_transport_init() (true)
#endif

#endif /* IPC_LOG_TRANSPORT_H */
