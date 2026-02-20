#ifndef SENSORHUB_V1_POT_COMPONENT_H
#define SENSORHUB_V1_POT_COMPONENT_H

#include "lvgl.h"
#include "../../sensorhub_v1_types.h"

typedef struct
{
  lv_obj_t *arc;
  lv_obj_t *pct_label;
  lv_obj_t *info_label;
} sensorhub_v1_pot_component_t;

void sensorhub_v1_pot_component_create(sensorhub_v1_pot_component_t *comp, lv_obj_t *parent);
void sensorhub_v1_pot_component_render(sensorhub_v1_pot_component_t *comp, const sensorhub_v1_snapshot_t *snapshot);

#endif /* SENSORHUB_V1_POT_COMPONENT_H */