#ifndef SENSORHUB_V1_BMI270_COMPONENT_H
#define SENSORHUB_V1_BMI270_COMPONENT_H

#include "lvgl.h"
#include "../../sensorhub_v1_types.h"

typedef struct
{
  lv_obj_t *label;
  lv_obj_t *chart;
  lv_chart_series_t *acc_series;
  lv_chart_series_t *gyr_series;
} sensorhub_v1_bmi270_component_t;

void sensorhub_v1_bmi270_component_create(sensorhub_v1_bmi270_component_t *comp, lv_obj_t *parent);
void sensorhub_v1_bmi270_component_render(sensorhub_v1_bmi270_component_t *comp,
                                          const sensorhub_v1_snapshot_t *snapshot);

#endif /* SENSORHUB_V1_BMI270_COMPONENT_H */