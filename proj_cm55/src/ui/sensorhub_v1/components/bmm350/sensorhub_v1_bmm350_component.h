#ifndef SENSORHUB_V1_BMM350_COMPONENT_H
#define SENSORHUB_V1_BMM350_COMPONENT_H

#include "lvgl.h"
#include "../../sensorhub_v1_types.h"

typedef struct
{
  lv_obj_t *compass;
  lv_obj_t *needle;
  lv_obj_t *heading_label;
  lv_obj_t *info_label;
  lv_point_precise_t needle_points[2];
} sensorhub_v1_bmm350_component_t;

void sensorhub_v1_bmm350_component_create(sensorhub_v1_bmm350_component_t *comp, lv_obj_t *parent);
void sensorhub_v1_bmm350_component_render(sensorhub_v1_bmm350_component_t *comp,
                                          const sensorhub_v1_snapshot_t *snapshot);

#endif /* SENSORHUB_V1_BMM350_COMPONENT_H */