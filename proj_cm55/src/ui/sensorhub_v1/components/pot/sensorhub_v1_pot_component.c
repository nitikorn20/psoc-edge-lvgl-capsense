#include "sensorhub_v1_pot_component.h"

#include <stdio.h>
#include <string.h>

static bool sensor_enabled(uint8_t mask, uint8_t sensor_mask)
{
  return (0U != (mask & sensor_mask));
}

void sensorhub_v1_pot_component_create(sensorhub_v1_pot_component_t *comp, lv_obj_t *parent)
{
  lv_obj_t *wrap;

  if ((NULL == comp) || (NULL == parent))
  {
    return;
  }

  (void)memset(comp, 0, sizeof(*comp));

  lv_obj_set_style_pad_all(parent, 12, 0);
  lv_obj_set_style_pad_row(parent, 8, 0);
  lv_obj_set_layout(parent, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  wrap = lv_obj_create(parent);
  lv_obj_set_size(wrap, 180, 180);
  lv_obj_set_style_bg_opa(wrap, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(wrap, 0, 0);
  lv_obj_set_style_pad_all(wrap, 0, 0);

  comp->arc = lv_arc_create(wrap);
  lv_obj_set_size(comp->arc, 180, 180);
  lv_arc_set_range(comp->arc, 0, 1000);
  lv_arc_set_rotation(comp->arc, 135);
  lv_arc_set_bg_angles(comp->arc, 0, 270);
  lv_arc_set_value(comp->arc, 0);
  lv_obj_remove_style(comp->arc, NULL, LV_PART_KNOB);
  lv_obj_set_style_arc_width(comp->arc, 16, LV_PART_MAIN);
  lv_obj_set_style_arc_width(comp->arc, 16, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(comp->arc, lv_color_hex(0xDCE7F7), LV_PART_MAIN);
  lv_obj_set_style_arc_color(comp->arc, lv_color_hex(0x2E7D32), LV_PART_INDICATOR);
  lv_obj_center(comp->arc);

  comp->pct_label = lv_label_create(wrap);
  lv_label_set_text(comp->pct_label, "--.-%");
  lv_obj_set_style_text_font(comp->pct_label, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(comp->pct_label, lv_color_hex(0x102A43), 0);
  lv_obj_center(comp->pct_label);

  comp->info_label = lv_label_create(parent);
  lv_obj_set_width(comp->info_label, LV_PCT(100));
  lv_label_set_long_mode(comp->info_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(comp->info_label, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(comp->info_label, lv_color_hex(0x334E68), 0);
  lv_obj_set_style_text_align(comp->info_label, LV_TEXT_ALIGN_CENTER, 0);
}

void sensorhub_v1_pot_component_render(sensorhub_v1_pot_component_t *comp, const sensorhub_v1_snapshot_t *snapshot)
{
  char text[96];
  uint16_t pct_x10;
  int16_t mv;

  if ((NULL == comp) || (NULL == snapshot) || (NULL == comp->arc) || (NULL == comp->pct_label) || (NULL == comp->info_label))
  {
    return;
  }

  if (!sensor_enabled(snapshot->status.sensors_mask, IPC_SENSORHUB_SENSOR_MASK_POT))
  {
    lv_arc_set_value(comp->arc, 0);
    lv_label_set_text(comp->pct_label, "--.-%");
    lv_label_set_text(comp->info_label, "POT disabled in mask.");
    return;
  }

  if (!snapshot->has_pot)
  {
    lv_arc_set_value(comp->arc, 0);
    lv_label_set_text(comp->pct_label, "--.-%");
    lv_label_set_text(comp->info_label, "Waiting for POT sample...");
    return;
  }

  pct_x10 = snapshot->pot_sample.data.pot.pct_x10;
  mv = snapshot->pot_sample.data.pot.mv;

  lv_arc_set_value(comp->arc, pct_x10);
  (void)snprintf(text, sizeof(text), "%u.%u%%", (unsigned int)(pct_x10 / 10U), (unsigned int)(pct_x10 % 10U));
  lv_label_set_text(comp->pct_label, text);

  (void)snprintf(text, sizeof(text), "%d mV | seq=%u | %lu ms",
                 mv,
                 (unsigned int)snapshot->pot_sample.sequence,
                 (unsigned long)snapshot->pot_sample.timestamp_ms);
  lv_label_set_text(comp->info_label, text);
}
