/*******************************************************************************
 * File Name        : tesa_display.h
 *
 * Description      : TESA display controller: GFXSS, vg_lite, LVGL init,
 *                    display/indev ports, and graphics task (CM55).
 *
 * Author           : Asst.Prof.Santi Nuratch, Ph.D
 *                    Thailand Embedded Systems Association (TESA)
 *
 *******************************************************************************/

#ifndef TESA_DISPLAY_H
#define TESA_DISPLAY_H

#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

#include "display_i2c_config.h"
#include "lv_port_disp.h"
#include "lvgl.h"
#include "mtb_disp_ws7p0dsi_drv.h"
#include "vg_lite.h"
#include "vg_lite_platform.h"

#define GPU_INT_PRIORITY (3U)
#define DC_INT_PRIORITY (3U)
#define I2C_CONTROLLER_IRQ_PRIORITY (2UL)

#define APP_BUFFER_COUNT (2U)
#define DEFAULT_GPU_CMD_BUFFER_SIZE ((64U) * (1024U))
#define GPU_TESSELLATION_BUFFER_SIZE ((MY_DISP_VER_RES) * 128U)
#define VGLITE_HEAP_SIZE                                                       \
  (((DEFAULT_GPU_CMD_BUFFER_SIZE) * (APP_BUFFER_COUNT)) +                      \
   ((GPU_TESSELLATION_BUFFER_SIZE) * (APP_BUFFER_COUNT)))

#define GPU_MEM_BASE (0x0U)
#define VG_PARAMS_POS (0UL)

#define GFX_TASK_NAME ("CM55 Gfx Task")
#define GFX_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE * 16)
#define GFX_TASK_PRIORITY (configMAX_PRIORITIES - 1)

extern TaskHandle_t rtos_cm55_gfx_task_handle;

BaseType_t tesa_display_init(void);
void tesa_display_task(void *arg);

static inline void tesa_display_tick(void) { lv_tick_inc(1); }

#endif
