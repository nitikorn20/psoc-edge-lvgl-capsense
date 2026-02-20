/*******************************************************************************
 * File Name        : tesa_rtos_stats.h
 *
 * Description      : FreeRTOS run-time statistics: timer setup, counter read,
 *                    and idle percentage (when configGENERATE_RUN_TIME_STATS).
 *
 * Author           : Asst.Prof.Santi Nuratch, Ph.D
 *                    Thailand Embedded Systems Association (TESA)
 *
 *******************************************************************************/

#ifndef TESA_RTOS_STATS_H
#define TESA_RTOS_STATS_H

#include <stdint.h>

#if (configGENERATE_RUN_TIME_STATS == 1)
#define TCPWM_TIMER_INT_PRIORITY (1U)

void setup_run_time_stats_timer(void);
uint32_t get_run_time_counter_value(void);
uint32_t calculate_idle_percentage(void);
#endif

#endif
