/*******************************************************************************
 * File Name        : cm55_system.c
 *
 * Description      : Implementation of the CM55 system initialization (BSP,
 *                    RTC, retarget-io, LPTimer tickless idle).
 *
 * Author           : Asst.Prof.Santi Nuratch, Ph.D
 *                    Thailand Embedded Systems Association (TESA)
 * Version          : 1.0
 * Target           : PSoC Edge E84, CM55
 *
 *******************************************************************************/

#include "cm55_system.h"

#include "cy_syslib.h"
#include "cy_time.h"
#include "cybsp.h"
#include "cyabs_rtos.h"
#include "error_handler.h"
#include "retarget_io_init.h"

#include "mtb_hal_rtc.h"
#include "mtb_hal_lptimer.h"
#include "task.h"

/*******************************************************************************
 * Macros
 *******************************************************************************/

#define LPTIMER_1_WAIT_TIME_USEC (62U)
#define APP_LPTIMER_INTERRUPT_PRIORITY (1U)

/*******************************************************************************
 * Statics / Global Variables
 *******************************************************************************/

static mtb_hal_lptimer_t lptimer_obj;
static mtb_hal_rtc_t rtc_obj;

static system_tick_hook_cb_t s_tick_hook_cb = NULL;
static void *s_tick_hook_user_data = NULL;

/*******************************************************************************
 * Private Functions
 *******************************************************************************/

static void setup_clib_support(void)
{
  mtb_clib_support_init(&rtc_obj);
}

static void lptimer_interrupt_handler(void)
{
  mtb_hal_lptimer_process_interrupt(&lptimer_obj);
}

static void setup_tickless_idle_timer(void)
{
  cy_stc_sysint_t lptimer_intr_cfg = {.intrSrc = CYBSP_CM55_LPTIMER_1_IRQ,
                                      .intrPriority = APP_LPTIMER_INTERRUPT_PRIORITY};

  cy_en_sysint_status_t interrupt_init_status =
      Cy_SysInt_Init(&lptimer_intr_cfg, lptimer_interrupt_handler);

  if (CY_SYSINT_SUCCESS != interrupt_init_status)
  {
    cm55_handle_error(NULL);
  }

  NVIC_EnableIRQ(lptimer_intr_cfg.intrSrc);

  cy_en_mcwdt_status_t mcwdt_init_status =
      Cy_MCWDT_Init(CYBSP_CM55_LPTIMER_1_HW, &CYBSP_CM55_LPTIMER_1_config);

  if (CY_MCWDT_SUCCESS != mcwdt_init_status)
  {
    cm55_handle_error(NULL);
  }

  Cy_MCWDT_Enable(CYBSP_CM55_LPTIMER_1_HW, CY_MCWDT_CTR_Msk, LPTIMER_1_WAIT_TIME_USEC);

  cy_rslt_t result =
      mtb_hal_lptimer_setup(&lptimer_obj, &CYBSP_CM55_LPTIMER_1_hal_config);

  if (CY_RSLT_SUCCESS != result)
  {
    cm55_handle_error(NULL);
  }

  cyabs_rtos_set_lptimer(&lptimer_obj);
}

/*******************************************************************************
 * Public API
 *******************************************************************************/

bool cm55_system_init(void)
{
  if (CY_RSLT_SUCCESS != cybsp_init())
  {
    return false;
  }

  setup_clib_support();
  setup_tickless_idle_timer();
  init_retarget_io();
  __enable_irq();

  return true;
}

void system_register_tick_hook(system_tick_hook_cb_t callback, void *user_data)
{
  s_tick_hook_cb = callback;
  s_tick_hook_user_data = user_data;
}

void vApplicationTickHook(void)
{
  if (NULL != s_tick_hook_cb)
  {
    TickType_t tick = xTaskGetTickCountFromISR();
    system_tick_hook_params_t params = {
      .tick_count = (uint32_t)tick,
      .user_data = s_tick_hook_user_data,
    };
    s_tick_hook_cb(&params);
  }
}
