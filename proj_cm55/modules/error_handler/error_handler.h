/*******************************************************************************
 * File Name        : error_handler.h
 *
 * Description      : Fatal error handler for CM55: disables IRQ, asserts,
 *                    prints message, blinks user LED in infinite loop.
 *
 * Author           : Asst.Prof.Santi Nuratch, Ph.D
 *                    Thailand Embedded Systems Association (TESA)
 *
 *******************************************************************************/

#ifndef CM55_ERROR_HANDLER_H
#define CM55_ERROR_HANDLER_H

#include <stdbool.h>

void cm55_handle_error(const char *message);

#endif /* CM55_ERROR_HANDLER_H */
