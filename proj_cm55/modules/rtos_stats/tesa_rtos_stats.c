/*******************************************************************************
 * File Name        : tesa_rtos_stats.c
 *
 * Description      : Implementation of FreeRTOS run-time stats: TCPWM timer
 *                    for run-time counter, idle percentage calculation.
 *
 * Author           : Asst.Prof.Santi Nuratch, Ph.D
 *                    Thailand Embedded Systems Association (TESA)
 * Version          : 1.0
 * Target           : PSoC Edge E84, CM55
 *
 *******************************************************************************/

#include "tesa_rtos_stats.h"
#include "FreeRTOS.h"
#include "cy_pdl.h"
#include "cybsp.h"
#include "cycfg.h"
#include "retarget_io_init.h"
#include "task.h"

#if (configGENERATE_RUN_TIME_STATS == 1)

/*******************************************************************************
 * Public API
 *******************************************************************************/

/*******************************************************************************
 * Function Name: setup_run_time_stats_timer
 ********************************************************************************
 * Summary:
 *  Initializes and starts the TCPWM counter used for run-time statistics.
 *
 * Parameters:
 *  none
 *
 * Return :
 *  void
 *
 *******************************************************************************/
void setup_run_time_stats_timer(void) {
  if (CY_TCPWM_SUCCESS !=
      Cy_TCPWM_Counter_Init(CYBSP_GENERAL_PURPOSE_TIMER_HW,
                            CYBSP_GENERAL_PURPOSE_TIMER_NUM,
                            &CYBSP_GENERAL_PURPOSE_TIMER_config)) {
    handle_app_error();
  }

  Cy_TCPWM_Counter_Enable(CYBSP_GENERAL_PURPOSE_TIMER_HW,
                          CYBSP_GENERAL_PURPOSE_TIMER_NUM);

  Cy_TCPWM_TriggerStart_Single(CYBSP_GENERAL_PURPOSE_TIMER_HW,
                               CYBSP_GENERAL_PURPOSE_TIMER_NUM);
}

/*******************************************************************************
 * Function Name: get_run_time_counter_value
 ********************************************************************************
 * Summary:
 *  Returns the current TCPWM counter value for run-time stats.
 *
 * Parameters:
 *  none
 *
 * Return :
 *  Current counter value
 *
 *******************************************************************************/
uint32_t get_run_time_counter_value(void) {
  return (Cy_TCPWM_Counter_GetCounter(CYBSP_GENERAL_PURPOSE_TIMER_HW,
                                      CYBSP_GENERAL_PURPOSE_TIMER_NUM));
}

/*******************************************************************************
 * Function Name: calculate_idle_percentage
 ********************************************************************************
 * Summary:
 *  Computes idle time percentage since last call from run-time counters.
 *
 * Parameters:
 *  none
 *
 * Return :
 *  Idle percentage (0-100)
 *
 *******************************************************************************/
uint32_t calculate_idle_percentage(void) {
  static uint32_t previousIdleTime = 0;
  static TickType_t previousTick = 0;
  uint32_t time_diff = 0;
  uint32_t idle_percent = 0;

  uint32_t currentIdleTime = ulTaskGetIdleRunTimeCounter();
  TickType_t currentTick = portGET_RUN_TIME_COUNTER_VALUE();

  time_diff = currentTick - previousTick;

  if ((currentIdleTime >= previousIdleTime) && (currentTick > previousTick)) {
    idle_percent = ((currentIdleTime - previousIdleTime) * 100) / time_diff;
  }

  previousIdleTime = ulTaskGetIdleRunTimeCounter();
  previousTick = portGET_RUN_TIME_COUNTER_VALUE();

  return idle_percent;
}
#endif
