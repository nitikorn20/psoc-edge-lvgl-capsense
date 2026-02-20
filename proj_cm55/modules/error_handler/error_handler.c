/*******************************************************************************
 * File Name        : error_handler.c
 *
 * Description      : Implementation of CM55 fatal error handler: disables IRQ,
 *                    asserts, prints message, blinks user LED indefinitely.
 *
 * Author           : Asst.Prof.Santi Nuratch, Ph.D
 *                    Thailand Embedded Systems Association (TESA)
 * Version          : 1.0
 * Target           : PSoC Edge E84, CM55
 *
 *******************************************************************************/

#include "error_handler.h"
#include "cy_pdl.h"
#include "cybsp.h"
#include <stdio.h>

/*******************************************************************************
 * Public API
 *******************************************************************************/

/*******************************************************************************
 * Function Name: cm55_handle_error
 ********************************************************************************
 * Summary:
 *  Fatal error handler: disables interrupts, triggers assert, prints message
 *  to stdout, then blinks user LED in an infinite loop.
 *
 * Parameters:
 *  message - optional error string (NULL for generic message)
 *
 * Return :
 *  void (does not return)
 *
 *******************************************************************************/
void cm55_handle_error(const char *message)
{
  __disable_irq();

  CY_ASSERT(0);

  if (NULL != message)
  {
    (void)printf("\n[CM55 ERROR] %s\n", message);
  }
  else
  {
    (void)printf("\n[CM55 ERROR] Unspecified fatal error occurred.\n");
  }

  while (true)
  {
    Cy_GPIO_Inv(CYBSP_USER_LED_PORT, CYBSP_USER_LED_PIN);
#ifdef CYBSP_USER_LED
    Cy_GPIO_Inv(CYBSP_USER_LED_PORT, CYBSP_USER_LED_PIN);
#endif
    Cy_SysLib_Delay(100);
  }
}
