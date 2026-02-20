#ifndef SENSORHUB_V1_CAPSENSE_TOUCH_COMPONENT_H
#define SENSORHUB_V1_CAPSENSE_TOUCH_COMPONENT_H

#include "lvgl.h"
#include "../../sensorhub_v1_types.h"

typedef struct
{
  lv_obj_t *btn0_tile;
  lv_obj_t *btn0_icon;
  lv_obj_t *btn0_state;
  lv_obj_t *btn1_tile;
  lv_obj_t *btn1_icon;
  lv_obj_t *btn1_state;
  lv_obj_t *slider_arc;
  lv_obj_t *slider_value;
  lv_obj_t *meta_label;
} sensorhub_v1_capsense_touch_component_t;

void sensorhub_v1_capsense_touch_component_create(sensorhub_v1_capsense_touch_component_t *comp, lv_obj_t *parent);
void sensorhub_v1_capsense_touch_component_render(sensorhub_v1_capsense_touch_component_t *comp,
                                                  const sensorhub_v1_snapshot_t *snapshot);

#endif /* SENSORHUB_V1_CAPSENSE_TOUCH_COMPONENT_H */
