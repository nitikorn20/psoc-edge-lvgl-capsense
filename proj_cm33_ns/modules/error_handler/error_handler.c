/*******************************************************************************
 * File Name        : error_handler.c
 *
 * Description      : Implementation of the CM33 centralized error handler.
 *
 * Author           : Asst.Prof.Santi Nuratch, Ph.D
 *                    Thailand Embedded Systems Association (TESA)
 *
 *******************************************************************************/

#include "error_handler.h"
#include "cy_pdl.h"
#include "cybsp.h"
#include <stdio.h>

/**
 * Centrally handles application errors.
 */
void handle_error(const char *message) {
  /* Disable all interrupts. */
  __disable_irq();

  /* Log the error if a message is provided.
   * Note: This uses the redirected printf (IPC Log) if ipc_log.h is included
   * in the compilation unit, otherwise it uses standard UART.
   */
  if (message != NULL) {
    printf("\n[ERROR] %s\n", message);
  } else {
    printf("\n[ERROR] Unspecified fatal error occurred.\n");
  }

  /* Infinite loop: Blink the User LED to indicate failure */
  while (true) {
    Cy_GPIO_Inv(CYBSP_USER_LED_PORT, CYBSP_USER_LED_PIN);
    Cy_SysLib_Delay(100);
  }
}
