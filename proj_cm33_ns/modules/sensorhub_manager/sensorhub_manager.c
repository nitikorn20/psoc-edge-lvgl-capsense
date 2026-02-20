#include "sensorhub_manager.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "cy_autanalog.h"
#include "cy_pdl.h"
#include "cy_scb_i2c.h"
#include "cybsp.h"
#include "cycfg_peripherals.h"
#include "mtb_bmi270.h"
#include "mtb_bmm350.h"
#include "mtb_hal_i2c.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SENSORHUB_MANAGER_TASK_STACK (2048U)
#define SENSORHUB_MANAGER_TASK_PRIO (2U)
#define SENSORHUB_MANAGER_QUEUE_LEN (12U)
#define SENSORHUB_MANAGER_IDLE_POLL_MS (1000U)
#define SENSORHUB_MANAGER_FAST_TICK_MS (20U)
#define SENSORHUB_MANAGER_DEFAULT_PERIOD_MS (400U)
#define SENSORHUB_MANAGER_MIN_PERIOD_MS (100U)
#define SENSORHUB_MANAGER_MAX_PERIOD_MS (5000U)
#define SENSORHUB_MANAGER_DEFAULT_MASK                                                                                   \
  (IPC_SENSORHUB_SENSOR_MASK_POT | IPC_SENSORHUB_SENSOR_MASK_BMI270 | IPC_SENSORHUB_SENSOR_MASK_CAPSENSE |            \
   IPC_SENSORHUB_SENSOR_MASK_BMM350)
#define SENSORHUB_MANAGER_POT_VREF_MV (1800U)
#define SENSORHUB_MANAGER_POT_SAR_INDEX (0U)
#define SENSORHUB_MANAGER_POT_SAR_SEQ (0U)
#define SENSORHUB_MANAGER_POT_SAR_CHANNEL (0U)
#define SENSORHUB_MANAGER_BMI_G_RANGE (2)
#define SENSORHUB_MANAGER_BMI_GYRO_DPS (2000.0f)
#define SENSORHUB_MANAGER_GRAVITY_EARTH (9.80665f)
#define SENSORHUB_MANAGER_DEG_TO_RAD (0.01745f)
#define SENSORHUB_MANAGER_BMI_READ_RETRY_COUNT (3U)
#define SENSORHUB_MANAGER_BMI_READ_RETRY_DELAY_MS (10U)
#define SENSORHUB_MANAGER_BMI_REINIT_ATTEMPTS (2U)
#define SENSORHUB_MANAGER_BMI_FAIL_STREAK_DISABLE (5U)
#define SENSORHUB_MANAGER_BMI_ZERO_STREAK_RECONFIG (8U)
#define SENSORHUB_MANAGER_BMI_WARMUP_SAMPLES (2U)
#define SENSORHUB_MANAGER_BMM_I3C_IRQ_PRIO (2U)
#define SENSORHUB_MANAGER_BMM_FAIL_STREAK_DISABLE (5U)
#define SENSORHUB_MANAGER_CAPSENSE_I2C_ADDRESS (0x08U)
#define SENSORHUB_MANAGER_CAPSENSE_READ_SIZE (3U)
#define SENSORHUB_MANAGER_CAPSENSE_HEARTBEAT_MS (1000U)
#define SENSORHUB_MANAGER_CAPSENSE_DEADBAND (2U)
#define SENSORHUB_MANAGER_TOUCH_I2C_ADDR_PRIMARY (0x14U)
#define SENSORHUB_MANAGER_TOUCH_I2C_ADDR_SECONDARY (0x5DU)
#define SENSORHUB_MANAGER_TOUCH_REG_STATUS (0x814EU)
#define SENSORHUB_MANAGER_TOUCH_REG_POINT1 (0x8150U)
#define SENSORHUB_MANAGER_TOUCH_HEARTBEAT_MS (200U)
#define SENSORHUB_MANAGER_TOUCH_STALE_RELEASE_MS (300U)

typedef enum
{
  SENSORHUB_MANAGER_CMD_START = 0U,
  SENSORHUB_MANAGER_CMD_STOP = 1U,
  SENSORHUB_MANAGER_CMD_STATUS = 2U
} sensorhub_manager_cmd_t;

typedef struct
{
  sensorhub_manager_cmd_t cmd;
  ipc_sensorhub_start_request_t request;
} sensorhub_manager_msg_t;

typedef struct
{
  QueueHandle_t queue;
  TaskHandle_t task;
  sensorhub_manager_event_cb_t callback;
  void *callback_user_data;
  bool started;
  bool running;
  uint8_t sensors_mask;
  uint16_t period_ms;
  uint16_t sequence;
  bool pot_ready;
  bool i2c_ready;
  bool i3c_ready;
  bool i3c_irq_ready;
  bool bmi_ready;
  bool bmm_ready;
  bool capsense_ready;
  bool touch_ready;
  uint8_t bmi_fail_streak;
  uint8_t bmi_zero_data_streak;
  uint8_t bmi_warmup_samples;
  uint8_t bmm_fail_streak;
  uint8_t touch_i2c_addr;
  bool capsense_have_baseline;
  bool capsense_last_btn0_pressed;
  bool capsense_last_btn1_pressed;
  uint8_t capsense_last_slider;
  uint8_t capsense_idle_btn0_code;
  uint8_t capsense_idle_btn1_code;
  uint32_t capsense_elapsed_ms_since_heartbeat;
  bool touch_last_pressed;
  uint16_t touch_last_x;
  uint16_t touch_last_y;
  uint8_t touch_last_points;
  uint32_t touch_elapsed_ms_since_heartbeat;
  uint32_t touch_last_update_ms;
  uint16_t fast_tick_accum_ms;
  mtb_hal_i2c_t i2c_hal_obj;
  cy_stc_scb_i2c_context_t i2c_pdl_context;
  cy_stc_i3c_context_t i3c_pdl_context;
  cy_stc_i3c_device_t bmm_i3c_device;
  mtb_bmi270_t bmi;
  mtb_bmm350_t bmm;
} sensorhub_manager_ctx_t;

static sensorhub_manager_ctx_t s_ctx;

static void sensorhub_manager_emit(sensorhub_manager_event_t event, const void *data, uint32_t count)
{
  if (NULL != s_ctx.callback)
  {
    s_ctx.callback(event, data, count, s_ctx.callback_user_data);
  }
}

static void sensorhub_manager_emit_status(ipc_sensorhub_reason_t reason)
{
  ipc_sensorhub_status_t status;

  (void)memset(&status, 0, sizeof(status));
  status.state = s_ctx.running ? (uint8_t)IPC_SENSORHUB_STATE_RUNNING : (uint8_t)IPC_SENSORHUB_STATE_STOPPED;
  status.sensors_mask = s_ctx.sensors_mask;
  status.reason = (uint16_t)reason;
  sensorhub_manager_emit(SENSORHUB_MANAGER_EVENT_STATUS, &status, 1U);
}

static float sensorhub_absf(float value)
{
  return (value < 0.0f) ? -value : value;
}

static bool sensorhub_manager_is_ascii_digit(uint8_t value)
{
  return (value >= (uint8_t)'0') && (value <= (uint8_t)'9');
}

static uint8_t sensorhub_manager_capsense_normalize_btn(uint8_t raw)
{
  if ((raw >= 30U) && (raw <= 32U))
  {
    return (uint8_t)(raw - 30U);
  }
  if (sensorhub_manager_is_ascii_digit(raw))
  {
    return (uint8_t)(raw - (uint8_t)'0');
  }
  if (raw <= 2U)
  {
    return raw;
  }
  return (raw != 0U) ? 1U : 0U;
}

static bool sensorhub_manager_i2c_start_write(uint8_t addr)
{
  cy_en_scb_i2c_status_t status;
  cy_stc_scb_i2c_context_t *ctx = &s_ctx.i2c_pdl_context;

  status = (ctx->state == CY_SCB_I2C_IDLE)
               ? Cy_SCB_I2C_MasterSendStart(CYBSP_I2C_CONTROLLER_HW, addr, CY_SCB_I2C_WRITE_XFER, 0U, ctx)
               : Cy_SCB_I2C_MasterSendReStart(CYBSP_I2C_CONTROLLER_HW, addr, CY_SCB_I2C_WRITE_XFER, 0U, ctx);
  return (CY_SCB_I2C_SUCCESS == status);
}

static bool sensorhub_manager_i2c_start_read(uint8_t addr)
{
  cy_en_scb_i2c_status_t status;
  cy_stc_scb_i2c_context_t *ctx = &s_ctx.i2c_pdl_context;

  status = (ctx->state == CY_SCB_I2C_IDLE)
               ? Cy_SCB_I2C_MasterSendStart(CYBSP_I2C_CONTROLLER_HW, addr, CY_SCB_I2C_READ_XFER, 0U, ctx)
               : Cy_SCB_I2C_MasterSendReStart(CYBSP_I2C_CONTROLLER_HW, addr, CY_SCB_I2C_READ_XFER, 0U, ctx);
  return (CY_SCB_I2C_SUCCESS == status);
}

static void sensorhub_manager_i2c_stop(void)
{
  (void)Cy_SCB_I2C_MasterSendStop(CYBSP_I2C_CONTROLLER_HW, 0U, &s_ctx.i2c_pdl_context);
}

static bool sensorhub_manager_i2c_write_bytes(const uint8_t *buffer, uint32_t length)
{
  cy_en_scb_i2c_status_t status;

  if ((NULL == buffer) || (0U == length))
  {
    return false;
  }

  for (uint32_t i = 0U; i < length; i++)
  {
    status = Cy_SCB_I2C_MasterWriteByte(CYBSP_I2C_CONTROLLER_HW, buffer[i], 0U, &s_ctx.i2c_pdl_context);
    if (CY_SCB_I2C_SUCCESS != status)
    {
      return false;
    }
  }
  return true;
}

static bool sensorhub_manager_i2c_read_bytes(uint8_t *buffer, uint32_t length)
{
  cy_en_scb_i2c_status_t status;

  if ((NULL == buffer) || (0U == length))
  {
    return false;
  }

  for (uint32_t i = 0U; i < length; i++)
  {
    cy_en_scb_i2c_command_t ack = (i + 1U < length) ? CY_SCB_I2C_ACK : CY_SCB_I2C_NAK;
    status = Cy_SCB_I2C_MasterReadByte(CYBSP_I2C_CONTROLLER_HW, ack, &buffer[i], 0U, &s_ctx.i2c_pdl_context);
    if (CY_SCB_I2C_SUCCESS != status)
    {
      return false;
    }
  }
  return true;
}

static bool sensorhub_manager_i2c_read_register(uint8_t addr, uint16_t reg, uint8_t *buffer, uint32_t length)
{
  uint8_t reg_bytes[2];
  bool ok = false;

  if ((NULL == buffer) || (0U == length))
  {
    return false;
  }

  reg_bytes[0] = (uint8_t)((reg >> 8U) & 0xFFU);
  reg_bytes[1] = (uint8_t)(reg & 0xFFU);

  if (!sensorhub_manager_i2c_start_write(addr))
  {
    sensorhub_manager_i2c_stop();
    return false;
  }
  if (!sensorhub_manager_i2c_write_bytes(reg_bytes, sizeof(reg_bytes)))
  {
    sensorhub_manager_i2c_stop();
    return false;
  }
  if (!sensorhub_manager_i2c_start_read(addr))
  {
    sensorhub_manager_i2c_stop();
    return false;
  }
  ok = sensorhub_manager_i2c_read_bytes(buffer, length);
  sensorhub_manager_i2c_stop();
  return ok;
}

static bool sensorhub_manager_i2c_write_register(uint8_t addr, uint16_t reg, const uint8_t *buffer, uint32_t length)
{
  uint8_t reg_bytes[2];
  bool ok = false;

  if ((NULL == buffer) || (0U == length))
  {
    return false;
  }

  reg_bytes[0] = (uint8_t)((reg >> 8U) & 0xFFU);
  reg_bytes[1] = (uint8_t)(reg & 0xFFU);

  if (!sensorhub_manager_i2c_start_write(addr))
  {
    sensorhub_manager_i2c_stop();
    return false;
  }
  if (!sensorhub_manager_i2c_write_bytes(reg_bytes, sizeof(reg_bytes)))
  {
    sensorhub_manager_i2c_stop();
    return false;
  }
  ok = sensorhub_manager_i2c_write_bytes(buffer, length);
  sensorhub_manager_i2c_stop();
  return ok;
}

static bool sensorhub_manager_i2c_read_capsense(uint8_t *buffer, uint32_t length)
{
  bool ok = false;

  if ((NULL == buffer) || (0U == length))
  {
    return false;
  }

  if (!sensorhub_manager_i2c_start_read(SENSORHUB_MANAGER_CAPSENSE_I2C_ADDRESS))
  {
    sensorhub_manager_i2c_stop();
    return false;
  }
  ok = sensorhub_manager_i2c_read_bytes(buffer, length);
  sensorhub_manager_i2c_stop();
  return ok;
}

static bool sensorhub_manager_init_pot(void)
{
  cy_rslt_t rslt;

  if (s_ctx.pot_ready)
  {
    return true;
  }

  rslt = Cy_AutAnalog_Init(&autonomous_analog_init);
  if (CY_AUTANALOG_SUCCESS != rslt)
  {
    (void)printf("[CM33] POT init failed rslt=0x%08lx\n", (unsigned long)rslt);
    return false;
  }

  Cy_AutAnalog_SetInterruptMask(CY_AUTANALOG_INT_SAR0_RESULT);
  Cy_AutAnalog_StartAutonomousControl();
  s_ctx.pot_ready = true;
  return true;
}

static int16_t sensorhub_manager_read_pot_mv(bool *ok)
{
  int32_t sar_count;
  int16_t sar_mv;

  if (NULL != ok)
  {
    *ok = false;
  }
  if (!s_ctx.pot_ready)
  {
    return 0;
  }

  sar_count = Cy_AutAnalog_SAR_ReadResult(SENSORHUB_MANAGER_POT_SAR_INDEX, CY_AUTANALOG_SAR_INPUT_GPIO,
                                          SENSORHUB_MANAGER_POT_SAR_CHANNEL);

  sar_mv = Cy_AutAnalog_SAR_CountsTo_mVolts(SENSORHUB_MANAGER_POT_SAR_INDEX, false, SENSORHUB_MANAGER_POT_SAR_SEQ,
                                            CY_AUTANALOG_SAR_INPUT_GPIO, SENSORHUB_MANAGER_POT_SAR_CHANNEL,
                                            SENSORHUB_MANAGER_POT_VREF_MV, sar_count);
  if (sar_mv < 0)
  {
    sar_mv = 0;
  }
  else if (sar_mv > (int16_t)SENSORHUB_MANAGER_POT_VREF_MV)
  {
    sar_mv = (int16_t)SENSORHUB_MANAGER_POT_VREF_MV;
  }
  if (NULL != ok)
  {
    *ok = true;
  }
  return sar_mv;
}

static uint16_t sensorhub_manager_pot_mv_to_pct_x10(int16_t mv)
{
  return (uint16_t)((((uint32_t)mv * 1000U) + (SENSORHUB_MANAGER_POT_VREF_MV / 2U)) / SENSORHUB_MANAGER_POT_VREF_MV);
}

static void sensorhub_manager_emit_pot_sample(uint32_t now_ms)
{
  ipc_sensorhub_sample_t sample;
  bool ok = false;
  int16_t mv = sensorhub_manager_read_pot_mv(&ok);

  if (!ok)
  {
    return;
  }

  (void)memset(&sample, 0, sizeof(sample));
  sample.sensor_type = (uint8_t)IPC_SENSORHUB_SENSOR_POT;
  sample.sequence = s_ctx.sequence++;
  sample.timestamp_ms = now_ms;
  sample.data.pot.pct_x10 = sensorhub_manager_pot_mv_to_pct_x10(mv);
  sample.data.pot.mv = mv;

  sensorhub_manager_emit(SENSORHUB_MANAGER_EVENT_SAMPLE, &sample, 1U);
}

static uint8_t sensorhub_manager_calc_orient(float ax, float ay, float az)
{
  float abs_ax = sensorhub_absf(ax);
  float abs_ay = sensorhub_absf(ay);
  float abs_az = sensorhub_absf(az);

  if ((abs_ax >= abs_ay) && (abs_ax >= abs_az))
  {
    return (ax >= 0.0f) ? 0U : 1U;
  }
  if ((abs_ay >= abs_ax) && (abs_ay >= abs_az))
  {
    return (ay >= 0.0f) ? 2U : 3U;
  }
  return (az >= 0.0f) ? 4U : 5U;
}

static float sensorhub_manager_bmi_lsb_to_mps2(int16_t val, int8_t g_range, uint8_t bit_width)
{
  float half_scale = (float)(1U << (bit_width - 1U));
  return (SENSORHUB_MANAGER_GRAVITY_EARTH * (float)val * (float)g_range) / half_scale;
}

static float sensorhub_manager_bmi_lsb_to_rps(int16_t val, float dps, uint8_t bit_width)
{
  float half_scale = (float)(1U << (bit_width - 1U));
  return SENSORHUB_MANAGER_DEG_TO_RAD * (dps / half_scale) * (float)val;
}

static bool sensorhub_manager_init_i2c(void)
{
  cy_en_scb_i2c_status_t init_status;
  cy_rslt_t rslt;

  if (s_ctx.i2c_ready)
  {
    return true;
  }

  init_status = Cy_SCB_I2C_Init(CYBSP_I2C_CONTROLLER_HW, &CYBSP_I2C_CONTROLLER_config, &s_ctx.i2c_pdl_context);
  if (CY_SCB_I2C_SUCCESS != init_status)
  {
    (void)printf("[CM33] I2C init failed status=%u\n", (unsigned int)init_status);
    return false;
  }
  Cy_SCB_I2C_Enable(CYBSP_I2C_CONTROLLER_HW);

  rslt = mtb_hal_i2c_setup(&s_ctx.i2c_hal_obj, &CYBSP_I2C_CONTROLLER_hal_config, &s_ctx.i2c_pdl_context, NULL);
  if (CY_RSLT_SUCCESS != rslt)
  {
    (void)printf("[CM33] I2C HAL setup failed rslt=0x%08lx\n", (unsigned long)rslt);
    return false;
  }

  s_ctx.i2c_ready = true;
  return true;
}

void CYBSP_I3C_CONTROLLER_Interrupt(void)
{
  Cy_I3C_Interrupt(CYBSP_I3C_CONTROLLER_HW, &s_ctx.i3c_pdl_context);
}

static bool sensorhub_manager_init_i3c_irq(void)
{
  cy_stc_sysint_t i3c_irq_cfg = {
      .intrSrc = CYBSP_I3C_CONTROLLER_IRQ,
      .intrPriority = SENSORHUB_MANAGER_BMM_I3C_IRQ_PRIO,
  };

  if (s_ctx.i3c_irq_ready)
  {
    return true;
  }

  if (CY_SYSINT_SUCCESS != Cy_SysInt_Init(&i3c_irq_cfg, CYBSP_I3C_CONTROLLER_Interrupt))
  {
    (void)printf("[CM33] I3C IRQ init failed\n");
    return false;
  }

  NVIC_EnableIRQ((IRQn_Type)i3c_irq_cfg.intrSrc);
  s_ctx.i3c_irq_ready = true;
  return true;
}

static bool sensorhub_manager_init_i3c(void)
{
  cy_en_i3c_status_t i3c_status;

  if (s_ctx.i3c_ready)
  {
    return true;
  }

  if (!sensorhub_manager_init_i3c_irq())
  {
    return false;
  }

  i3c_status = Cy_I3C_Init(CYBSP_I3C_CONTROLLER_HW, &CYBSP_I3C_CONTROLLER_config, &s_ctx.i3c_pdl_context);
  if (CY_I3C_SUCCESS != i3c_status)
  {
    (void)printf("[CM33] I3C init failed status=%u\n", (unsigned int)i3c_status);
    return false;
  }

  Cy_I3C_Enable(CYBSP_I3C_CONTROLLER_HW, &s_ctx.i3c_pdl_context);
  s_ctx.i3c_ready = true;
  return true;
}

static bool sensorhub_manager_init_bmi(void)
{
  cy_rslt_t rslt;
  mtb_bmi270_data_t data;

  if (s_ctx.bmi_ready)
  {
    return true;
  }

  if (!sensorhub_manager_init_i2c())
  {
    return false;
  }

  rslt = mtb_bmi270_init_i2c(&s_ctx.bmi, &s_ctx.i2c_hal_obj, MTB_BMI270_ADDRESS_DEFAULT);
  if (CY_RSLT_SUCCESS == rslt)
  {
    rslt = mtb_bmi270_config_default(&s_ctx.bmi);
  }
  if (CY_RSLT_SUCCESS == rslt)
  {
    rslt = mtb_bmi270_read(&s_ctx.bmi, &data);
  }
  if (CY_RSLT_SUCCESS != rslt)
  {
    (void)printf("[CM33] BMI270 init failed rslt=0x%08lx\n", (unsigned long)rslt);
    return false;
  }

  s_ctx.bmi_ready = true;
  s_ctx.bmi_zero_data_streak = 0U;
  s_ctx.bmi_warmup_samples = SENSORHUB_MANAGER_BMI_WARMUP_SAMPLES;
  return true;
}

static bool sensorhub_manager_init_bmm(void)
{
  cy_rslt_t rslt;

  if (s_ctx.bmm_ready)
  {
    return true;
  }

  if (!sensorhub_manager_init_i3c())
  {
    return false;
  }

  (void)memset(&s_ctx.bmm, 0, sizeof(s_ctx.bmm));
  s_ctx.bmm_i3c_device.staticAddress = MTB_BMM350_ADDRESS_SEC;
  rslt = mtb_bmm350_init_i3c(&s_ctx.bmm, CYBSP_I3C_CONTROLLER_HW, &s_ctx.i3c_pdl_context, &s_ctx.bmm_i3c_device);

  if (CY_RSLT_SUCCESS == rslt)
  {
    s_ctx.bmm_ready = true;
    return true;
  }

  (void)printf("[CM33] BMM350 init failed rslt=0x%08lx\n", (unsigned long)rslt);
  return false;
}

static bool sensorhub_manager_init_capsense(void)
{
  if (s_ctx.capsense_ready)
  {
    return true;
  }

  if (!sensorhub_manager_init_i2c())
  {
    return false;
  }

  s_ctx.capsense_ready = true;
  s_ctx.capsense_have_baseline = false;
  s_ctx.capsense_elapsed_ms_since_heartbeat = 0U;
  return true;
}

static void sensorhub_manager_disable_bmm_fallback(void)
{
  s_ctx.bmm_ready = false;
  s_ctx.sensors_mask &= (uint8_t)(~IPC_SENSORHUB_SENSOR_MASK_BMM350);
  s_ctx.running = (s_ctx.sensors_mask != 0U);
  (void)printf("[CM33] BMM350 disabled after repeated failures\n");
  sensorhub_manager_emit_status(IPC_SENSORHUB_REASON_ERROR);
}

static bool sensorhub_manager_init_touch(void)
{
  uint8_t status = 0U;
  uint8_t clear_val = 0U;

  if (s_ctx.touch_ready)
  {
    return true;
  }

  if (!sensorhub_manager_init_i2c())
  {
    return false;
  }

  if (sensorhub_manager_i2c_read_register(SENSORHUB_MANAGER_TOUCH_I2C_ADDR_PRIMARY, SENSORHUB_MANAGER_TOUCH_REG_STATUS,
                                          &status, 1U))
  {
    s_ctx.touch_i2c_addr = SENSORHUB_MANAGER_TOUCH_I2C_ADDR_PRIMARY;
  }
  else if (sensorhub_manager_i2c_read_register(SENSORHUB_MANAGER_TOUCH_I2C_ADDR_SECONDARY,
                                               SENSORHUB_MANAGER_TOUCH_REG_STATUS, &status, 1U))
  {
    s_ctx.touch_i2c_addr = SENSORHUB_MANAGER_TOUCH_I2C_ADDR_SECONDARY;
  }
  else
  {
    return false;
  }

  (void)sensorhub_manager_i2c_write_register(s_ctx.touch_i2c_addr, SENSORHUB_MANAGER_TOUCH_REG_STATUS, &clear_val, 1U);

  s_ctx.touch_ready = true;
  s_ctx.touch_last_pressed = false;
  s_ctx.touch_last_x = 0U;
  s_ctx.touch_last_y = 0U;
  s_ctx.touch_last_points = 0U;
  s_ctx.touch_last_update_ms = 0U;
  s_ctx.touch_elapsed_ms_since_heartbeat = 0U;
  return true;
}

static void sensorhub_manager_emit_capsense_sample(uint32_t now_ms)
{
  uint8_t buffer[SENSORHUB_MANAGER_CAPSENSE_READ_SIZE];
  uint8_t btn0_code;
  uint8_t btn1_code;
  uint8_t slider;
  bool btn0_pressed;
  bool btn1_pressed;
  bool should_update = false;
  bool should_heartbeat;
  ipc_sensorhub_sample_t sample;

  if (0U == (s_ctx.sensors_mask & IPC_SENSORHUB_SENSOR_MASK_CAPSENSE))
  {
    return;
  }

  if ((!s_ctx.capsense_ready) && (!sensorhub_manager_init_capsense()))
  {
    return;
  }

  if (!sensorhub_manager_i2c_read_capsense(buffer, sizeof(buffer)))
  {
    return;
  }

  btn0_code = sensorhub_manager_capsense_normalize_btn(buffer[0]);
  btn1_code = sensorhub_manager_capsense_normalize_btn(buffer[1]);
  slider = buffer[2];

  if (!s_ctx.capsense_have_baseline)
  {
    s_ctx.capsense_have_baseline = true;
    s_ctx.capsense_idle_btn0_code = btn0_code;
    s_ctx.capsense_idle_btn1_code = btn1_code;
    s_ctx.capsense_last_btn0_pressed = false;
    s_ctx.capsense_last_btn1_pressed = false;
    s_ctx.capsense_last_slider = slider;
    should_update = true;
  }
  else
  {
    btn0_pressed = (btn0_code != s_ctx.capsense_idle_btn0_code);
    btn1_pressed = (btn1_code != s_ctx.capsense_idle_btn1_code);
    if ((btn0_pressed != s_ctx.capsense_last_btn0_pressed) || (btn1_pressed != s_ctx.capsense_last_btn1_pressed))
    {
      should_update = true;
    }
    else
    {
      uint8_t slider_delta = (slider > s_ctx.capsense_last_slider) ? (uint8_t)(slider - s_ctx.capsense_last_slider)
                                                                    : (uint8_t)(s_ctx.capsense_last_slider - slider);
      if (slider_delta >= SENSORHUB_MANAGER_CAPSENSE_DEADBAND)
      {
        should_update = true;
      }
    }
  }

  s_ctx.capsense_elapsed_ms_since_heartbeat += SENSORHUB_MANAGER_FAST_TICK_MS;
  should_heartbeat = (s_ctx.capsense_elapsed_ms_since_heartbeat >= SENSORHUB_MANAGER_CAPSENSE_HEARTBEAT_MS);
  if (should_heartbeat)
  {
    s_ctx.capsense_elapsed_ms_since_heartbeat = 0U;
  }

  if (!should_update && !should_heartbeat)
  {
    return;
  }

  btn0_pressed = (btn0_code != s_ctx.capsense_idle_btn0_code);
  btn1_pressed = (btn1_code != s_ctx.capsense_idle_btn1_code);
  s_ctx.capsense_last_btn0_pressed = btn0_pressed;
  s_ctx.capsense_last_btn1_pressed = btn1_pressed;
  s_ctx.capsense_last_slider = slider;

  (void)memset(&sample, 0, sizeof(sample));
  sample.sensor_type = (uint8_t)IPC_SENSORHUB_SENSOR_CAPSENSE;
  sample.sequence = s_ctx.sequence++;
  sample.timestamp_ms = now_ms;
  sample.data.capsense.btn0_pressed = btn0_pressed ? 1U : 0U;
  sample.data.capsense.btn1_pressed = btn1_pressed ? 1U : 0U;
  sample.data.capsense.slider = slider;
  sensorhub_manager_emit(SENSORHUB_MANAGER_EVENT_SAMPLE, &sample, 1U);
}

static void sensorhub_manager_emit_touch_sample(uint32_t now_ms)
{
  uint8_t status = 0U;
  uint8_t clear_val = 0U;
  uint8_t points = 0U;
  bool has_new_frame = false;
  bool pressed = s_ctx.touch_last_pressed;
  uint16_t x = s_ctx.touch_last_x;
  uint16_t y = s_ctx.touch_last_y;
  bool should_emit = false;
  bool should_heartbeat;
  ipc_sensorhub_sample_t sample;

  if (0U == (s_ctx.sensors_mask & IPC_SENSORHUB_SENSOR_MASK_TOUCH))
  {
    return;
  }

  if ((!s_ctx.touch_ready) && (!sensorhub_manager_init_touch()))
  {
    return;
  }

  if (sensorhub_manager_i2c_read_register(s_ctx.touch_i2c_addr, SENSORHUB_MANAGER_TOUCH_REG_STATUS, &status, 1U))
  {
    if (0U != (status & 0x80U))
    {
      uint8_t point_buf[4];
      has_new_frame = true;
      points = (uint8_t)(status & 0x0FU);

      if ((points > 0U) &&
          sensorhub_manager_i2c_read_register(s_ctx.touch_i2c_addr, SENSORHUB_MANAGER_TOUCH_REG_POINT1, point_buf, sizeof(point_buf)))
      {
        x = (uint16_t)(((uint16_t)point_buf[1] << 8U) | point_buf[0]);
        y = (uint16_t)(((uint16_t)point_buf[3] << 8U) | point_buf[2]);
        pressed = true;
      }
      else
      {
        pressed = false;
      }

      (void)sensorhub_manager_i2c_write_register(s_ctx.touch_i2c_addr, SENSORHUB_MANAGER_TOUCH_REG_STATUS, &clear_val, 1U);
    }
  }
  else
  {
    s_ctx.touch_ready = false;
    return;
  }

  if (has_new_frame)
  {
    if ((pressed != s_ctx.touch_last_pressed) || (x != s_ctx.touch_last_x) || (y != s_ctx.touch_last_y) ||
        (points != s_ctx.touch_last_points))
    {
      should_emit = true;
    }
    s_ctx.touch_last_update_ms = now_ms;
  }
  else if (s_ctx.touch_last_pressed && ((now_ms - s_ctx.touch_last_update_ms) >= SENSORHUB_MANAGER_TOUCH_STALE_RELEASE_MS))
  {
    pressed = false;
    points = 0U;
    should_emit = true;
  }

  s_ctx.touch_elapsed_ms_since_heartbeat += SENSORHUB_MANAGER_FAST_TICK_MS;
  should_heartbeat = (s_ctx.touch_elapsed_ms_since_heartbeat >= SENSORHUB_MANAGER_TOUCH_HEARTBEAT_MS);
  if (should_heartbeat)
  {
    s_ctx.touch_elapsed_ms_since_heartbeat = 0U;
  }

  if (!should_emit && !should_heartbeat)
  {
    return;
  }

  s_ctx.touch_last_pressed = pressed;
  s_ctx.touch_last_x = x;
  s_ctx.touch_last_y = y;
  s_ctx.touch_last_points = points;

  (void)memset(&sample, 0, sizeof(sample));
  sample.sensor_type = (uint8_t)IPC_SENSORHUB_SENSOR_TOUCH;
  sample.sequence = s_ctx.sequence++;
  sample.timestamp_ms = now_ms;
  sample.data.touch.pressed = pressed ? 1U : 0U;
  sample.data.touch.points = points;
  sample.data.touch.x = x;
  sample.data.touch.y = y;
  sensorhub_manager_emit(SENSORHUB_MANAGER_EVENT_SAMPLE, &sample, 1U);
}

static bool sensorhub_manager_try_read_bmi(mtb_bmi270_data_t *data)
{
  cy_rslt_t rslt = (cy_rslt_t)0xFFFFFFFFUL;

  if ((NULL == data) || (!s_ctx.bmi_ready))
  {
    return false;
  }

  for (uint32_t attempt = 0U; attempt < SENSORHUB_MANAGER_BMI_READ_RETRY_COUNT; attempt++)
  {
    rslt = mtb_bmi270_read(&s_ctx.bmi, data);
    if (CY_RSLT_SUCCESS == rslt)
    {
      return true;
    }
    if (attempt + 1U < SENSORHUB_MANAGER_BMI_READ_RETRY_COUNT)
    {
      vTaskDelay(pdMS_TO_TICKS(SENSORHUB_MANAGER_BMI_READ_RETRY_DELAY_MS));
    }
  }

  (void)printf("[CM33] BMI270 read failed rslt=0x%08lx\n", (unsigned long)rslt);
  return false;
}

static bool sensorhub_manager_recover_bmi(mtb_bmi270_data_t *data)
{
  if (NULL == data)
  {
    return false;
  }

  s_ctx.bmi_ready = false;

  for (uint32_t attempt = 0U; attempt < SENSORHUB_MANAGER_BMI_REINIT_ATTEMPTS; attempt++)
  {
    if (sensorhub_manager_init_bmi() && sensorhub_manager_try_read_bmi(data))
    {
      (void)printf("[CM33] BMI270 recovered after re-init (attempt %lu)\n", (unsigned long)(attempt + 1U));
      return true;
    }
  }

  return false;
}

static void sensorhub_manager_disable_bmi_fallback(void)
{
  s_ctx.bmi_ready = false;
  s_ctx.sensors_mask &= (uint8_t)(~IPC_SENSORHUB_SENSOR_MASK_BMI270);
  s_ctx.running = (s_ctx.sensors_mask != 0U);
  (void)printf("[CM33] BMI270 disabled after repeated failures, continue with POT only\n");
  sensorhub_manager_emit_status(IPC_SENSORHUB_REASON_ERROR);
}

static void sensorhub_manager_emit_bmi_sample(uint32_t now_ms)
{
  ipc_sensorhub_sample_t sample;
  mtb_bmi270_data_t data;
  float ax;
  float ay;
  float az;
  float gx;
  float gy;
  float gz;
  int16_t raw_ax;
  int16_t raw_ay;
  int16_t raw_az;
  int16_t raw_gx;
  int16_t raw_gy;
  int16_t raw_gz;
  uint8_t bit_width = 16U;

  if (0U == (s_ctx.sensors_mask & IPC_SENSORHUB_SENSOR_MASK_BMI270))
  {
    return;
  }

  if ((!s_ctx.bmi_ready) && (!sensorhub_manager_init_bmi()))
  {
    s_ctx.bmi_fail_streak++;
    if (s_ctx.bmi_fail_streak >= SENSORHUB_MANAGER_BMI_FAIL_STREAK_DISABLE)
    {
      sensorhub_manager_disable_bmi_fallback();
    }
    return;
  }

  if (!sensorhub_manager_try_read_bmi(&data))
  {
    if (!sensorhub_manager_recover_bmi(&data))
    {
      s_ctx.bmi_fail_streak++;
      if (s_ctx.bmi_fail_streak >= SENSORHUB_MANAGER_BMI_FAIL_STREAK_DISABLE)
      {
        sensorhub_manager_disable_bmi_fallback();
      }
      else
      {
        sensorhub_manager_emit_status(IPC_SENSORHUB_REASON_ERROR);
      }
      return;
    }
  }

  s_ctx.bmi_fail_streak = 0U;
  if (s_ctx.bmi_warmup_samples > 0U)
  {
    s_ctx.bmi_warmup_samples--;
    return;
  }
  raw_ax = data.sensor_data.acc.x;
  raw_ay = data.sensor_data.acc.y;
  raw_az = data.sensor_data.acc.z;
  raw_gx = data.sensor_data.gyr.x;
  raw_gy = data.sensor_data.gyr.y;
  raw_gz = data.sensor_data.gyr.z;

  if ((0 == raw_ax) && (0 == raw_ay) && (0 == raw_az) && (0 == raw_gx) && (0 == raw_gy) && (0 == raw_gz))
  {
    s_ctx.bmi_zero_data_streak++;
    if (s_ctx.bmi_zero_data_streak >= SENSORHUB_MANAGER_BMI_ZERO_STREAK_RECONFIG)
    {
      cy_rslt_t cfg_rslt = mtb_bmi270_config_default(&s_ctx.bmi);
      (void)printf("[CM33] BMI270 stuck-zero detected, re-config rslt=0x%08lx\n", (unsigned long)cfg_rslt);
      s_ctx.bmi_zero_data_streak = 0U;
      if (CY_RSLT_SUCCESS != cfg_rslt)
      {
        s_ctx.bmi_fail_streak++;
        if (s_ctx.bmi_fail_streak >= SENSORHUB_MANAGER_BMI_FAIL_STREAK_DISABLE)
        {
          sensorhub_manager_disable_bmi_fallback();
        }
        else
        {
          sensorhub_manager_emit_status(IPC_SENSORHUB_REASON_ERROR);
        }
      }
    }
    return;
  }
  s_ctx.bmi_zero_data_streak = 0U;

  if ((s_ctx.bmi.sensor.resolution >= 8U) && (s_ctx.bmi.sensor.resolution <= 24U))
  {
    bit_width = s_ctx.bmi.sensor.resolution;
  }

  ax = sensorhub_manager_bmi_lsb_to_mps2(raw_ax, (int8_t)SENSORHUB_MANAGER_BMI_G_RANGE, bit_width);
  ay = sensorhub_manager_bmi_lsb_to_mps2(raw_ay, (int8_t)SENSORHUB_MANAGER_BMI_G_RANGE, bit_width);
  az = sensorhub_manager_bmi_lsb_to_mps2(raw_az, (int8_t)SENSORHUB_MANAGER_BMI_G_RANGE, bit_width);
  gx = sensorhub_manager_bmi_lsb_to_rps(raw_gx, SENSORHUB_MANAGER_BMI_GYRO_DPS, bit_width);
  gy = sensorhub_manager_bmi_lsb_to_rps(raw_gy, SENSORHUB_MANAGER_BMI_GYRO_DPS, bit_width);
  gz = sensorhub_manager_bmi_lsb_to_rps(raw_gz, SENSORHUB_MANAGER_BMI_GYRO_DPS, bit_width);

  (void)memset(&sample, 0, sizeof(sample));
  sample.sensor_type = (uint8_t)IPC_SENSORHUB_SENSOR_BMI270;
  sample.sequence = s_ctx.sequence++;
  sample.timestamp_ms = now_ms;

  sample.data.bmi270.ax = ax;
  sample.data.bmi270.ay = ay;
  sample.data.bmi270.az = az;
  sample.data.bmi270.gx = gx;
  sample.data.bmi270.gy = gy;
  sample.data.bmi270.gz = gz;
  sample.data.bmi270.accel_mag = sqrtf((ax * ax) + (ay * ay) + (az * az));
  sample.data.bmi270.gyro_mag = sqrtf((gx * gx) + (gy * gy) + (gz * gz));
  sample.data.bmi270.orient = sensorhub_manager_calc_orient(ax, ay, az);

  sensorhub_manager_emit(SENSORHUB_MANAGER_EVENT_SAMPLE, &sample, 1U);
}

static void sensorhub_manager_emit_bmm350_sample(uint32_t now_ms)
{
  ipc_sensorhub_sample_t sample;
  mtb_bmm350_data_t mag_data;
  cy_rslt_t rslt;
  float mx;
  float my;
  float mz;

  if (0U == (s_ctx.sensors_mask & IPC_SENSORHUB_SENSOR_MASK_BMM350))
  {
    return;
  }

  if ((!s_ctx.bmm_ready) && (!sensorhub_manager_init_bmm()))
  {
    s_ctx.bmm_fail_streak++;
    if (s_ctx.bmm_fail_streak >= SENSORHUB_MANAGER_BMM_FAIL_STREAK_DISABLE)
    {
      sensorhub_manager_disable_bmm_fallback();
    }
    return;
  }

  rslt = mtb_bmm350_read(&s_ctx.bmm, &mag_data);
  if (CY_RSLT_SUCCESS != rslt)
  {
    s_ctx.bmm_fail_streak++;
    if (s_ctx.bmm_fail_streak >= SENSORHUB_MANAGER_BMM_FAIL_STREAK_DISABLE)
    {
      sensorhub_manager_disable_bmm_fallback();
    }
    else
    {
      sensorhub_manager_emit_status(IPC_SENSORHUB_REASON_ERROR);
    }
    return;
  }

  s_ctx.bmm_fail_streak = 0U;
  mx = mag_data.sensor_data.x;
  my = mag_data.sensor_data.y;
  mz = mag_data.sensor_data.z;

  (void)memset(&sample, 0, sizeof(sample));
  sample.sensor_type = (uint8_t)IPC_SENSORHUB_SENSOR_BMM350;
  sample.sequence = s_ctx.sequence++;
  sample.timestamp_ms = now_ms;
  sample.data.bmm350.mx = mx;
  sample.data.bmm350.my = my;
  sample.data.bmm350.mz = mz;
  sample.data.bmm350.mag = sqrtf((mx * mx) + (my * my) + (mz * mz));
  sample.data.bmm350.temperature = mag_data.sensor_data.temperature;

  sensorhub_manager_emit(SENSORHUB_MANAGER_EVENT_SAMPLE, &sample, 1U);
}

static void sensorhub_manager_publish_cycle(void)
{
  uint32_t now_ms = (uint32_t)xTaskGetTickCount() * (uint32_t)portTICK_PERIOD_MS;
  bool slow_cycle_due = false;

  s_ctx.fast_tick_accum_ms = (uint16_t)(s_ctx.fast_tick_accum_ms + SENSORHUB_MANAGER_FAST_TICK_MS);
  if (s_ctx.fast_tick_accum_ms >= s_ctx.period_ms)
  {
    slow_cycle_due = true;
    s_ctx.fast_tick_accum_ms = 0U;
  }

  if (slow_cycle_due && (0U != (s_ctx.sensors_mask & IPC_SENSORHUB_SENSOR_MASK_POT)))
  {
    sensorhub_manager_emit_pot_sample(now_ms);
  }
  if (slow_cycle_due && (0U != (s_ctx.sensors_mask & IPC_SENSORHUB_SENSOR_MASK_BMI270)))
  {
    sensorhub_manager_emit_bmi_sample(now_ms);
  }
  if (slow_cycle_due && (0U != (s_ctx.sensors_mask & IPC_SENSORHUB_SENSOR_MASK_BMM350)))
  {
    sensorhub_manager_emit_bmm350_sample(now_ms);
  }
  sensorhub_manager_emit_capsense_sample(now_ms);
  sensorhub_manager_emit_touch_sample(now_ms);
}

static uint16_t sensorhub_manager_clamp_period(uint16_t period_ms)
{
  if (period_ms < SENSORHUB_MANAGER_MIN_PERIOD_MS)
  {
    return SENSORHUB_MANAGER_MIN_PERIOD_MS;
  }
  if (period_ms > SENSORHUB_MANAGER_MAX_PERIOD_MS)
  {
    return SENSORHUB_MANAGER_MAX_PERIOD_MS;
  }
  return period_ms;
}

static void sensorhub_manager_handle_start(const ipc_sensorhub_start_request_t *request)
{
  uint8_t mask = SENSORHUB_MANAGER_DEFAULT_MASK;
  uint8_t actual_mask = 0U;
  uint16_t period_ms = SENSORHUB_MANAGER_DEFAULT_PERIOD_MS;

  if (NULL != request)
  {
    if (0U != request->sensors_mask)
    {
      mask = request->sensors_mask;
    }
    if (0U != request->period_ms)
    {
      period_ms = request->period_ms;
    }
  }

  if ((0U != (mask & IPC_SENSORHUB_SENSOR_MASK_POT)) && sensorhub_manager_init_pot())
  {
    actual_mask |= IPC_SENSORHUB_SENSOR_MASK_POT;
  }

  if (0U != (mask & IPC_SENSORHUB_SENSOR_MASK_BMI270))
  {
    if (sensorhub_manager_init_bmi())
    {
      actual_mask |= IPC_SENSORHUB_SENSOR_MASK_BMI270;
    }
    else
    {
      (void)printf("[CM33] BMI270 unavailable, continue with POT only\n");
    }
  }

  if (0U != (mask & IPC_SENSORHUB_SENSOR_MASK_BMM350))
  {
    if (sensorhub_manager_init_bmm())
    {
      actual_mask |= IPC_SENSORHUB_SENSOR_MASK_BMM350;
    }
    else
    {
      (void)printf("[CM33] BMM350 unavailable\n");
    }
  }

  if (0U != (mask & IPC_SENSORHUB_SENSOR_MASK_CAPSENSE))
  {
    if (sensorhub_manager_init_capsense())
    {
      actual_mask |= IPC_SENSORHUB_SENSOR_MASK_CAPSENSE;
    }
    else
    {
      (void)printf("[CM33] CAPSENSE unavailable\n");
    }
  }

  if (0U != (mask & IPC_SENSORHUB_SENSOR_MASK_TOUCH))
  {
    if (sensorhub_manager_init_touch())
    {
      actual_mask |= IPC_SENSORHUB_SENSOR_MASK_TOUCH;
    }
    else
    {
      (void)printf("[CM33] TOUCH unavailable\n");
    }
  }

  s_ctx.sensors_mask = actual_mask;
  s_ctx.period_ms = sensorhub_manager_clamp_period(period_ms);
  s_ctx.bmi_fail_streak = 0U;
  s_ctx.bmi_zero_data_streak = 0U;
  s_ctx.bmi_warmup_samples =
      (0U != (s_ctx.sensors_mask & IPC_SENSORHUB_SENSOR_MASK_BMI270)) ? SENSORHUB_MANAGER_BMI_WARMUP_SAMPLES : 0U;
  s_ctx.bmm_fail_streak = 0U;
  s_ctx.fast_tick_accum_ms = s_ctx.period_ms;
  s_ctx.capsense_elapsed_ms_since_heartbeat = 0U;
  s_ctx.touch_elapsed_ms_since_heartbeat = 0U;
  s_ctx.running = (s_ctx.sensors_mask != 0U);
  (void)printf("[CM33] SensorHub start mask=0x%02X period=%u ms\n", (unsigned int)s_ctx.sensors_mask,
               (unsigned int)s_ctx.period_ms);
  if (s_ctx.running)
  {
    sensorhub_manager_emit_status(IPC_SENSORHUB_REASON_STARTED);
    sensorhub_manager_publish_cycle();
  }
  else
  {
    sensorhub_manager_emit_status(IPC_SENSORHUB_REASON_ERROR);
  }
}

static void sensorhub_manager_handle_stop(void)
{
  s_ctx.running = false;
  (void)printf("[CM33] SensorHub stopped\n");
  sensorhub_manager_emit_status(IPC_SENSORHUB_REASON_STOPPED);
}

static void sensorhub_manager_task(void *arg)
{
  sensorhub_manager_msg_t msg;

  (void)arg;

  while (true)
  {
    uint32_t wait_ms = s_ctx.running ? SENSORHUB_MANAGER_FAST_TICK_MS : SENSORHUB_MANAGER_IDLE_POLL_MS;
    if (xQueueReceive(s_ctx.queue, &msg, pdMS_TO_TICKS(wait_ms)) == pdPASS)
    {
      switch (msg.cmd)
      {
      case SENSORHUB_MANAGER_CMD_START:
        sensorhub_manager_handle_start(&msg.request);
        break;
      case SENSORHUB_MANAGER_CMD_STOP:
        sensorhub_manager_handle_stop();
        break;
      case SENSORHUB_MANAGER_CMD_STATUS:
        sensorhub_manager_emit_status(IPC_SENSORHUB_REASON_NONE);
        break;
      default:
        break;
      }
    }
    else if (s_ctx.running)
    {
      sensorhub_manager_publish_cycle();
    }
  }
}

bool sensorhub_manager_init(void)
{
  (void)memset(&s_ctx, 0, sizeof(s_ctx));
  s_ctx.sensors_mask = SENSORHUB_MANAGER_DEFAULT_MASK;
  s_ctx.period_ms = SENSORHUB_MANAGER_DEFAULT_PERIOD_MS;
  return true;
}

bool sensorhub_manager_start(void)
{
  if (s_ctx.started)
  {
    return true;
  }

  s_ctx.queue = xQueueCreate(SENSORHUB_MANAGER_QUEUE_LEN, sizeof(sensorhub_manager_msg_t));
  if (NULL == s_ctx.queue)
  {
    return false;
  }

  if (xTaskCreate(sensorhub_manager_task, "sensorhub_mgr", SENSORHUB_MANAGER_TASK_STACK, NULL, SENSORHUB_MANAGER_TASK_PRIO,
                  &s_ctx.task) != pdPASS)
  {
    return false;
  }

  s_ctx.started = true;
  return true;
}

bool sensorhub_manager_set_event_callback(sensorhub_manager_event_cb_t callback, void *user_data)
{
  s_ctx.callback = callback;
  s_ctx.callback_user_data = user_data;
  return true;
}

bool sensorhub_manager_request_start(const ipc_sensorhub_start_request_t *request)
{
  sensorhub_manager_msg_t msg;

  if (NULL == s_ctx.queue)
  {
    return false;
  }

  (void)memset(&msg, 0, sizeof(msg));
  msg.cmd = SENSORHUB_MANAGER_CMD_START;
  if (NULL != request)
  {
    msg.request = *request;
  }
  return (pdPASS == xQueueSend(s_ctx.queue, &msg, 0U));
}

bool sensorhub_manager_request_stop(void)
{
  sensorhub_manager_msg_t msg;

  if (NULL == s_ctx.queue)
  {
    return false;
  }

  (void)memset(&msg, 0, sizeof(msg));
  msg.cmd = SENSORHUB_MANAGER_CMD_STOP;
  return (pdPASS == xQueueSend(s_ctx.queue, &msg, 0U));
}

bool sensorhub_manager_request_status(void)
{
  sensorhub_manager_msg_t msg;

  if (NULL == s_ctx.queue)
  {
    return false;
  }

  (void)memset(&msg, 0, sizeof(msg));
  msg.cmd = SENSORHUB_MANAGER_CMD_STATUS;
  return (pdPASS == xQueueSend(s_ctx.queue, &msg, 0U));
}
