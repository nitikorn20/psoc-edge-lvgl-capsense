#include "sensorhub_v1_dashboard_component.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const char *state_text(uint8_t state)
{
  if ((uint8_t)IPC_SENSORHUB_STATE_STOPPED == state)
  {
    return "Stopped";
  }
  if ((uint8_t)IPC_SENSORHUB_STATE_RUNNING == state)
  {
    return "Running";
  }
  if ((uint8_t)IPC_SENSORHUB_STATE_ERROR == state)
  {
    return "Error";
  }
  return "Unknown";
}

static void mask_text(uint8_t mask, char *buf, uint32_t buf_size)
{
  uint32_t pos = 0U;

  if ((NULL == buf) || (0U == buf_size))
  {
    return;
  }

  buf[0] = '\0';
  if (0U == mask)
  {
    (void)snprintf(buf, buf_size, "None");
    return;
  }

  if (0U != (mask & IPC_SENSORHUB_SENSOR_MASK_POT))
  {
    pos += (uint32_t)snprintf(&buf[pos], (buf_size > pos) ? (buf_size - pos) : 0U, "POT");
  }
  if (0U != (mask & IPC_SENSORHUB_SENSOR_MASK_BMI270))
  {
    pos += (uint32_t)snprintf(&buf[pos], (buf_size > pos) ? (buf_size - pos) : 0U, "%sBMI", (pos > 0U) ? "+" : "");
  }
  if (0U != (mask & IPC_SENSORHUB_SENSOR_MASK_CAPSENSE))
  {
    pos += (uint32_t)snprintf(&buf[pos], (buf_size > pos) ? (buf_size - pos) : 0U, "%sCAP", (pos > 0U) ? "+" : "");
  }
  if (0U != (mask & IPC_SENSORHUB_SENSOR_MASK_TOUCH))
  {
    pos += (uint32_t)snprintf(&buf[pos], (buf_size > pos) ? (buf_size - pos) : 0U, "%sTOUCH", (pos > 0U) ? "+" : "");
  }
  if (0U != (mask & IPC_SENSORHUB_SENSOR_MASK_BMM350))
  {
    (void)snprintf(&buf[pos], (buf_size > pos) ? (buf_size - pos) : 0U, "%sBMM", (pos > 0U) ? "+" : "");
  }
}

static uint16_t heading_compass_deg(const ipc_sensorhub_bmm350_t *bmm)
{
  float heading_math;
  int32_t heading_compass;

  if (NULL == bmm)
  {
    return 0U;
  }

  heading_math = atan2f(bmm->my, bmm->mx) * (180.0f / 3.1415926f);
  if (heading_math < 0.0f)
  {
    heading_math += 360.0f;
  }

  heading_compass = (int32_t)(90.0f - heading_math);
  while (heading_compass < 0)
  {
    heading_compass += 360;
  }
  while (heading_compass >= 360)
  {
    heading_compass -= 360;
  }
  return (uint16_t)heading_compass;
}

void sensorhub_v1_dashboard_component_create(sensorhub_v1_dashboard_component_t *comp, lv_obj_t *parent)
{
  lv_obj_t *debug_title;

  if ((NULL == comp) || (NULL == parent))
  {
    return;
  }

  (void)memset(comp, 0, sizeof(*comp));

  lv_obj_set_style_pad_all(parent, 10, 0);
  lv_obj_set_style_pad_row(parent, 10, 0);
  lv_obj_set_layout(parent, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);

  comp->summary_label = lv_label_create(parent);
  lv_obj_set_width(comp->summary_label, LV_PCT(100));
  lv_label_set_long_mode(comp->summary_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(comp->summary_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(comp->summary_label, lv_color_hex(0x334E68), 0);

  debug_title = lv_label_create(parent);
  lv_label_set_text(debug_title, "IPC Debug Log");
  lv_obj_set_style_text_font(debug_title, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(debug_title, lv_color_hex(0x102A43), 0);

  comp->debug_ta = lv_textarea_create(parent);
  lv_obj_set_width(comp->debug_ta, LV_PCT(100));
  lv_obj_set_flex_grow(comp->debug_ta, 1);
  lv_textarea_set_one_line(comp->debug_ta, false);
  lv_textarea_set_cursor_click_pos(comp->debug_ta, false);
  lv_obj_set_scrollbar_mode(comp->debug_ta, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_style_bg_color(comp->debug_ta, lv_color_hex(0xF7FAFE), 0);
  lv_obj_set_style_border_width(comp->debug_ta, 0, 0);
  lv_obj_set_style_text_color(comp->debug_ta, lv_color_hex(0x334E68), 0);
  lv_obj_set_style_text_font(comp->debug_ta, &lv_font_montserrat_14, 0);
  lv_textarea_set_text(comp->debug_ta, "[INIT] SensorHub log is empty");
}

void sensorhub_v1_dashboard_component_render(sensorhub_v1_dashboard_component_t *comp, const sensorhub_v1_snapshot_t *snapshot)
{
  char mask_buf[32];
  char text[768];
  uint32_t pos = 0U;

  if ((NULL == comp) || (NULL == snapshot) || (NULL == comp->summary_label) || (NULL == comp->debug_ta))
  {
    return;
  }

  mask_text(snapshot->status.sensors_mask, mask_buf, sizeof(mask_buf));
  pos += (uint32_t)snprintf(&text[pos], sizeof(text) - pos,
                            "State: %s | Mask: %s | Reason: %u\n",
                            state_text(snapshot->status.state),
                            mask_buf,
                            (unsigned int)snapshot->status.reason);

  if (snapshot->has_pot)
  {
    pos += (uint32_t)snprintf(&text[pos], sizeof(text) - pos,
                              "POT: %d mV (%.1f%%)\n",
                              snapshot->pot_sample.data.pot.mv,
                              (double)snapshot->pot_sample.data.pot.pct_x10 / 10.0);
  }
  else
  {
    pos += (uint32_t)snprintf(&text[pos], sizeof(text) - pos, "POT: waiting\n");
  }

  if (snapshot->has_bmi270)
  {
    const ipc_sensorhub_bmi270_t *bmi = &snapshot->bmi270_sample.data.bmi270;
    pos += (uint32_t)snprintf(&text[pos], sizeof(text) - pos,
                              "BMI: |A|=%.2f |G|=%.2f O=%u\n",
                              (double)bmi->accel_mag,
                              (double)bmi->gyro_mag,
                              (unsigned int)bmi->orient);
  }
  else
  {
    pos += (uint32_t)snprintf(&text[pos], sizeof(text) - pos, "BMI: waiting\n");
  }

  if (snapshot->has_capsense)
  {
    pos += (uint32_t)snprintf(&text[pos], sizeof(text) - pos,
                              "CAP: b0=%u b1=%u slider=%u%%\n",
                              (unsigned int)snapshot->capsense_sample.data.capsense.btn0_pressed,
                              (unsigned int)snapshot->capsense_sample.data.capsense.btn1_pressed,
                              (unsigned int)snapshot->capsense_sample.data.capsense.slider);
  }
  else
  {
    pos += (uint32_t)snprintf(&text[pos], sizeof(text) - pos, "CAP: waiting\n");
  }

  if (snapshot->has_touch)
  {
    pos += (uint32_t)snprintf(&text[pos], sizeof(text) - pos,
                              "TOUCH: p=%u pt=%u (%u,%u)\n",
                              (unsigned int)snapshot->touch_sample.data.touch.pressed,
                              (unsigned int)snapshot->touch_sample.data.touch.points,
                              (unsigned int)snapshot->touch_sample.data.touch.x,
                              (unsigned int)snapshot->touch_sample.data.touch.y);
  }
  else
  {
    pos += (uint32_t)snprintf(&text[pos], sizeof(text) - pos, "TOUCH: waiting\n");
  }

  if (snapshot->has_bmm350)
  {
    const ipc_sensorhub_bmm350_t *bmm = &snapshot->bmm350_sample.data.bmm350;
    pos += (uint32_t)snprintf(&text[pos], sizeof(text) - pos,
                              "BMM: |B|=%.1f uT Head=%u deg T=%.1fC\n",
                              (double)bmm->mag,
                              (unsigned int)heading_compass_deg(bmm),
                              (double)bmm->temperature);
  }
  else
  {
    pos += (uint32_t)snprintf(&text[pos], sizeof(text) - pos, "BMM: waiting\n");
  }

  lv_label_set_text(comp->summary_label, text);

  if (snapshot->debug_changed)
  {
    if ('\0' == snapshot->debug_text[0])
    {
      lv_textarea_set_text(comp->debug_ta, "[INIT] SensorHub log is empty");
    }
    else
    {
      lv_textarea_set_text(comp->debug_ta, snapshot->debug_text);
    }
    lv_textarea_set_cursor_pos(comp->debug_ta, LV_TEXTAREA_CURSOR_LAST);
  }
}