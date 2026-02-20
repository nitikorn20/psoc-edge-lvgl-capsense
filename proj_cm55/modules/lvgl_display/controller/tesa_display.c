/*******************************************************************************
 * File Name        : tesa_display.c
 *
 * Description      : TESA display: GFXSS/DC/GPU init, vg_lite, LVGL, display
 *                    and input ports; graphics task runs LVGL and examples.
 *
 * Author           : Asst.Prof.Santi Nuratch, Ph.D
 *                    Thailand Embedded Systems Association (TESA)
 * Version          : 1.0
 * Target           : PSoC Edge E84, CM55
 *
 *******************************************************************************/

#include "tesa_display.h"
#include "cy_graphics.h"
#include "lv_port_indev.h"
#include "ui/widgets/examples.h"
#include "cy_pdl.h"
#include "cybsp.h"
#include "cycfg.h"
#include "retarget_io_init.h"

/*******************************************************************************
 * Global Variables
 *******************************************************************************/

CY_SECTION(".cy_gpu_buf") uint8_t contiguous_mem[VGLITE_HEAP_SIZE] = {0xFF};

volatile void *vglite_heap_base = &contiguous_mem;

TaskHandle_t rtos_cm55_gfx_task_handle = NULL;

cy_stc_sysint_t dc_irq_cfg = {.intrSrc = GFXSS_DC_IRQ,
                              .intrPriority = DC_INT_PRIORITY};

cy_stc_sysint_t gpu_irq_cfg = {.intrSrc = GFXSS_GPU_IRQ,
                               .intrPriority = GPU_INT_PRIORITY};

cy_stc_scb_i2c_context_t disp_touch_i2c_controller_context;

cy_stc_sysint_t disp_touch_i2c_controller_irq_cfg = {
    .intrSrc = DISPLAY_I2C_CONTROLLER_IRQ,
    .intrPriority = I2C_CONTROLLER_IRQ_PRIORITY,
};

extern cy_stc_gfx_context_t gfx_context;
extern void *frame_buffer1;

/*******************************************************************************
 * Private Functions
 *******************************************************************************/

/*******************************************************************************
 * Function Name: dc_irq_handler
 ********************************************************************************
 * Summary:
 *  Display controller interrupt handler; clears interrupt and notifies GFX task.
 *
 * Parameters:
 *  none
 *
 * Return :
 *  void
 *
 *******************************************************************************/
static void dc_irq_handler(void) {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;

  Cy_GFXSS_Clear_DC_Interrupt(GFXSS, &gfx_context);

  xTaskNotifyFromISR(rtos_cm55_gfx_task_handle, 1, eSetValueWithOverwrite,
                     &xHigherPriorityTaskWoken);

  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/*******************************************************************************
 * Function Name: gpu_irq_handler
 ********************************************************************************
 * Summary:
 *  GPU interrupt handler; clears GFXSS GPU interrupt and vg_lite IRQ.
 *
 * Parameters:
 *  none
 *
 * Return :
 *  void
 *
 *******************************************************************************/
static void gpu_irq_handler(void) {
  Cy_GFXSS_Clear_GPU_Interrupt(GFXSS, &gfx_context);
  vg_lite_IRQHandler();
}

/*******************************************************************************
 * Function Name: disp_touch_i2c_controller_interrupt
 ********************************************************************************
 * Summary:
 *  I2C controller interrupt for display/touch; forwards to Cy_SCB_I2C_Interrupt.
 *
 * Parameters:
 *  none
 *
 * Return :
 *  void
 *
 *******************************************************************************/
static void disp_touch_i2c_controller_interrupt(void) {
  Cy_SCB_I2C_Interrupt(DISPLAY_I2C_CONTROLLER_HW,
                       &disp_touch_i2c_controller_context);
}

/*******************************************************************************
 * Public API
 *******************************************************************************/

/*******************************************************************************
 * Function Name: tesa_display_init
 ********************************************************************************
 * Summary:
 *  Creates the graphics FreeRTOS task (tesa_display_task).
 *
 * Parameters:
 *  none
 *
 * Return :
 *  pdPASS on success, otherwise task create failed
 *
 *******************************************************************************/
BaseType_t tesa_display_init(void) {
  return xTaskCreate(tesa_display_task, GFX_TASK_NAME, GFX_TASK_STACK_SIZE,
                     NULL, GFX_TASK_PRIORITY, &rtos_cm55_gfx_task_handle);
}

/*******************************************************************************
 * Function Name: tesa_display_task
 ********************************************************************************
 * Summary:
 *  Graphics task: initializes GFXSS, DC, GPU, I2C, panel, vg_lite, LVGL,
 *  display/indev ports, runs example; then LVGL timer loop.
 *
 * Parameters:
 *  arg - unused
 *
 * Return :
 *  void
 *
 *******************************************************************************/
void tesa_display_task(void *arg) {
  CY_UNUSED_PARAMETER(arg);

  cy_en_sysint_status_t sysint_status = CY_SYSINT_SUCCESS;
  cy_en_gfx_status_t gfx_status = CY_GFX_SUCCESS;
  vg_lite_error_t vglite_status = VG_LITE_SUCCESS;
  cy_rslt_t status = CY_RSLT_SUCCESS;
  cy_en_scb_i2c_status_t i2c_result = CY_SCB_I2C_SUCCESS;

  GFXSS_config.mipi_dsi_cfg = &mtb_disp_ws7p0dsi_dsi_config;

  GFXSS_config.dc_cfg->gfx_layer_config->width = MY_DISP_HOR_RES;
  GFXSS_config.dc_cfg->gfx_layer_config->height = MY_DISP_VER_RES;
  GFXSS_config.dc_cfg->display_width = MY_DISP_HOR_RES;
  GFXSS_config.dc_cfg->display_height = MY_DISP_VER_RES;

  GFXSS_config.dc_cfg->gfx_layer_config->buffer_address = frame_buffer1;
  GFXSS_config.dc_cfg->gfx_layer_config->uv_buffer_address = frame_buffer1;

  gfx_status = Cy_GFXSS_Init(GFXSS, &GFXSS_config, &gfx_context);

  if (CY_GFX_SUCCESS == gfx_status) {
    sysint_status = Cy_SysInt_Init(&dc_irq_cfg, dc_irq_handler);

    if (CY_SYSINT_SUCCESS != sysint_status) {
      printf("Error in registering DC interrupt: %d\r\n", sysint_status);
      handle_app_error();
    }

    NVIC_EnableIRQ(GFXSS_DC_IRQ);

    sysint_status = Cy_SysInt_Init(&gpu_irq_cfg, gpu_irq_handler);

    if (CY_SYSINT_SUCCESS != sysint_status) {
      printf("Error in registering GPU interrupt: %d\r\n", sysint_status);
      handle_app_error();
    }

    Cy_GFXSS_Enable_GPU_Interrupt(GFXSS);

    NVIC_EnableIRQ(GFXSS_GPU_IRQ);

    i2c_result = Cy_SCB_I2C_Init(DISPLAY_I2C_CONTROLLER_HW,
                                 &DISPLAY_I2C_CONTROLLER_config,
                                 &disp_touch_i2c_controller_context);

    if (CY_SCB_I2C_SUCCESS != i2c_result) {
      printf("I2C controller initialization failed !!\n");
      handle_app_error();
    }

    sysint_status = Cy_SysInt_Init(&disp_touch_i2c_controller_irq_cfg,
                                   &disp_touch_i2c_controller_interrupt);

    if (CY_SYSINT_SUCCESS != sysint_status) {
      printf("I2C controller interrupt initialization failed\r\n");
      handle_app_error();
    }

    NVIC_EnableIRQ(disp_touch_i2c_controller_irq_cfg.intrSrc);

    Cy_SCB_I2C_Enable(DISPLAY_I2C_CONTROLLER_HW);

    vTaskDelay(pdMS_TO_TICKS(500));

    status = mtb_disp_ws7p0dsi_panel_init(DISPLAY_I2C_CONTROLLER_HW,
                                          &disp_touch_i2c_controller_context);

    if (CY_RSLT_SUCCESS != status) {
      printf("Waveshare 7-Inch R-Pi display init failed with status = %u\r\n",
             (unsigned int)status);
      CY_ASSERT(0);
    }
    vg_module_parameters_t vg_params;
    vg_params.register_mem_base = (uint32_t)GFXSS_GFXSS_GPU_GCNANO;
    vg_params.gpu_mem_base[VG_PARAMS_POS] = GPU_MEM_BASE;
    vg_params.contiguous_mem_base[VG_PARAMS_POS] = vglite_heap_base;
    vg_params.contiguous_mem_size[VG_PARAMS_POS] = VGLITE_HEAP_SIZE;

    vg_lite_init_mem(&vg_params);

    vglite_status = vg_lite_init((MY_DISP_HOR_RES) / 4, (MY_DISP_VER_RES) / 4);

    if (VG_LITE_SUCCESS == vglite_status) {
      lv_init();
      lv_port_disp_init();
      lv_port_indev_init();
      run_example();

    } else {
      printf("vg_lite_init failed, status: %d\r\n", vglite_status);

      vg_lite_close();
      handle_app_error();
    }
  } else {
    printf("Graphics subsystem init failed, status: %d\r\n", gfx_status);
    handle_app_error();
  }

  for (;;) {
    uint32_t time_till_next = lv_timer_handler();
    if (time_till_next > 5) {
      time_till_next = 5;
    }
    vTaskDelay(pdMS_TO_TICKS(time_till_next));
  }
}
