#include "sensorhub_v1_bmi270_component.h"

#include <stdio.h>
#include <string.h>

#define SENSORHUB_V1_BMI_CHART_POINTS (36U)
#define SENSORHUB_V1_BMI_CHART_RANGE_MAX (220)

static bool sensor_enabled(uint8_t mask, uint8_t sensor_mask)
{
  return (0U != (mask & sensor_mask));
}

static int32_t clamp_i32(int32_t value, int32_t min_val, int32_t max_val)
{
  if (value < min_val)
  {
    return min_val;
  }
  if (value > max_val)
  {
    return max_val;
  }
  return value;
}

void sensorhub_v1_bmi270_component_create(sensorhub_v1_bmi270_component_t *comp, lv_obj_t *parent)
{
  if ((NULL == comp) || (NULL == parent))
  {
    return;
  }

  (void)memset(comp, 0, sizeof(*comp));

  lv_obj_set_style_pad_all(parent, 12, 0);
  lv_obj_set_style_pad_row(parent, 10, 0);
  lv_obj_set_layout(parent, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);

  comp->label = lv_label_create(parent);
  lv_obj_set_width(comp->label, LV_PCT(100));
  lv_label_set_long_mode(comp->label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(comp->label, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(comp->label, lv_color_hex(0x334E68), 0);
  lv_label_set_text(comp->label, "Waiting for BMI270 sample...");

  comp->chart = lv_chart_create(parent);
  lv_obj_set_width(comp->chart, LV_PCT(100));
  lv_obj_set_flex_grow(comp->chart, 1);
  lv_chart_set_type(comp->chart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(comp->chart, SENSORHUB_V1_BMI_CHART_POINTS);
  lv_chart_set_range(comp->chart, LV_CHART_AXIS_PRIMARY_Y, 0, SENSORHUB_V1_BMI_CHART_RANGE_MAX);
  lv_obj_set_style_border_width(comp->chart, 0, 0);
  lv_obj_set_style_bg_color(comp->chart, lv_color_hex(0xF7FAFE), 0);
  lv_obj_set_style_line_width(comp->chart, 2, LV_PART_ITEMS);
  comp->acc_series = lv_chart_add_series(comp->chart, lv_color_hex(0x1565C0), LV_CHART_AXIS_PRIMARY_Y);
  comp->gyr_series = lv_chart_add_series(comp->chart, lv_color_hex(0xD97706), LV_CHART_AXIS_PRIMARY_Y);
}

void sensorhub_v1_bmi270_component_render(sensorhub_v1_bmi270_component_t *comp, const sensorhub_v1_snapshot_t *snapshot)
{
  const ipc_sensorhub_bmi270_t *bmi;
  char text[220];
  int32_t acc_plot;
  int32_t gyr_plot;

  if ((NULL == comp) || (NULL == snapshot) || (NULL == comp->label) || (NULL == comp->chart) || (NULL == comp->acc_series) ||
      (NULL == comp->gyr_series))
  {
    return;
  }

  if (!sensor_enabled(snapshot->status.sensors_mask, IPC_SENSORHUB_SENSOR_MASK_BMI270))
  {
    lv_label_set_text(comp->label, "BMI270 disabled in mask.");
    return;
  }

  if (!snapshot->has_bmi270)
  {
    lv_label_set_text(comp->label, "Waiting for BMI270 sample...");
    return;
  }

  bmi = &snapshot->bmi270_sample.data.bmi270;
  (void)snprintf(text, sizeof(text),
                 "A: %.2f %.2f %.2f m/s2\n"
                 "G: %.2f %.2f %.2f rad/s\n"
                 "|A| %.2f  |G| %.2f  O%u  seq=%u",
                 (double)bmi->ax, (double)bmi->ay, (double)bmi->az,
                 (double)bmi->gx, (double)bmi->gy, (double)bmi->gz,
                 (double)bmi->accel_mag, (double)bmi->gyro_mag,
                 (unsigned int)bmi->orient,
                 (unsigned int)snapshot->bmi270_sample.sequence);
  lv_label_set_text(comp->label, text);

  acc_plot = (int32_t)(bmi->accel_mag * 10.0f);
  gyr_plot = (int32_t)(bmi->gyro_mag * 40.0f);
  acc_plot = clamp_i32(acc_plot, 0, SENSORHUB_V1_BMI_CHART_RANGE_MAX);
  gyr_plot = clamp_i32(gyr_plot, 0, SENSORHUB_V1_BMI_CHART_RANGE_MAX);
  lv_chart_set_next_value(comp->chart, comp->acc_series, acc_plot);
  lv_chart_set_next_value(comp->chart, comp->gyr_series, gyr_plot);
}